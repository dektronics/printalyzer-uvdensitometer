/*
 * Functions for performing various higher level operations with the
 * light sensor, and data types for interacting with sensor data.
 */
#ifndef SENSOR_H
#define SENSOR_H

#include <stdbool.h>
#include <cmsis_os.h>

#include "stm32l0xx_hal.h"
#include "tsl2585.h"

/**
 * Sensor read light selection.
 */
typedef enum {
    SENSOR_LIGHT_OFF = 0,
    SENSOR_LIGHT_VIS_REFLECTION,
    SENSOR_LIGHT_VIS_TRANSMISSION,
    SENSOR_LIGHT_UV_TRANSMISSION
} sensor_light_t;

#define SENSOR_LIGHT_MAX 128

/**
 * Sensor spectrum measurement mode
 */
typedef enum {
    SENSOR_MODE_DEFAULT = 0,
    SENSOR_MODE_VIS,
    SENSOR_MODE_UV,
    SENSOR_MODE_VIS_DUAL,
    SENSOR_MODE_UV_DUAL
} sensor_mode_t;

typedef enum {
    SENSOR_GAIN_CALIBRATION_STATUS_INIT = 0,
    SENSOR_GAIN_CALIBRATION_STATUS_MEDIUM,
    SENSOR_GAIN_CALIBRATION_STATUS_HIGH,
    SENSOR_GAIN_CALIBRATION_STATUS_MAXIMUM,
    SENSOR_GAIN_CALIBRATION_STATUS_FAILED,
    SENSOR_GAIN_CALIBRATION_STATUS_LED,
    SENSOR_GAIN_CALIBRATION_STATUS_COOLDOWN,
    SENSOR_GAIN_CALIBRATION_STATUS_DONE
} sensor_gain_calibration_status_t;

typedef enum {
    SENSOR_RESULT_INVALID = 0,
    SENSOR_RESULT_VALID,
    SENSOR_RESULT_SATURATED_ANALOG,
    SENSOR_RESULT_SATURATED_DIGITAL
} sensor_result_t;

typedef struct {
    uint32_t als_data;      /*!< Full ALS sensor reading */
    sensor_result_t result; /*!< Sensor result status. */
    tsl2585_gain_t gain;    /*!< Sensor ADC gain */
} sensor_mod_reading_t;

typedef struct {
    sensor_mod_reading_t mod0; /*!< Sensor result from modulator 0 */
    sensor_mod_reading_t mod1; /*!< Sensor result from modulator 1 */
    uint16_t sample_time;   /*!< Sensor integration sample time */
    uint16_t sample_count;  /*!< Sensor integration sample count */
    uint32_t reading_ticks; /*!< Tick time when the integration cycle finished */
    uint32_t elapsed_ticks; /*!< Elapsed ticks since the last sensor reading interrupt */
    uint32_t light_ticks;   /*!< Tick time when the light state last changed */
    uint32_t reading_count; /*!< Number of integration cycles since the sensor was enabled */
} sensor_reading_t;

typedef bool (*sensor_gain_calibration_callback_t)(sensor_gain_calibration_status_t status, int param, void *user_data);
typedef void (*sensor_read_callback_t)(void *user_data);

/**
 * Run the sensor gain calibration process.
 *
 * This function will run the sensor and transmission LED through a series of
 * measurements to determine optimal measurement brightness and the actual
 * gain values that correspond to each gain setting on the sensor.
 * The results will be saved for use in future sensor data calculations.
 *
 * @param callback Callback to monitor progress of the calibration
 * @return osOK on success
 */
osStatus_t sensor_gain_calibration(sensor_gain_calibration_callback_t callback, void *user_data);

#ifdef TEST_LIGHT_CAL
/**
 * Run the sensor light source calibration process.
 *
 * This function will turn on the selected LED and keep the sensor at constant
 * settings. It will then measure the intensity of the light over time, run a
 * logarithmic regression on the results, and save the resulting drop factor.
 *
 * @param light_source Light source to calibrate
 * @return osOK on success
 */
osStatus_t sensor_light_calibration(sensor_light_t light_source);
#endif

/**
 * Perform a target reading with the sensor.
 *
 * This function will turn on the selected LED and take a series of readings,
 * using automatic gain adjustment to arrive at a result in basic counts
 * from which target density can be calculated.
 *
 * @param light_source Light source to use for target measurement
 * @param light_value Light brightness value (Always use `SENSOR_LIGHT_MAX` for normal measurements)
 * @param als_result Sensor result
 * @return osOK on success
 */
osStatus_t sensor_read_target(sensor_light_t light_source, uint8_t light_value,
    float *als_result,
    sensor_read_callback_t callback, void *user_data);

/**
 * Perform a repeatable raw target reading with the sensor.
 *
 * This function will turn on the selected LED and take a series of readings,
 * using the exact sensor settings provided. It will return a result that
 * averages across the readings. This function is intended to be used
 * for repeatable device characterization measurements, where initial
 * conditions are set in advance and data processing happens elsewhere.
 *
 * If the sensor is saturated, then the function will return early
 * with the results set to UINT32_MAX.
 *
 * @param light_source Light source to use for target measurement
 * @param light_value Light brightness value (Always use `SENSOR_LIGHT_MAX` for normal measurements)
 * @param mode Sensor mode
 * @param gain Sensor gain
 * @param sample_time Sensor integration sample time
 * @param sample_count Sensor integration sample count
 * @param als_reading Sensor reading, in raw counts
 * @return osOK on success
 */
osStatus_t sensor_read_target_raw(sensor_light_t light_source, uint8_t light_value,
    sensor_mode_t mode, tsl2585_gain_t gain,
    uint16_t sample_time, uint16_t sample_count,
    uint32_t *als_reading);

/**
 * Convert sensor readings from raw counts to basic counts.
 *
 * Basic counts are normalized based on the sensor gain, integration time,
 * and various system constants. This allows them to be compared across
 * multiple readings and different device settings. All actual light
 * calculations shall be performed in terms of basic counts.
 *
 * @param reading Reading structure with all raw values
 * @param mod The modulator data to convert, 0 or 1.
 * @return Basic count output for the sensor
 */
double sensor_convert_to_basic_counts(const sensor_reading_t *reading, uint8_t mod);

/**
 * Apply the configured zero correction formula to a sensor reading.
 *
 * The input value is in basic counts, as is normally done as part
 * of the measurement process.
 *
 * This is a special case of slope correction that only covers the
 * case where the sensor is directly exposed to the measurement light
 * with no film or paper in the way.
 *
 * If the slope correction values are not correctly configured, then
 * the input will be returned unmodified.
 *
 * @param basic_reading Sensor reading in combined basic counts
 * @return Zero corrected sensor reading
 */
float sensor_apply_zero_correction(float basic_reading);

/**
 * Apply the configured slope correction formula to a sensor reading.
 *
 * The input value is in basic counts, as is normally done as part
 * of the measurement process.
 *
 * If the slope correction values are not correctly configured, then
 * the input will be returned unmodified.
 *
 * @param basic_reading Sensor reading in combined basic counts
 * @return Slope corrected sensor reading
 */
float sensor_apply_slope_correction(float basic_reading);

#endif /* SENSOR_H */
