#include "densitometer.h"

#define LOG_TAG "densitometer"

#include <math.h>
#include <elog.h>
#include <printf.h>

#include "settings.h"
#include "sensor.h"
#include "task_sensor.h"
#include "light.h"
#include "cdc_handler.h"
#include "hid_handler.h"
#include "util.h"

static densitometer_result_t reflection_measure(densitometer_t *densitometer, sensor_read_callback_t callback, void *user_data);
static densitometer_result_t transmission_measure(densitometer_t *densitometer, sensor_read_callback_t callback, void *user_data);

struct __densitometer_t {
    float last_d;
    float zero_d;
    const float max_d;
    const sensor_light_t read_light;
    const densitometer_result_t (*measure_func)(densitometer_t *densitometer, sensor_read_callback_t callback, void *user_data);
};

static densitometer_t vis_reflection_data = {
    .last_d = NAN,
    .zero_d = NAN,
    .max_d = REFLECTION_MAX_D,
    .read_light = SENSOR_LIGHT_VIS_REFLECTION,
    .measure_func = reflection_measure
};

static densitometer_t vis_transmission_data = {
    .last_d = NAN,
    .zero_d = NAN,
    .max_d = TRANSMISSION_MAX_D,
    .read_light = SENSOR_LIGHT_VIS_TRANSMISSION,
    .measure_func = transmission_measure
};

static densitometer_t uv_transmission_data = {
    .last_d = NAN,
    .zero_d = NAN,
    .max_d = TRANSMISSION_MAX_D,
    .read_light = SENSOR_LIGHT_UV_TRANSMISSION,
    .measure_func = transmission_measure
};

static bool densitometer_allow_uncalibrated = false;

void densitometer_set_allow_uncalibrated_measurements(bool allow)
{
    densitometer_allow_uncalibrated = allow;
}

densitometer_t *densitometer_vis_reflection()
{
    return &vis_reflection_data;
}

densitometer_t *densitometer_vis_transmission()
{
    return &vis_transmission_data;
}

densitometer_t *densitometer_uv_transmission()
{
    return &uv_transmission_data;
}

densitometer_result_t densitometer_measure(densitometer_t *densitometer, sensor_read_callback_t callback, void *user_data)
{
    if (!densitometer) { return DENSITOMETER_CAL_ERROR; }

    return densitometer->measure_func(densitometer, callback, user_data);
}

void densitometer_set_idle_light(const densitometer_t *densitometer, bool enabled)
{
    if (!densitometer) { return; }

    if (enabled) {
        uint8_t idle_value = 0;

        /* Copy over latest idle value from settings */
        sensor_light_t idle_light = densitometer->read_light;
        settings_user_idle_light_t idle_light_settings;
        settings_get_user_idle_light(&idle_light_settings);
        if (densitometer->read_light == SENSOR_LIGHT_VIS_REFLECTION) {
            idle_value = idle_light_settings.reflection;
        } else if (densitometer->read_light == SENSOR_LIGHT_VIS_TRANSMISSION) {
            idle_value = idle_light_settings.transmission;
        } else if (densitometer->read_light == SENSOR_LIGHT_UV_TRANSMISSION) {
            idle_value = idle_light_settings.transmission;
            idle_light = SENSOR_LIGHT_VIS_TRANSMISSION;
        }

        sensor_set_light_mode(idle_light, false, idle_value);
    } else {
        sensor_set_light_mode(SENSOR_LIGHT_OFF, false, 0);
    }
}

densitometer_result_t reflection_measure(densitometer_t *densitometer, sensor_read_callback_t callback, void *user_data)
{
    settings_cal_reflection_t cal_reflection;
    bool use_target_cal = true;
    float temp_c;

    /* Get the current calibration values */
    if (!settings_get_cal_vis_reflection(&cal_reflection)) {
        if (densitometer_allow_uncalibrated) {
            use_target_cal = false;
        } else {
            return DENSITOMETER_CAL_ERROR;
        }
    }

    /* Read the current sensor head temperature */
    if (sensor_read_temperature(&temp_c) != osOK) {
        log_w("Temperature sensor read error");
        temp_c = NAN;
    }

    /* Perform sensor read */
    float als_basic_raw;
    if (sensor_read_target(densitometer->read_light, SENSOR_LIGHT_MAX, &als_basic_raw, callback, user_data) != osOK) {
        log_w("Sensor read error");
        densitometer_set_idle_light(densitometer, true);
        return DENSITOMETER_SENSOR_ERROR;
    }

    /* Apply temperature correction to the basic reading */
    float als_basic_temp = sensor_apply_temperature_correction(densitometer->read_light, temp_c, als_basic_raw);

    /* Apply slope correction */
    float corr_value = sensor_apply_slope_correction(als_basic_temp);

    if (use_target_cal) {
        /* Convert all values into log units */
        float meas_ll = log10f(corr_value);
        float cal_hi_ll = log10f(cal_reflection.hi_value);
        float cal_lo_ll = log10f(cal_reflection.lo_value);

        /* Calculate the slope of the line */
        float m = (cal_reflection.hi_d - cal_reflection.lo_d) / (cal_hi_ll - cal_lo_ll);

        /* Calculate the measured density */
        float meas_d = (m * (meas_ll - cal_lo_ll)) + cal_reflection.lo_d;

        log_i("D=%.2f, VALUE=%f,%f(%.1fC),%f", meas_d, als_basic_raw, als_basic_temp, temp_c, corr_value);

        /* Clamp the return value to be within an acceptable range */
        if (meas_d <= 0.0F) { meas_d = 0.0F; }
        else if (meas_d > densitometer->max_d) { meas_d = densitometer->max_d; }

        densitometer->last_d = meas_d;

    } else {
        log_i("D=<uncal>, VALUE=%f,%f(%.1fC),%f", als_basic_raw, als_basic_temp, temp_c, corr_value);

        /* Assign a default reading when missing target calibration */
        densitometer->last_d = 0.0F;
    }

    /* Set light back to idle */
    densitometer_set_idle_light(densitometer, true);

    if (cdc_is_connected()) {
        cdc_send_density_reading('R', densitometer->last_d, densitometer->zero_d, als_basic_temp, corr_value);
    } else {
        hid_send_density_reading('R', densitometer->last_d, densitometer->zero_d);
    }

    return DENSITOMETER_OK;
}

densitometer_result_t transmission_measure(densitometer_t *densitometer, sensor_read_callback_t callback, void *user_data)
{
    settings_cal_transmission_t cal_transmission;
    bool use_target_cal = true;
    char prefix;
    float temp_c;

    /* Get the current calibration values */
    bool result;
    if (densitometer->read_light == SENSOR_LIGHT_UV_TRANSMISSION) {
        result = settings_get_cal_uv_transmission(&cal_transmission);
        prefix = 'U';
    } else {
        result = settings_get_cal_vis_transmission(&cal_transmission);
        prefix = 'T';
    }
    if (!result) {
        if (densitometer_allow_uncalibrated) {
            use_target_cal = false;
        } else {
            return DENSITOMETER_CAL_ERROR;
        }
    }

    /* Read the current sensor head temperature */
    if (sensor_read_temperature(&temp_c) != osOK) {
        log_w("Temperature sensor read error");
        temp_c = NAN;
    }

    /* Perform sensor read */
    float als_basic_raw;
    if (sensor_read_target(densitometer->read_light, SENSOR_LIGHT_MAX, &als_basic_raw, callback, user_data) != osOK) {
        log_w("Sensor read error");
        densitometer_set_idle_light(densitometer, true);
        return DENSITOMETER_SENSOR_ERROR;
    }

    /* Apply temperature correction to the basic reading */
    float als_basic_temp = sensor_apply_temperature_correction(densitometer->read_light, temp_c, als_basic_raw);

    /* Apply slope correction, only to VIS readings */
    float corr_value;
    if (densitometer->read_light == SENSOR_LIGHT_UV_TRANSMISSION) {
        /*
         * Not applying slope calibration to UV readings, due to the lack of an
         * adequate reference for determining appropriate corrections.
         */
        corr_value = als_basic_temp;
    } else {
        corr_value = sensor_apply_slope_correction(als_basic_temp);
    }

    if (use_target_cal) {
        /* Calculate the measured CAL-HI density relative to the zero value */
        float cal_hi_meas_d = -1.0F * log10f(cal_transmission.hi_value / cal_transmission.zero_value);

        /* Calculate the measured target density relative to the zero value */
        float meas_d = -1.0F * log10f(corr_value / cal_transmission.zero_value);

        /* Calculate the adjustment factor */
        float adj_factor = cal_transmission.hi_d / cal_hi_meas_d;

        /* Calculate the calibration corrected density */
        float corr_d = meas_d * adj_factor;

        log_i("D=%.2f, VALUE=%f,%f(%.1fC),%f", corr_d, als_basic_raw, als_basic_temp, temp_c, corr_value);

        /* Clamp the return value to be within an acceptable range */
        if (corr_d <= 0.0F) { corr_d = 0.0F; }
        else if (corr_d > densitometer->max_d) { corr_d = densitometer->max_d; }

        densitometer->last_d = corr_d;

    } else {
        log_i("D=<uncal>, VALUE=%f,%f(%.1fC),%f", als_basic_raw, als_basic_temp, temp_c, corr_value);

        /* Assign a default reading when missing target calibration */
        densitometer->last_d = 0.0F;
    }

    /* Set light back to idle */
    densitometer_set_idle_light(densitometer, true);

    if (cdc_is_connected()) {
        cdc_send_density_reading(prefix, densitometer->last_d, densitometer->zero_d, als_basic_temp, corr_value);
    } else {
        hid_send_density_reading(prefix, densitometer->last_d, densitometer->zero_d);
    }

    return DENSITOMETER_OK;
}

densitometer_result_t densitometer_calibrate(densitometer_t *densitometer, float *cal_value, bool is_zero, sensor_read_callback_t callback, void *user_data)
{
    float temp_c;
    if (!densitometer) { return DENSITOMETER_CAL_ERROR; }

    /* Read the current sensor head temperature */
    if (sensor_read_temperature(&temp_c) != osOK) {
        log_w("Temperature sensor read error");
        temp_c = NAN;
    }

    /* Perform sensor read */
    float als_basic_raw;
    if (sensor_read_target(densitometer->read_light, SENSOR_LIGHT_MAX, &als_basic_raw, callback, user_data) != osOK) {
        log_w("Sensor read error");
        return DENSITOMETER_SENSOR_ERROR;
    }

    /* Apply temperature correction to the basic reading */
    float als_basic_temp = sensor_apply_temperature_correction(densitometer->read_light, temp_c, als_basic_raw);

    /* Apply zero and slope corrections as appropriate */
    float corr_value;
    if (is_zero) {
        if (densitometer->read_light == SENSOR_LIGHT_UV_TRANSMISSION) {
            /* Not currently zero-correcting UV measurements */
            corr_value = als_basic_raw;
        } else {
            corr_value = sensor_apply_zero_correction(als_basic_raw);
        }
    } else {
        if (densitometer->read_light == SENSOR_LIGHT_UV_TRANSMISSION) {
            /* Not currently slope-correcting UV measurements */
            corr_value = als_basic_raw;
        } else {
            corr_value = sensor_apply_slope_correction(als_basic_raw);
        }
    }

    if (als_basic_temp < 0.0001F || corr_value < 0.0001F) {
        return DENSITOMETER_CAL_ERROR;
    }

    /* Assign the calibration value */
    if (cal_value) {
        *cal_value = corr_value;
    }

    return DENSITOMETER_OK;
}

void densitometer_set_zero_d(densitometer_t *densitometer, float d_value)
{
    if (!densitometer) { return; }

    if (!isnanf(d_value) && d_value >= 0.0F && d_value <= densitometer->max_d) {
        densitometer->zero_d = d_value;
    } else {
        densitometer->zero_d = NAN;
    }
}

float densitometer_get_zero_d(const densitometer_t *densitometer)
{
    if (!densitometer) { return NAN; }

    return densitometer->zero_d;
}

float densitometer_get_reading_d(const densitometer_t *densitometer)
{
    if (!densitometer) { return NAN; }

    return densitometer->last_d;
}

float densitometer_get_display_d(const densitometer_t *densitometer)
{
    if (!densitometer) { return NAN; }

    float display_value;
    if (!isnanf(densitometer->zero_d)) {
        display_value = densitometer->last_d - densitometer->zero_d;
    } else {
        display_value = densitometer->last_d;
    }

    /*
     * Clamp the return value to be within an acceptable range, allowing
     * for negative values as an indication to the user that their selected
     * offset might be inappropriate.
     */
    if (display_value > densitometer->max_d) {
        display_value = densitometer->max_d;
    } else if (display_value < (-1.0F * densitometer->max_d)) {
        display_value = -1.0F * densitometer->max_d;
    }

    return display_value;
}

float densitometer_get_display_f(const densitometer_t *densitometer)
{
    float d_value = densitometer_get_display_d(densitometer);
    if (isnanf(d_value)) { return d_value; }

    float f_value = log2f(powf(10.0F, d_value));

    return f_value;
}
