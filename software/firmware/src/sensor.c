#include "sensor.h"

#define LOG_TAG "sensor"
#define LOG_LVL  4
#include <elog.h>

#include <math.h>
#include <limits.h>
#include <cmsis_os.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "stm32l0xx_hal.h"
#include "settings.h"
#include "task_sensor.h"
#include "light.h"
#include "util.h"

#define SENSOR_TARGET_READ_ITERATIONS 2
#define SENSOR_GAIN_CAL_READ_ITERATIONS 5
#define SENSOR_GAIN_LED_CHECK_READ_ITERATIONS 2

/* These constants are for the matte white stage plate */
#define GAIN_CAL_BRIGHTNESS_LOW_MED   128
#define GAIN_CAL_BRIGHTNESS_MED_HIGH  128  /* actual value determined dynamically */
#define GAIN_CAL_BRIGHTNESS_HIGH_MAX  8    /* actual value determined dynamically */

#define LIGHT_CAL_CH0_TARGET_FACTOR   (0.98F)
#define GAIN_CAL_TARGET_FACTOR        (0.8F)

/* Number of iterations to use for light source calibration */
#define LIGHT_CAL_ITERATIONS 600

static osStatus_t sensor_gain_calibration_segment(
    tsl2585_gain_t low_gain, tsl2585_gain_t high_gain,
    float *gain_readings,
    sensor_gain_calibration_callback_t callback, void *user_data);

static osStatus_t sensor_raw_read_loop(uint8_t count, float *als_avg);
static osStatus_t sensor_find_gain_brightness(uint8_t *led_brightness,
    tsl2585_gain_t gain,
    float target_factor,
    sensor_gain_calibration_callback_t callback, void *user_data);
static bool gain_status_callback(
    sensor_gain_calibration_callback_t callback,
    sensor_gain_calibration_status_t status, int param,
    void *user_data);
static uint8_t sensor_get_read_brightness(sensor_light_t light_source);

osStatus_t sensor_gain_calibration(sensor_gain_calibration_callback_t callback, void *user_data)
{
    osStatus_t ret = osOK;

    float gain_readings[12];
    settings_cal_gain_t cal_gain = {0};
    size_t i, j;

    log_i("Starting gain calibration");

    do {
        /* Set lights to initial state */
        ret = sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);
        if (ret != osOK) { break; }

        /* Put the sensor into a known initial state */
        ret = sensor_set_mode(SENSOR_MODE_VIS_DUAL);
        if (ret != osOK) { break; }

        ret = sensor_set_config(TSL2585_GAIN_256X, 719, 99);
        if (ret != osOK) { break; }

        ret = sensor_set_oscillator_calibration(true);
        if (ret != osOK) { break; }

        /* Start the sensor */
        ret = sensor_start();
        if (ret != osOK) { break; }

        /* Measure a segments of gain settings, ordered by LED brightness increase */
        ret = sensor_gain_calibration_segment(TSL2585_GAIN_32X, TSL2585_GAIN_256X, gain_readings + 8,
            callback, user_data);
        if (ret != osOK) { break; }

        ret = sensor_gain_calibration_segment(TSL2585_GAIN_4X, TSL2585_GAIN_32X, gain_readings + 4,
            callback, user_data);
        if (ret != osOK) { break; }

        ret = sensor_gain_calibration_segment(TSL2585_GAIN_0_5X, TSL2585_GAIN_4X, gain_readings,
            callback, user_data);
        if (ret != osOK) { break; }

        /* Calculate the resulting gain values */
        j = 0;
        for (i = 0; i <= TSL2585_GAIN_256X; i++) {
            if (i == 0) {
                cal_gain.values[i] = tsl2585_gain_value(TSL2585_GAIN_0_5X);
            } else {
                cal_gain.values[i] = cal_gain.values[i - 1] * (gain_readings[j] / gain_readings[j - 1]);
            }
            if (((j + 1) % 4) == 0) { j += 2; }
            else { j++; }
        }

        /* Log gain results */
        for (i = 0; i <= TSL2585_GAIN_256X; i++) {
            log_d("%s,%f,%f", tsl2585_gain_str(i), tsl2585_gain_value(i), cal_gain.values[i]);
        }

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

    if (ret == osOK) {
        if (settings_set_cal_gain(&cal_gain)) {
            log_i("Gain calibration saved");
        }
    }

    return ret;
}

osStatus_t sensor_gain_calibration_segment(
    tsl2585_gain_t low_gain, tsl2585_gain_t high_gain,
    float *gain_readings,
    sensor_gain_calibration_callback_t callback, void *user_data)
{
    osStatus_t ret = osOK;
    uint8_t led_brightness;
    float gain_avg;
    size_t i = 0;
    if (low_gain >= high_gain) { return osErrorParameter; }

    ret = sensor_find_gain_brightness(&led_brightness, high_gain,
        GAIN_CAL_TARGET_FACTOR, callback, user_data);
    if (ret != osOK) { return ret; }

    //TODO Update callback to report status in here

    ret = sensor_set_light_mode(SENSOR_LIGHT_VIS_TRANSMISSION, true, led_brightness);
    if (ret != osOK) { return ret; }

    /* Wait for brightness to get to the linear region */
    osDelay(5000);

    /* Set modulator 1 to the reference gain */
    ret = sensor_set_gain(high_gain, TSL2585_MOD1);
    if (ret != osOK) { return ret; }

    for (tsl2585_gain_t gain = low_gain; gain <= high_gain; gain++) {
        /* Short delay between measurements for better LED average grouping */
        osDelay(1000);

        /* Set modulator 0 to the measurement gain */
        ret = sensor_set_gain(gain, TSL2585_MOD0);
        if (ret != osOK) { break; }

        /* Split the delay across the gain change to make sure its fully taken effect */
        osDelay(1000);

        /* Measure the gain setting */
        ret = sensor_raw_read_loop(SENSOR_GAIN_CAL_READ_ITERATIONS, &gain_avg);
        if (ret != osOK) { break; }

        gain_readings[i++] = gain_avg;
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
        ret = sensor_set_light_mode(light_source, /*next_cycle*/true, 128);
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

osStatus_t sensor_read_target(sensor_light_t light_source,
    float *als_result,
    sensor_read_callback_t callback, void *user_data)
{
    osStatus_t ret = osOK;
    uint8_t light_value = 0;
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

    light_value = sensor_get_read_brightness(light_source);

    log_i("Starting sensor target read (light=%d)", light_value);

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

        ret = sensor_set_oscillator_calibration(true);
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

osStatus_t sensor_read_target_raw(sensor_light_t light_source,
    sensor_mode_t mode, tsl2585_gain_t gain,
    uint16_t sample_time, uint16_t sample_count,
    uint32_t *als_reading)
{
    osStatus_t ret = osOK;
    uint8_t light_value = 0;
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

    light_value = sensor_get_read_brightness(light_source);

    log_i("Starting sensor raw target read (light=%d)", light_value);

    do {
        /* Put the sensor into the configured state */
        ret = sensor_set_mode(mode);
        if (ret != osOK) { break; }

        ret = sensor_set_config(gain, sample_time, sample_count);
        if (ret != osOK) { break; }

        ret = sensor_set_agc_disabled();
        if (ret != osOK) { break; }

        ret = sensor_set_oscillator_calibration(true);
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

/**
 * Sensor read loop used for internal calibration purposes.
 *
 * Assumes the sensor is already running and configured, and returns the
 * geometric mean of a series of raw sensor readings.
 * No corrections are performed, so the results from this function should
 * only be compared to results from a similar run under the same conditions.
 *
 * @param count Number of values to average
 * @param als_avg Average raw reading
 */
osStatus_t sensor_raw_read_loop(uint8_t count, float *als_avg)
{
    osStatus_t ret = osOK;
    sensor_reading_t reading;
    double als_sum = 0;
    bool invalid = false;

    if (count == 0) {
        return osErrorParameter;
    }

    /* Loop over measurements */
    for (uint8_t i = 0; i < count; i++) {
        /* Wait for the next reading */
        ret = sensor_get_next_reading(&reading, 2000);
        if (ret != osOK) {
            log_e("sensor_get_next_reading error: %d", ret);
            break;
        }

        /* Accumulate the results */
        log_v("TSL2585[%d]: MOD0=%lu, MOD1=%lu", i, reading.mod0.als_data, reading.mod1.als_data);
        if (reading.mod0.result != SENSOR_RESULT_VALID) {
            log_w("Sensor value indicates invalid MOD0");
            invalid = true;
            break;
        }
        if (reading.mod1.result != SENSOR_RESULT_VALID) {
            log_w("Sensor value indicates invalid MOD1");
            invalid = true;
            break;
        }

        als_sum += logf((float)reading.mod0.als_data / (float)reading.mod1.als_data);
    }

    if (ret == osOK) {
        if (als_avg) {
            *als_avg = invalid ? NAN : expf(als_sum / (float)count);
        }
    } else {
        log_e("Sensor error during read loop: %d", ret);
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

/**
 * Find the ideal LED brightness for measuring gain at a particular gain setting.
 *
 * This routine counts upward and is intended to select a brightness near
 * the bottom of the brightness range without coming too close to saturation.
 *
 * @param gain Measurement gain setting
 * @param led_brightness Brightness to use for further measurements
 * @param target_factor Multiplier to determine how close to saturation is allowed
 */
static osStatus_t sensor_find_gain_brightness(uint8_t *led_brightness,
    tsl2585_gain_t gain,
    float target_factor,
    sensor_gain_calibration_callback_t callback, void *user_data)
{
    osStatus_t ret = osOK;
    uint8_t start_brightness = 0;
    uint8_t end_brightness = 0;
    uint8_t sat_brightness = 0;
    sensor_reading_t reading;
    uint8_t closest_led = 0;

    /* Basic parameter validation */
    if (isnanf(target_factor) || target_factor < 0.1F || target_factor > 1.0F) {
        return osErrorParameter;
    }

    if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_LED, 0, user_data)) { return osError; }

    do {
        /* Enable the light at the starting value */
        sensor_set_light_mode(SENSOR_LIGHT_VIS_TRANSMISSION, false, 128);

        /* Set both modulators to the measurement gain */
        ret = sensor_set_gain(gain, TSL2585_MOD0);
        if (ret != osOK) { break; }
        ret = sensor_set_gain(gain, TSL2585_MOD1);
        if (ret != osOK) { break; }

        /* Wait for the first reading at the new settings to come through */
        ret = sensor_get_next_reading(&reading, 2000);
        if (ret != osOK) { break; }

        /* Start at max brightness and use multiplicative decrease to find a rough saturation threshold */
        for (uint8_t i = 128; i >= 1; i >>= 1) {
            if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_LED, i, user_data)) { return osError; }

            /* Set the LED to target brightness on the next cycle */
            sensor_set_light_mode(SENSOR_LIGHT_VIS_TRANSMISSION, /*next_cycle*/true, i);

            /* Wait for two readings, discarding the first */
            ret = sensor_get_next_reading(&reading, 2000);
            if (ret != osOK) { break; }
            ret = sensor_get_next_reading(&reading, 2000);
            if (ret != osOK) { break; }

            if (reading.mod0.result == SENSOR_RESULT_VALID && reading.mod1.result == SENSOR_RESULT_VALID) {
                if (i == 128) {
                    start_brightness = i;
                    end_brightness = i;
                } else {
                    start_brightness = i;
                    end_brightness = i * 2;
                }
                break;
            } else if (reading.mod0.result != SENSOR_RESULT_SATURATED_ANALOG && reading.mod1.result != SENSOR_RESULT_SATURATED_ANALOG) {
                log_w("Sensor reading error: %d %d", reading.mod0.result, reading.mod1.result);
                ret = osError;
                break;
            }
        }
        if (ret != osOK) { break; }

        /* Check if we're never going to saturate */
        if (start_brightness > 0 && start_brightness == end_brightness) {
            log_d("Does not saturate at max brightness");
            break;
        }

        /* Use additive increase to find the actual saturation point */
        for (uint8_t i = start_brightness; i < end_brightness; i++) {
            if (!gain_status_callback(callback, SENSOR_GAIN_CALIBRATION_STATUS_LED, i, user_data)) { return osError; }

            /* Set the LED to target brightness on the next cycle */
            sensor_set_light_mode(SENSOR_LIGHT_VIS_TRANSMISSION, /*next_cycle*/true, i);

            /* Wait for two readings, discarding the first */
            ret = sensor_get_next_reading(&reading, 2000);
            if (ret != osOK) { break; }
            ret = sensor_get_next_reading(&reading, 2000);
            if (ret != osOK) { break; }

            /* Check for saturation */
            if (reading.mod0.result == SENSOR_RESULT_SATURATED_ANALOG || reading.mod1.result == SENSOR_RESULT_SATURATED_ANALOG) {
                if (i == start_brightness) {
                    log_w("Saturated at start brightness %d, adjust parameters", start_brightness);
                    ret = osError;
                    break;
                } else {
                    log_d("Saturated at brightness: %d", i);
                    sat_brightness = i;
                    break;
                }
            } else if (reading.mod0.result != SENSOR_RESULT_VALID || reading.mod1.result != SENSOR_RESULT_VALID) {
                log_w("Sensor reading error: %d %d", reading.mod0.result, reading.mod1.result);
                ret = osError;
                break;
            }
        }
        if (ret != osOK) { break; }

    } while (0);

    /* Turn off the LED */
    sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);

    if (sat_brightness == 0) {
        closest_led = end_brightness;
    } else {
        /* Select a value that's the saturation brightness scaled by the target factor */
        //XXX closest_led = lroundf((float)sat_brightness * target_factor);
        closest_led = sat_brightness - 1;
    }

    /* Prevent zero */
    if (closest_led < 1) {
        closest_led = 1;
    }

    if (ret == osOK) {
        if (led_brightness) {
            *led_brightness = closest_led;
        }
        log_d("Selected brightness: %d (%d)", closest_led, sat_brightness);
    }

    return ret;
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

    if (isnanf(basic_reading) || isinff(basic_reading) || basic_reading <= 0.0F) {
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

float sensor_apply_slope_correction(float basic_reading)
{
    settings_cal_slope_t cal_slope;

    bool valid = settings_get_cal_slope(&cal_slope);

    if (isnanf(basic_reading) || isinff(basic_reading) || basic_reading <= 0.0F) {
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

uint8_t sensor_get_read_brightness(sensor_light_t light_source)
{
    settings_cal_light_t cal_light = {0};

    if (!settings_get_cal_light(&cal_light)) {
        log_w("Using default light values due to invalid calibration");
    }

    switch (light_source) {
    case SENSOR_LIGHT_VIS_REFLECTION:
        return cal_light.reflection;
    case SENSOR_LIGHT_VIS_TRANSMISSION:
        return cal_light.transmission;
    case SENSOR_LIGHT_OFF:
        return 0;
    default:
        return 128;
    }
}
