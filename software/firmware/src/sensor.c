#include "sensor.h"

#define LOG_TAG "sensor"
#define LOG_LVL  4
#include <elog.h>

#include <math.h>
#include <limits.h>
#include <cmsis_os.h>

#include "settings.h"
#include "task_sensor.h"
#include "light.h"
#include "util.h"

#define SENSOR_TARGET_READ_ITERATIONS 2
#define SENSOR_GAIN_CAL_BRIGHTNESS_THRESHOLD 0.95F
#define SENSOR_GAIN_CAL_READ_ITERATIONS 5
#define SENSOR_GAIN_CAL_LIGHT_LEVELS 5
#define SENSOR_GAIN_CAL_LIGHT_LEVEL_INCREMENT 0.10F
#define SENSOR_GAIN_LED_CHECK_READ_ITERATIONS 2
#define SENSOR_GAIN_LED_COOLDOWN_MS 5000UL

/* Number of iterations to use for light source calibration */
#define LIGHT_CAL_ITERATIONS 600

static osStatus_t sensor_find_gain_brightness(uint16_t led_brightness[static 1],
    tsl2585_gain_t gain,
    sensor_gain_calibration_callback_t callback, void *user_data);
static osStatus_t sensor_measure_gain_pair(
    float gain_ratio[static 1],
    tsl2585_gain_t low_gain, tsl2585_gain_t high_gain,
    uint16_t led_brightness,
    sensor_gain_calibration_callback_t callback, void *user_data);
static bool gain_status_callback(
    sensor_gain_calibration_callback_t callback,
    sensor_gain_calibration_status_t status, int param,
    void *user_data);

osStatus_t sensor_gain_calibration(sensor_gain_calibration_callback_t callback, void *user_data)
{
    osStatus_t ret = osOK;
    uint16_t led_brightness[TSL2585_GAIN_256X + 1] = {0};
    float gain_ratios[TSL2585_GAIN_256X] = {0};
    tsl2585_gain_t base_gain = TSL2585_GAIN_0_5X;
    settings_cal_gain_t cal_gain = {0};
    size_t i;

    log_i("Starting gain calibration");

    do {
        if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_INIT, 0, user_data)) { ret = osError; break; }

        /* Set lights to an initial off state */
        ret = sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);
        if (ret != osOK) { break; }

        /* Change the light frequency for better calibration behavior */
        light_set_frequency(LIGHT_FREQUENCY_HIGH);

        /* Put the sensor into a known initial state */
        ret = sensor_set_mode(SENSOR_MODE_VIS);
        if (ret != osOK) { break; }

        ret = sensor_set_config(TSL2585_GAIN_256X, 719, 199);
        if (ret != osOK) { break; }

        /* Start the sensor */
        ret = sensor_start();
        if (ret != osOK) { break; }

        /* Iterate over each gain setting and find the maximum measurement brightness */
        for (i = TSL2585_GAIN_1X; i <= TSL2585_GAIN_256X; i++) {
            uint16_t measure_brightness;
            ret = sensor_find_gain_brightness(&measure_brightness, i, callback, user_data);
            if (ret != osOK) { break; }
            led_brightness[i] = measure_brightness;
        }
        if (ret != osOK) { break; }
        led_brightness[TSL2585_GAIN_0_5X] = led_brightness[TSL2585_GAIN_1X];

        /* Turn off the sensor */
        sensor_stop();

        /* Set lights to off state */
        ret = sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);
        if (ret != osOK) { break; }

        if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_WAITING, 0, user_data)) { return osError; }

        /*
         * Note: It is possible this routine may fail to find an appropriate
         * measurement brightness for the highest gain settings on certain
         * units. If this happens, then those gain pairs will need to use
         * a different light frequency setting for measurement.
         *
         * The biggest issues with light frequency and gain measurement
         * occur around the middle gain settings, so it should not be a
         * big problem if it needs to be reverted before measuring the
         * highest gain settings.
         *
         * This issue doesn't exist yet on current prototype hardware, so
         * this is just a note for potential future bug fixes.
         */

#if 1
        log_d("Gain measurement brightness values:");
        for (i = TSL2585_GAIN_1X; i <= TSL2585_GAIN_256X; i++) {
            log_d("%s => %d", tsl2585_gain_str(i), led_brightness[i]);
        }
#endif

        /*
         * Calculate the base gain, which is the highest gain we can measure
         * with at full brightness. This is likely to be the gain setting
         * selected for measuring an open sensor.
         */
        for (i = 0; i <= TSL2585_GAIN_256X; i++) {
            if (led_brightness[i] == light_get_max_value()) {
                base_gain = i;
            } else {
                break;
            }
        }
        log_d("Base measurement gain: %s", tsl2585_gain_str(base_gain));

        log_d("Waiting for cooldown before measurement");
        osDelay(SENSOR_GAIN_LED_COOLDOWN_MS * 2);

        if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_WAITING, 0, user_data)) { return osError; }

        /* Iterate over each gain pair and measure the ratio */
        for (i = TSL2585_GAIN_1X; i <= TSL2585_GAIN_256X; i++) {
            ret = sensor_measure_gain_pair(gain_ratios + (i - 1), i - 1, i, led_brightness[i], callback, user_data);
            if (ret != osOK) { break; }

            if (i < TSL2585_GAIN_256X) {
                if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_WAITING, 0, user_data)) { return osError; }
                osDelay(SENSOR_GAIN_LED_COOLDOWN_MS);
            }
        }
        if (ret != osOK) { break; }

#if 1
        log_d("Gain ratios:");
        for (i = 0; i < TSL2585_GAIN_256X; i++) {
            log_d("%s/%s => %f", tsl2585_gain_str(i + 1), tsl2585_gain_str(i), gain_ratios[i]);
        }
#endif

        /* Set the base gain value */
        cal_gain.values[base_gain] = tsl2585_gain_value(base_gain);

        /* Calculate gain values above the base gain */
        for (i = base_gain + 1; i <= TSL2585_GAIN_256X; i++) {
            cal_gain.values[i] = cal_gain.values[i - 1] * gain_ratios[i - 1];
        }

        /* Calculate gain values below the base gain */
        if (base_gain > 0) {
            for (i = base_gain - 1; i < base_gain; --i) {
                cal_gain.values[i] = cal_gain.values[i + 1] / gain_ratios[i];
            }
        }

#if 1
        log_d("Gain values:");
        for (i = 0; i <= TSL2585_GAIN_256X; i++) {
            log_d("%s,%f,%f", tsl2585_gain_str(i), tsl2585_gain_value(i), cal_gain.values[i]);
        }
#endif
    } while (0);

    if (ret == osOK) {
        if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_DONE, 0, user_data)) { ret = osError; }
    } else {
        if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_FAILED, 0, user_data)) { ret = osError; }
    }

    /* Turn off the sensor */
    sensor_stop();

    /* Turn off the lights */
    sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);

    /* Change back to the default light frequency */
    light_set_frequency(LIGHT_FREQUENCY_DEFAULT);

    if (ret == osOK) {
        if (settings_set_cal_gain(&cal_gain)) {
            log_i("Gain calibration saved");
        }
    }

    return ret;
}

osStatus_t sensor_find_gain_brightness(uint16_t led_brightness[static 1],
    tsl2585_gain_t gain,
    sensor_gain_calibration_callback_t callback, void *user_data)
{
    osStatus_t ret = osOK;
    uint16_t cur_brightness = 0;
    uint16_t sat_brightness = 0;
    uint16_t min_brightness = 0;
    sensor_reading_t reading;

    const uint16_t max_brightness = light_get_max_value();

    if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_LED, gain, user_data)) { return osError; }

    do {
        /* Select the measurement gain */
        ret = sensor_set_gain(gain, TSL2585_MOD0);
        if (ret != osOK) { break; }

        /* Wait for the first reading at the new settings to come through */
        ret = sensor_get_next_reading(&reading, 2000);
        if (ret != osOK) { break; }

        /* Begin at the max brightness */
        cur_brightness = max_brightness;

        while (1) {
            /* Set the LED to target brightness on the next cycle */
            log_d("Setting brightness to %d", cur_brightness);
            sensor_set_light_mode(SENSOR_LIGHT_VIS_TRANSMISSION, /*next_cycle*/true, cur_brightness);

            /* Wait for two readings, discarding the first */
            ret = sensor_get_next_reading(&reading, 2000);
            if (ret != osOK) { break; }
            ret = sensor_get_next_reading(&reading, 2000);
            if (ret != osOK) { break; }

            if (reading.mod0.result == SENSOR_RESULT_VALID) {
                /* Valid result */
                if (cur_brightness == max_brightness) {
                    log_d("Does not saturate at max brightness");
                    break;
                } else {
                    /* Sensor not saturated, need to increase brightness */
                    if (cur_brightness + 1 == sat_brightness) {
                        log_d("Found target brightness");
                        break;
                    } else {
                        min_brightness = cur_brightness;
                        if (sat_brightness == 0) {
                            cur_brightness = (cur_brightness + max_brightness) / 2;
                        } else {
                            cur_brightness = (cur_brightness + sat_brightness) / 2;
                        }
                    }
                }
            } else if (reading.mod0.result == SENSOR_RESULT_SATURATED_ANALOG || reading.mod0.result == SENSOR_RESULT_SATURATED_DIGITAL) {
                /* Sensor saturated, need to reduce brightness by half */
                sat_brightness = cur_brightness;
                cur_brightness = min_brightness + ((cur_brightness - min_brightness) / 2);
                if (cur_brightness == sat_brightness) { cur_brightness--; }
            } else {
                log_w("Sensor reading error: %d", reading.mod0.result);
                ret = osError;
                break;
            }
        }
        if (ret != osOK) { break; }
    } while (0);

    /* Turn off the LED */
    sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);

    if (ret == osOK) {
        uint16_t adj_brightness;
        if (cur_brightness == max_brightness) {
            adj_brightness = cur_brightness;
        } else {
            /* Adjust to prevent saturation due to noise */
            adj_brightness = (uint16_t)ceilf((float)cur_brightness * SENSOR_GAIN_CAL_BRIGHTNESS_THRESHOLD);

            /* Prevent near-zero */
            if (adj_brightness < 2) {
                adj_brightness = 2;
            }
        }

        *led_brightness = adj_brightness;
        log_d("Selected brightness: %d (%d)", adj_brightness, sat_brightness);
    }

    return ret;
}

osStatus_t sensor_measure_gain_pair(float gain_ratio[static 1],
    tsl2585_gain_t low_gain, tsl2585_gain_t high_gain,
    uint16_t led_brightness,
    sensor_gain_calibration_callback_t callback, void *user_data)
{
    osStatus_t ret = osOK;
    uint32_t high_gain_reading;
    uint32_t low_gain_reading;

    do {
        log_d("Measuring %s/%s at %d", tsl2585_gain_str(high_gain), tsl2585_gain_str(low_gain), led_brightness);

        if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_GAIN, low_gain, user_data)) { return osError; }

        ret = sensor_read_target_raw(
            SENSOR_LIGHT_VIS_TRANSMISSION, led_brightness, SENSOR_MODE_VIS,
            high_gain, 719, 199,
            &high_gain_reading);
        if (ret != osOK) { break; }

        if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_WAITING, 0, user_data)) { return osError; }
        osDelay(SENSOR_GAIN_LED_COOLDOWN_MS);

        if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_GAIN, high_gain, user_data)) { return osError; }
        ret = sensor_read_target_raw(
            SENSOR_LIGHT_VIS_TRANSMISSION, led_brightness, SENSOR_MODE_VIS,
            low_gain, 719, 199,
            &low_gain_reading);
        if (ret != osOK) { break; }

    } while (0);

    if (ret == osOK) {
        *gain_ratio = (float)high_gain_reading / (float)low_gain_reading;
    }
    return ret;
}

#ifdef TEST_LIGHT_CAL
osStatus_t sensor_light_calibration(sensor_light_t light_source)
{
    osStatus_t ret = osOK;
    sensor_mode_t mode = SENSOR_MODE_DEFAULT;
    tsl2585_gain_t gain = TSL2585_GAIN_128X;
    sensor_reading_t reading;
    uint32_t ticks_start;
    uint16_t light_max = light_get_max_value();

    /*
     * Variables used for regression.
     * This is all being done with doubles so we don't need
     * to worry about sums on sensor reading calculations
     * overflowing.
     */
    const double n_real = (double)(LIGHT_CAL_ITERATIONS);
    double sum_x = 0.0;
    double sum_xx = 0.0;
    double sum_xy = 0.0;
    double sum_y = 0.0;
    double sum_yy = 0.0;
    double denominator = 0.0;
    double slope = 0.0;
    double intercept = 0.0;
    double drop_factor = 0.0;

    /* Parameter validation */
    if (light_source != SENSOR_LIGHT_VIS_REFLECTION
        && light_source != SENSOR_LIGHT_VIS_TRANSMISSION
        && light_source != SENSOR_LIGHT_UV_TRANSMISSION) {
        return osErrorParameter;
    }

    log_i("Starting LED brightness calibration");

    do {
        /* Set lights to initial off state */
        ret = sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);
        if (ret != osOK) { break; }

        /* Rough delay for things to settle */
        osDelay(1000);

        /* Start the sensor */
        switch (light_source) {
        case SENSOR_LIGHT_VIS_REFLECTION:
            mode = SENSOR_MODE_VIS;
            gain = TSL2585_GAIN_32X;
            break;
        case SENSOR_LIGHT_VIS_TRANSMISSION:
            mode = SENSOR_MODE_VIS;
            gain = TSL2585_GAIN_0_5X;
            break;
        case SENSOR_LIGHT_UV_TRANSMISSION:
            mode = SENSOR_MODE_UV;
            gain = TSL2585_GAIN_128X;
            break;
        default:
            break;
        }

        ret = sensor_set_mode(mode);
        if (ret != osOK) { break; }

        /* 200ms integration time */
        ret = sensor_set_config(gain, 719, 199);
        if (ret != osOK) { break; }

        ret = sensor_start();
        if (ret != osOK) { break; }

        /* Swallow the first reading */
        ret = sensor_get_next_reading(&reading, 2000);
        if (ret != osOK) { break; }

        /* Set LED to full brightness at the next cycle */
        ret = sensor_set_light_mode(light_source, /*next_cycle*/true, light_max);
        if (ret != osOK) { break; }

        /* Wait for another cycle which will trigger the LED on */
        ret = sensor_get_next_reading(&reading, 2000);
        if (ret != osOK) { break; }
        log_d("TSL2585[%d]: %ld", reading.reading_count, reading.mod0.als_data);

        ticks_start = reading.reading_ticks;

        /* Iterate over 2 minutes of readings and accumulate regression data */
        log_d("Starting read loop");
        for (int i = 0; i < LIGHT_CAL_ITERATIONS; i++) {
            ret = sensor_get_next_reading(&reading, 1000);
            if (ret != osOK) { break; }

            double x = log((double)(reading.reading_ticks - ticks_start));

            log_d("TSL2585[%d]: %ld", reading.reading_count, reading.mod0.als_data);

            sum_x += x;
            sum_xx += x * x;
            sum_xy += x * (double)reading.mod0.als_data;
            sum_y += (double)reading.mod0.als_data;
            sum_yy += (double)reading.mod0.als_data * (double)reading.mod0.als_data;
        }
        log_d("Finished read loop");
    } while (0);

    /* Turn LED off */
    sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);

    /* Stop the sensor */
    sensor_stop();

    osDelay(500);

    if (ret != osOK) {
        log_e("Light source calibration failed: %d", ret);
        return ret;
    }

    denominator = n_real * sum_xx - sum_x * sum_x;
    if (denominator <= 0.0) {
        log_e("Denominator calculation error: %f", denominator);
        return osError;
    }

    slope = (n_real * sum_xy - sum_x * sum_y) / denominator;
    intercept = (sum_y - slope * sum_x) / n_real;
    drop_factor = slope / intercept;

    /* The drop factor is supposed to be negative */
    if (drop_factor >= 0.0) {
        log_e("Drop factor calculation error: %f", drop_factor);
        return osError;
    }

    log_i("LED calibration run complete");

    log_d("Slope = %f", slope);
    log_d("Intercept = %f", intercept);
    log_d("Drop factor = %f", drop_factor);

    /*
     * Correction formula is:
     * ch_val - (ch_val * (drop_factor * log(elapsed_ticks)))
     */

    return os_to_hal_status(ret);
}
#endif

osStatus_t sensor_read_target(sensor_light_t light_source, uint16_t light_value,
    float *als_result,
    sensor_read_callback_t callback, void *user_data)
{
    osStatus_t ret = osOK;
    sensor_reading_t reading;
    sensor_mode_t sensor_mode;
    int agc_step;
    int invalid_count;
    int reading_count;
    double als_basic = 0;
    double als_sum = 0;
    double als_avg = NAN;

    if (light_source != SENSOR_LIGHT_VIS_REFLECTION
        && light_source != SENSOR_LIGHT_VIS_TRANSMISSION
        && light_source != SENSOR_LIGHT_UV_TRANSMISSION) {
        return osErrorParameter;
    }

    if (light_source == SENSOR_LIGHT_UV_TRANSMISSION) {
        sensor_mode = SENSOR_MODE_UV;
    } else {
        sensor_mode = SENSOR_MODE_VIS;
    }

    log_i("Starting sensor target read");

    do {
        /* Make sure the light is disabled */
        ret = sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);
        if (ret != osOK) { break; }

        osDelay(10);

        /* Configure initial sensor settings */
        ret = sensor_set_mode(sensor_mode);
        if (ret != osOK) { break; }

        ret = sensor_set_config(TSL2585_GAIN_256X, 719, 0);
        if (ret != osOK) { break; }

        ret = sensor_set_agc_enabled(9);
        if (ret != osOK) { break; }

        /* Activate light source synchronized with sensor cycle */
        ret = sensor_set_light_mode(light_source, /*next_cycle*/true, light_value);
        if (ret != osOK) { break; }

        /* Start the sensor */
        ret = sensor_start();
        if (ret != osOK) { break; }

        agc_step = 1;
        invalid_count = 0;
        reading_count = 0;
        do {
            /* Invoke the progress callback */
            if (callback) { callback(user_data); }

            ret = sensor_get_next_reading(&reading, 500);
            if (ret != osOK) { break; }

            /* Make sure the reading is valid */
            if (reading.mod0.result != SENSOR_RESULT_VALID) {
                invalid_count++;
                if (invalid_count > 5) {
                    ret = osErrorTimeout;
                    break;
                } else {
                    continue;
                }
            }

            /* Handle the process of moving from AGC to measurement */
            if (agc_step == 1) {
                /* Disable AGC */
                ret = sensor_set_agc_disabled();
                if (ret != osOK) { break; }
                /* Increase integration time to prevent FIFO overflow while AGC disable takes effect */
                ret = sensor_set_integration(719, 9);
                if (ret != osOK) { break; }
                agc_step++;
                continue;
            } else if (agc_step == 2) {
                /* Set measurement sample time */
                ret = sensor_set_integration(719, 199);
                if (ret != osOK) { break; }
                agc_step = 0;
                continue;
            }

            /* Collect the measurement */
            als_basic = sensor_convert_to_basic_counts(&reading, 0);
            als_sum += als_basic;
            reading_count++;
        } while (reading_count < SENSOR_TARGET_READ_ITERATIONS);
        if (ret != osOK) { break; }

        als_avg = (als_sum / (double)SENSOR_TARGET_READ_ITERATIONS);

        //TODO Detect errors or saturation here

    } while (0);

    /* Turn off the sensor */
    sensor_stop();
    sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);

    if (ret == osOK) {
        log_i("Sensor read complete");
        if (als_result) { *als_result = (float)als_avg; }
    } else {
        log_e("Sensor read failed: ret=%d", ret);
        ret = osError;
    }
    return ret;
}

osStatus_t sensor_read_target_raw(sensor_light_t light_source, uint16_t light_value,
    sensor_mode_t mode, tsl2585_gain_t gain,
    uint16_t sample_time, uint16_t sample_count,
    uint32_t *als_reading)
{
    osStatus_t ret = osOK;
    sensor_reading_t reading;
    double als_sum = 0;
    double als_avg = NAN;
    bool saturated = false;

    if (light_source != SENSOR_LIGHT_OFF
        && light_source != SENSOR_LIGHT_VIS_REFLECTION
        && light_source != SENSOR_LIGHT_VIS_TRANSMISSION
        && light_source != SENSOR_LIGHT_UV_TRANSMISSION) {
        return osErrorParameter;
    }
    if (mode < 0 || mode > SENSOR_MODE_UV) {
        return osErrorParameter;
    }
    if (gain < 0 || gain > TSL2585_GAIN_MAX) {
        return osErrorParameter;
    }
    if (sample_time > 2047 || sample_count > 2047) {
        return osErrorParameter;
    }

    log_i("Starting sensor raw target read");

    do {
        /* Put the sensor into the configured state */
        ret = sensor_set_mode(mode);
        if (ret != osOK) { break; }

        ret = sensor_set_config(gain, sample_time, sample_count);
        if (ret != osOK) { break; }

        ret = sensor_set_agc_disabled();
        if (ret != osOK) { break; }

        /* Activate light source synchronized with sensor cycle */
        ret = sensor_set_light_mode(light_source, /*next_cycle*/true, light_value);
        if (ret != osOK) { break; }

        /* Start the sensor */
        ret = sensor_start();
        if (ret != osOK) { break; }

        /* Take the target measurement readings */
        for (int i = 0; i < SENSOR_TARGET_READ_ITERATIONS; i++) {
            ret = sensor_get_next_reading(&reading, 2000);
            if (ret != osOK) { break; }

            /* Make sure we're consistent with our read cycles */
            if (reading.reading_count != i + 2) {
                log_e("Unexpected read cycle count: %d", reading.reading_count);
                ret = osError;
                break;
            }

            /* Abort if the sensor is saturated */
            if (reading.mod0.result != SENSOR_RESULT_VALID) {
                log_w("Aborting due to sensor saturation");
                saturated = true;
                break;
            }

            /* Accumulate the results */
            als_sum += (double)reading.mod0.als_data;
        }
        if (ret != osOK) { break; }

        als_avg = (als_sum / (double)SENSOR_TARGET_READ_ITERATIONS);
    } while (0);

    /* Turn off the sensor */
    sensor_stop();
    sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);

    if (ret == osOK) {
        log_i("Sensor read complete");
        if (saturated) {
            if (als_reading) { *als_reading = UINT32_MAX; }
        } else {
            if (als_reading) { *als_reading = (uint32_t)lroundf(als_avg); }
        }
    } else {
        log_e("Sensor read failed: ret=%d", ret);
        ret = osError;
    }
    return ret;
}

bool gain_status_callback(
    sensor_gain_calibration_callback_t callback,
    sensor_gain_calibration_status_t status, int param,
    void *user_data)
{
    if (callback) {
        return callback(status, param, user_data);
    } else {
        return true;
    }
}

double sensor_convert_to_basic_counts(const sensor_reading_t *reading, uint8_t mod)
{
    const sensor_mod_reading_t *mod_reading;
    settings_cal_gain_t cal_gain;
    double als_gain;
    double atime_ms;
    double als_reading;

    if (!reading) {
        return NAN;
    }

    if (mod == 0) {
        mod_reading = &reading->mod0;
    } else if (mod == 1) {
        mod_reading = &reading->mod1;
    } else {
        return NAN;
    }

    /* Get the gain value from sensor calibration */
    settings_get_cal_gain(&cal_gain);
    als_gain = settings_get_cal_gain_value(&cal_gain, mod_reading->gain);

    /*
     * Integration time is uncalibrated, due to the assumption that all
     * target measurements will be done at the same setting.
     */
    atime_ms = tsl2585_integration_time_ms(reading->sample_time, reading->sample_count);

    /* Divide to get numbers in a similar range as previous sensors */
    als_reading = (double)mod_reading->als_data / 16.0F;

    return als_reading / (atime_ms * als_gain);
}

float sensor_apply_zero_correction(float basic_reading)
{
    settings_cal_slope_t cal_slope;

    bool valid = settings_get_cal_slope(&cal_slope);

    if (isnan(basic_reading) || isinf(basic_reading) || basic_reading <= 0.0F) {
        log_w("Cannot apply zero correction to invalid reading: %f", basic_reading);
        return basic_reading;
    }

    if (!valid) {
        log_w("Invalid slope calibration values");
        return basic_reading;
    }

    float corr_value = basic_reading * powf(10.0F, cal_slope.z);

    return corr_value;
}

float sensor_apply_temperature_correction(sensor_light_t light_source, float temp_c, float basic_reading)
{
    bool valid;
    settings_cal_temperature_t cal_temperature;
    float b0;
    float b1;
    float b2;
    float temp_corr;

    switch (light_source) {
    case SENSOR_LIGHT_VIS_REFLECTION:
    case SENSOR_LIGHT_VIS_TRANSMISSION:
        valid = settings_get_cal_vis_temperature(&cal_temperature);
        break;
    case SENSOR_LIGHT_UV_TRANSMISSION:
        valid = settings_get_cal_uv_temperature(&cal_temperature);
        break;
    default:
        valid = false;
        break;
    }

    if (!valid) {
        log_w("Invalid temperature calibration values");
        return basic_reading;
    }

    if (!is_valid_number(temp_c)) {
        log_w("Invalid temperature reading");
        return basic_reading;
    }

    /*
     * Calculate the correction coefficients from the calibration values
     * and the basic reading.
     */
    b0 = cal_temperature.b0[0] + (cal_temperature.b0[1] * basic_reading) + (cal_temperature.b0[2] * powf(basic_reading, 2.0F));
    b1 = cal_temperature.b1[0] + (cal_temperature.b1[1] * basic_reading) + (cal_temperature.b1[2] * powf(basic_reading, 2.0F));
    b2 = cal_temperature.b2[0] + (cal_temperature.b2[1] * basic_reading) + (cal_temperature.b2[2] * powf(basic_reading, 2.0F));

    /*
     * Calculate the temperature correction multiplier from the correction
     * coefficients and the temperature reading.
     */
    temp_corr = b0 + (b1 * temp_c) + (b2 * powf(temp_c, 2.0F));

    /* Calculate the final temperature-corrected reading */
    float corr_reading = basic_reading * temp_corr;

    return corr_reading;
}

float sensor_apply_slope_correction(float basic_reading)
{
    settings_cal_slope_t cal_slope;

    bool valid = settings_get_cal_slope(&cal_slope);

    if (isnan(basic_reading) || isinf(basic_reading) || basic_reading <= 0.0F) {
        log_w("Cannot apply slope correction to invalid reading: %f", basic_reading);
        return basic_reading;
    }

    if (!valid) {
        log_w("Invalid slope calibration values");
        return basic_reading;
    }

    float l_reading = log10f(basic_reading);
    float l_expected = cal_slope.b0 + (cal_slope.b1 * l_reading) + (cal_slope.b2 * powf(l_reading, 2.0F));
    float corr_reading = powf(10.0F, l_expected);

    return corr_reading;
}
