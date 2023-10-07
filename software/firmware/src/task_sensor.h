/*
 * Sensor task that runs the TSL2591 light sensor, and controls the LEDs
 * used for making measurements with the sensor.
 */

#ifndef TASK_SENSOR_H
#define TASK_SENSOR_H

#include <cmsis_os.h>

#include "stm32l0xx_hal.h"
#include "tsl2591.h" //XXX
#include "tsl2585.h"
#include "sensor.h"

/**
 * Start the sensor task.
 *
 * @param argument The osSemaphoreId_t used to synchronize task startup.
 */
void task_sensor_run(void *argument);

/**
 * Check if the sensor has been successfully initialized.
 */
bool sensor_is_initialized();

/**
 * Enable the sensor.
 */
osStatus_t sensor_start();

/**
 * Disable the sensor.
 */
osStatus_t sensor_stop();

/**
 * FIXME Remove all uses of this function
 */
osStatus_t sensor_set_config_old(tsl2591_gain_t gain, tsl2591_time_t time);

/**
 * Set the sensor's spectrum measurement mode
 *
 * This configures the photodiodes on the sensor to determine which
 * elements will contribute to the output.
 *
 * @param mode Sensor spectrum selection
 */
osStatus_t sensor_set_mode(sensor_mode_t mode);

/**
 * Set the sensor's gain and integration time
 *
 * The sample time and count are combined to form the integration time,
 * according to the following formula:
 * TIME(μs) = (sample_count + 1) * (sample_time + 1) * 1.388889μs
 *
 * @param gain Sensor ADC gain
 * @param sample_time Duration of each sample in an integration cycle
 * @param sample_count Number of samples in an integration cycle
 */
osStatus_t sensor_set_config(tsl2585_gain_t gain, uint16_t sample_time, uint16_t sample_count);

/**
 * Change the state of the sensor read light sources.
 *
 * Both lights are treated as mutually exclusive and are never turned on at
 * the same time. Turning one on will result in the other being turned off.
 * However, for convenience, it is possible to turn both off by passing
 * SENSOR_LIGHT_OFF as the light selection.
 *
 * @param light Which light to change
 * @param next_cycle Whether to delay the change until the completion of the next integration cycle
 * @param value Value to set when making the change
 */
osStatus_t sensor_set_light_mode(sensor_light_t light, bool next_cycle, uint8_t value);

/**
 * FIXME Remove all uses of this function
 */
osStatus_t sensor_get_next_reading_old(sensor_reading_old_t *reading, uint32_t timeout);

/**
 * Get the next reading from the sensor.
 * If no reading is currently available, then this function will block
 * until the completion of the next sensor integration cycle.
 *
 * @param reading Sensor reading data
 * @param timeout Amount of time to wait for a reading to become available
 * @return osOK on success, osErrorTimeout on timeout
 */
osStatus_t sensor_get_next_reading(sensor_reading_t *reading, uint32_t timeout);

/**
 * Sensor interrupt handler.
 */
void sensor_int_handler();

#endif /* TASK_SENSOR_H */
