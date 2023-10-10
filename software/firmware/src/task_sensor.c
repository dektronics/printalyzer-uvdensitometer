#include "task_sensor.h"

#define LOG_TAG "task_sensor"
#include <elog.h>

#include <math.h>
#include <cmsis_os.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "stm32l0xx_hal.h"
#include "settings.h"
#include "tsl2591.h" //XXX
#include "tsl2585.h"
#include "sensor.h"
#include "light.h"
#include "util.h"
#include "cdc_handler.h"

/**
 * Sensor control event types.
 */
typedef enum {
    SENSOR_CONTROL_STOP = 0,
    SENSOR_CONTROL_START,
    SENSOR_CONTROL_SET_MODE,
    SENSOR_CONTROL_SET_CONFIG,
    SENSOR_CONTROL_SET_LIGHT_MODE,
    SENSOR_CONTROL_INTERRUPT
} sensor_control_event_type_t;

typedef struct {
    tsl2585_gain_t gain;
    uint16_t sample_time;
    uint16_t sample_count;
} sensor_control_config_params_t;

typedef struct {
    sensor_light_t light;
    bool next_cycle;
    uint8_t value;
} sensor_control_light_mode_params_t;

typedef struct {
    uint32_t sensor_ticks;
    uint32_t light_ticks;
    uint32_t reading_count;
} sensor_control_interrupt_params_t;

/**
 * Sensor control event data.
 */
typedef struct {
    sensor_control_event_type_t event_type;
    osStatus_t *result;
    union {
        sensor_mode_t sensor_mode;
        sensor_control_config_params_t config;
        sensor_control_light_mode_params_t light_mode;
        sensor_control_interrupt_params_t interrupt;
    };
} sensor_control_event_t;

/**
 * Configuration state for the TSL2585 light sensor
 */
typedef struct {
    bool running;
    uint8_t uv_calibration;
    sensor_mode_t sensor_mode;
    tsl2585_gain_t gain;
    uint16_t sample_time;
    uint16_t sample_count;
    uint8_t calibration_iteration;
    bool agc_enabled;
    uint16_t agc_sample_count;
    bool mode_pending;
    bool config_pending;
    bool agc_pending;
    bool sai_active;
    bool agc_disabled_reset_gain;
    bool discard_next_reading;
} tsl2585_state_t;

/* Global I2C handle for the sensor */
extern I2C_HandleTypeDef hi2c1;

static volatile bool sensor_initialized = false;
static volatile uint32_t pending_int_light_change = 0;
static volatile uint32_t light_change_ticks = 0;
static volatile uint32_t reading_count = 0;

/* Sensor task state variables */
static tsl2585_state_t sensor_state = {0};
static uint32_t last_aint_ticks = 0;

/* Queue for low level sensor control events */
static osMessageQueueId_t sensor_control_queue = NULL;
static const osMessageQueueAttr_t sensor_control_queue_attrs = {
    .name = "sensor_control_queue"
};

/* Queue to hold the latest sensor reading */
static osMessageQueueId_t sensor_reading_queue = NULL;
static const osMessageQueueAttr_t sensor_reading_queue_attrs = {
    .name = "sensor_reading_queue"
};

/* Semaphore to synchronize sensor control calls */
static osSemaphoreId_t sensor_control_semaphore = NULL;
static const osSemaphoreAttr_t sensor_control_semaphore_attrs = {
    .name = "sensor_control_semaphore"
};

/* Default photodiodes configuration */
static const tsl2585_modulator_t sensor_phd_mod_default[] = {
    TSL2585_MOD1, TSL2585_MOD0, TSL2585_MOD1, TSL2585_MOD0, TSL2585_MOD1, TSL2585_MOD0
};

/* Only enable Photopic photodiodes */
static const tsl2585_modulator_t sensor_phd_mod_vis[] = {
    0, TSL2585_MOD0, 0, 0, 0, TSL2585_MOD0
};

/* Only enable UV-A photodiodes */
static const tsl2585_modulator_t sensor_phd_mod_uv[] = {
    0, 0, 0, TSL2585_MOD0, TSL2585_MOD0, 0
};


/* Sensor control implementation functions */
static osStatus_t sensor_control_start();
static osStatus_t sensor_control_stop();
static osStatus_t sensor_control_set_mode(sensor_mode_t sensor_mode);
static osStatus_t sensor_control_set_config(const sensor_control_config_params_t *params);
static osStatus_t sensor_control_set_light_mode(const sensor_control_light_mode_params_t *params);
static osStatus_t sensor_control_interrupt(const sensor_control_interrupt_params_t *params);
static HAL_StatusTypeDef sensor_control_read_fifo(uint8_t *als_status, uint8_t *als_status2, uint8_t *als_status3, uint32_t *als_data);

void task_sensor_run(void *argument)
{
    osSemaphoreId_t task_start_semaphore = argument;
    HAL_StatusTypeDef ret;
    sensor_control_event_t control_event;

    log_d("sensor_task start");

    /* Create the queue for sensor control events */
    sensor_control_queue = osMessageQueueNew(20, sizeof(sensor_control_event_t), &sensor_control_queue_attrs);
    if (!sensor_control_queue) {
        log_e("Unable to create control queue");
        return;
    }

    /* Create the one-element queue to hold the latest sensor reading */
    sensor_reading_queue = osMessageQueueNew(1, sizeof(sensor_reading_t), &sensor_reading_queue_attrs);
    if (!sensor_reading_queue) {
        log_e("Unable to create reading queue");
        return;
    }

    /* Create the semaphore used to synchronize sensor control */
    sensor_control_semaphore = osSemaphoreNew(1, 0, &sensor_control_semaphore_attrs);
    if (!sensor_control_semaphore) {
        log_e("sensor_control_semaphore create error");
        return;
    }

    /*
     * Do a basic initialization of the sensor, which verifies that
     * the sensor is functional and responding to commands.
     */
    do {
        ret = tsl2585_init(&hi2c1);
        if (ret != HAL_OK) { break; }

        ret = tsl2585_get_uv_calibration(&hi2c1, &sensor_state.uv_calibration);
        if (ret != HAL_OK) { break; }

        log_d("UV calibration value: %d", sensor_state.uv_calibration);
    } while (0);

    if (ret != HAL_OK) {
        log_e("Sensor initialization failed");
        sensor_initialized = false;
    } else {
        /* Set the initialized flag */
        sensor_initialized = true;
    }

    /* Release the startup semaphore */
    if (osSemaphoreRelease(task_start_semaphore) != osOK) {
        log_e("Unable to release task_start_semaphore");
        return;
    }

    /* Start the main control event loop */
    for (;;) {
        if(osMessageQueueGet(sensor_control_queue, &control_event, NULL, portMAX_DELAY) == osOK) {
            osStatus_t ret = osOK;
            switch (control_event.event_type) {
            case SENSOR_CONTROL_STOP:
                ret = sensor_control_stop();
                break;
            case SENSOR_CONTROL_START:
                ret = sensor_control_start();
                break;
            case SENSOR_CONTROL_SET_MODE:
                ret = sensor_control_set_mode(control_event.sensor_mode);
                break;
            case SENSOR_CONTROL_SET_CONFIG:
                ret = sensor_control_set_config(&control_event.config);
                break;
            case SENSOR_CONTROL_SET_LIGHT_MODE:
                ret = sensor_control_set_light_mode(&control_event.light_mode);
                break;
            case SENSOR_CONTROL_INTERRUPT:
                ret = sensor_control_interrupt(&control_event.interrupt);
                break;
            default:
                break;
            }

            /* Handle all external commands by propagating their completion */
            if (control_event.event_type != SENSOR_CONTROL_INTERRUPT) {
                if (control_event.result) {
                    *(control_event.result) = ret;
                }
                if (osSemaphoreRelease(sensor_control_semaphore) != osOK) {
                    log_e("Unable to release sensor_control_semaphore");
                }
            }
        }
    }
}

bool sensor_is_initialized()
{
    return sensor_initialized;
}

osStatus_t sensor_start()
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_START,
        .result = &result
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_start()
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_start");

    do {
        sensor_state.running = false;

        /* Query the initial state of the sensor */
        if (!sensor_state.config_pending) {
            ret = tsl2585_get_mod_gain(&hi2c1, TSL2585_MOD0, TSL2585_STEP0, &sensor_state.gain);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_sample_time(&hi2c1, &sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_als_num_samples(&hi2c1, &sensor_state.sample_count);
            if (ret != HAL_OK) { break; }
        }

        if (!sensor_state.agc_pending) {
            ret = tsl2585_get_agc_num_samples(&hi2c1, &sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_calibration_nth_iteration(&hi2c1, &sensor_state.calibration_iteration);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_agc_calibration(&hi2c1, &sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }
        }

        /* Put the sensor into a known initial state */

        /* Enable writing of ALS status to the FIFO */
        ret = tsl2585_set_fifo_als_status_write_enable(&hi2c1, true);
        if (ret != HAL_OK) { break; }

        /* Enable writing of just modulator 0 results to the FIFO */
        ret = tsl2585_set_fifo_data_write_enable(&hi2c1, TSL2585_MOD0, true);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(&hi2c1, TSL2585_MOD1, false);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(&hi2c1, TSL2585_MOD2, false);
        if (ret != HAL_OK) { break; }

        /* Set FIFO data format to 32-bits */
        ret = tsl2585_set_fifo_als_data_format(&hi2c1, TSL2585_ALS_FIFO_32BIT);
        if (ret != HAL_OK) { break; }

        /* Set MSB position for full 26-bit result */
        ret = tsl2585_set_als_msb_position(&hi2c1, 6);
        if (ret != HAL_OK) { break; }

        /* Make sure residuals are enabled */
        ret = tsl2585_set_mod_residual_enable(&hi2c1, TSL2585_MOD0, TSL2585_STEPS_ALL);

        /* Select alternate gain table, which caps gain at 256x but gives us more residual bits */
        ret = tsl2585_set_mod_gain_table_select(&hi2c1, true);
        if (ret != HAL_OK) { break; }

        /* Set maximum gain to 256x per app note on residual measurement */
        ret = tsl2585_set_max_mod_gain(&hi2c1, TSL2585_GAIN_256X);
        if (ret != HAL_OK) { break; }

        /* Enable modulator 0 */
        ret = tsl2585_enable_modulators(&hi2c1, TSL2585_MOD0);
        if (ret != HAL_OK) { break; }

        /* Apply any startup settings */
        if (sensor_state.mode_pending) {
            const tsl2585_modulator_t *sensor_phd_mod;
            switch (sensor_state.sensor_mode) {
            case SENSOR_MODE_DEFAULT:
                sensor_phd_mod = sensor_phd_mod_default;
                break;
            case SENSOR_MODE_VIS:
                sensor_phd_mod = sensor_phd_mod_vis;
                break;
            case SENSOR_MODE_UV:
                sensor_phd_mod = sensor_phd_mod_uv;
                break;
            default:
                return osErrorParameter;
            }

            ret = tsl2585_set_mod_photodiode_smux(&hi2c1, TSL2585_STEP0, sensor_phd_mod);
            if (ret != HAL_OK) { break; }

            sensor_state.mode_pending = false;
        }

        if (sensor_state.config_pending) {
            ret = tsl2585_set_mod_gain(&hi2c1, TSL2585_MOD0, TSL2585_STEP0, sensor_state.gain);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_sample_time(&hi2c1, sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_als_num_samples(&hi2c1, sensor_state.sample_count);
            if (ret != HAL_OK) { break; }

            sensor_state.config_pending = false;
        }

        if (sensor_state.agc_pending) {
            ret = tsl2585_set_agc_num_samples(&hi2c1, sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_calibration_nth_iteration(&hi2c1, sensor_state.calibration_iteration);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_calibration(&hi2c1, sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }

            sensor_state.agc_pending = false;
        }

        /* Log initial state */
        const float als_atime = tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count);
        const float agc_atime = tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.agc_sample_count);
        log_d("TSL2585 Initial State: Mode=%d, Gain=%s, ALS_ATIME=%.2fms, AGC_ATIME=%.2fms",
            sensor_state.sensor_mode, tsl2585_gain_str(sensor_state.gain), als_atime, agc_atime);

        /* Clear out any old sensor readings */
        osMessageQueueReset(sensor_reading_queue);
        reading_count = 0;

        /* Configure to interrupt on every ALS cycle */
        ret = tsl2585_set_als_interrupt_persistence(&hi2c1, 0);
        if (ret != HAL_OK) {
            break;
        }

        /* Enable sensor ALS interrupts */
        ret = tsl2585_set_interrupt_enable(&hi2c1, TSL2585_INTENAB_AIEN);
        if (ret != HAL_OK) {
            break;
        }

        /* Enable the sensor (ALS Enable and Power ON) */
        ret = tsl2585_enable(&hi2c1);
        if (ret != HAL_OK) {
            break;
        }

        sensor_state.discard_next_reading = true;
        sensor_state.running = true;
    } while (0);

    return hal_to_os_status(ret);
}

osStatus_t sensor_stop()
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_STOP,
        .result = &result
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_stop()
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_stop");

    do {
        ret = tsl2585_disable(&hi2c1);
        if (ret != HAL_OK) { break; }
        sensor_state.running = false;
    } while (0);

    return hal_to_os_status(ret);
}

osStatus_t sensor_set_config_old(tsl2591_gain_t gain, tsl2591_time_t time)
{
    //FIXME Remove this function
    log_w("Deprecated sensor_set_config");
    return osOK;
}

osStatus_t sensor_set_mode(sensor_mode_t mode)
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_SET_MODE,
        .result = &result,
        .sensor_mode = mode
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_set_mode(sensor_mode_t sensor_mode)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_set_mode: %d", sensor_mode);

    if (sensor_state.running) {
        const tsl2585_modulator_t *sensor_phd_mod;
        switch (sensor_mode) {
        case SENSOR_MODE_DEFAULT:
            sensor_phd_mod = sensor_phd_mod_default;
            break;
        case SENSOR_MODE_VIS:
            sensor_phd_mod = sensor_phd_mod_vis;
            break;
        case SENSOR_MODE_UV:
            sensor_phd_mod = sensor_phd_mod_uv;
            break;
        default:
            return osErrorParameter;
        }

        ret = tsl2585_set_mod_photodiode_smux(&hi2c1, TSL2585_STEP0, sensor_phd_mod);
        if (ret == HAL_OK) {
            sensor_state.sensor_mode = sensor_mode;
        }

        sensor_state.discard_next_reading = true;
        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.sensor_mode = sensor_mode;
        sensor_state.mode_pending = true;
    }

    return hal_to_os_status(ret);

}

osStatus_t sensor_set_config(tsl2585_gain_t gain, uint16_t sample_time, uint16_t sample_count)
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_SET_CONFIG,
        .result = &result,
        .config = {
            .gain = gain,
            .sample_time = sample_time,
            .sample_count = sample_count
        }
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_set_config(const sensor_control_config_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_set_config: %d, %d, %d", params->gain, params->sample_time, params->sample_count);

    if (sensor_state.running) {
        ret = tsl2585_set_mod_gain(&hi2c1, TSL2585_MOD0, TSL2585_STEP0, params->gain);
        if (ret == HAL_OK) {
            sensor_state.gain = params->gain;
        }

        ret = tsl2585_set_sample_time(&hi2c1, params->sample_time);
        if (ret == HAL_OK) {
            sensor_state.sample_time = params->sample_time;
        }

        ret = tsl2585_set_als_num_samples(&hi2c1, params->sample_count);
        if (ret == HAL_OK) {
            sensor_state.sample_count = params->sample_count;
        }

        sensor_state.discard_next_reading = true;
        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.gain = params->gain;
        sensor_state.sample_time = params->sample_time;
        sensor_state.sample_count = params->sample_count;
        sensor_state.config_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t sensor_set_light_mode(sensor_light_t light, bool next_cycle, uint8_t value)
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_SET_LIGHT_MODE,
        .result = &result,
        .light_mode = {
            .light = light,
            .next_cycle = next_cycle,
            .value = value
        }
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_set_light_mode(const sensor_control_light_mode_params_t *params)
{
    //log_d("sensor_set_light_mode: %d, %d, %d", params->light, params->next_cycle, params->value);

    uint8_t pending_vis_reflection;
    uint8_t pending_vis_transmission;
    uint8_t pending_uv_transmission;

    /* Convert the parameters into pending values for the LEDs */
    if (params->light == SENSOR_LIGHT_OFF || params->value == 0) {
        pending_vis_reflection = 0;
        pending_vis_transmission = 0;
        pending_uv_transmission = 0;
    } else if (params->light == SENSOR_LIGHT_VIS_REFLECTION) {
        pending_vis_reflection = params->value;
        pending_vis_transmission = 0;
        pending_uv_transmission = 0;
    } else if (params->light == SENSOR_LIGHT_VIS_TRANSMISSION) {
        pending_vis_reflection = 0;
        pending_vis_transmission = params->value;
        pending_uv_transmission = 0;
    } else if (params->light == SENSOR_LIGHT_UV_TRANSMISSION) {
        pending_vis_reflection = 0;
        pending_vis_transmission = 0;
        pending_uv_transmission = params->value;
    } else {
        pending_vis_reflection = 0;
        pending_vis_transmission = 0;
        pending_uv_transmission = 0;
    }

    taskENTER_CRITICAL();
    if (params->next_cycle) {
        /* Schedule the change for the next ISR invocation */
        pending_int_light_change = 0x80000000
            | pending_vis_reflection
            | (pending_vis_transmission << 8)
            | (pending_uv_transmission << 16);
    } else {
        /* Apply the change immediately */
        light_set_vis_reflection(pending_vis_reflection);
        light_set_vis_transmission(pending_vis_transmission);
        light_set_uv_transmission(pending_uv_transmission);
        light_change_ticks = osKernelGetTickCount();
        pending_int_light_change = 0;
    }
    taskEXIT_CRITICAL();

    return osOK;
}

osStatus_t sensor_get_next_reading_old(sensor_reading_old_t *reading, uint32_t timeout)
{
    //FIXME Remove this function
    log_w("Deprecated sensor_set_config");
    return osOK;
}

osStatus_t sensor_get_next_reading(sensor_reading_t *reading, uint32_t timeout)
{
    if (!sensor_initialized) { return osErrorResource; }

    if (!reading) {
        return osErrorParameter;
    }

    return osMessageQueueGet(sensor_reading_queue, reading, NULL, timeout);
}

void sensor_int_handler()
{
    if (!sensor_initialized) { return; }

    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_INTERRUPT,
        .interrupt = {
            .sensor_ticks = osKernelGetTickCount()
        }
    };

    /* Apply any pending light change values */
    UBaseType_t interrupt_status = taskENTER_CRITICAL_FROM_ISR();
    if ((pending_int_light_change & 0x80000000) == 0x80000000) {
        light_set_vis_reflection(pending_int_light_change & 0x000000FF);
        light_set_vis_transmission((pending_int_light_change & 0x0000FF00) >> 8);
        light_set_uv_transmission((pending_int_light_change & 0x00FF0000) >> 16);
        light_change_ticks = osKernelGetTickCount();
        pending_int_light_change = 0;
    }
    control_event.interrupt.light_ticks = light_change_ticks;
    control_event.interrupt.reading_count = ++reading_count;
    taskEXIT_CRITICAL_FROM_ISR(interrupt_status);

    osMessageQueuePut(sensor_control_queue, &control_event, 0, 0);
}

osStatus_t sensor_control_interrupt(const sensor_control_interrupt_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t status = 0;
    sensor_reading_t reading = {0};
    bool has_reading = false;

    //log_d("sensor_control_interrupt");

    if (!sensor_state.running) {
        log_w("Unexpected sensor interrupt!");
    }

    do {
        /* Get the interrupt status */
        ret = tsl2585_get_status(&hi2c1, &status);
        if (ret != HAL_OK) { break; }

#if 0
        /* Log interrupt flags */
        log_d("MINT=%d, AINT=%d, FINT=%d, SINT=%d",
            (status & TSL2585_STATUS_MINT) != 0,
            (status & TSL2585_STATUS_AINT) != 0,
            (status & TSL2585_STATUS_FINT) != 0,
            (status & TSL2585_STATUS_SINT) != 0);
#endif

        if ((status & TSL2585_STATUS_AINT) != 0) {
            uint32_t elapsed_ticks = params->sensor_ticks - last_aint_ticks;
            last_aint_ticks = params->sensor_ticks;

            uint8_t als_status = 0;
            uint8_t als_status2 = 0;
            uint8_t als_status3 = 0;
            uint32_t als_data = 0;

            ret = sensor_control_read_fifo(&als_status, &als_status2, &als_status3, &als_data);
            if (ret != HAL_OK) { break; }

            if ((als_status & TSL2585_ALS_DATA0_ANALOG_SATURATION_STATUS) != 0) {
                log_d("TSL2585: [analog saturation]");
                reading.als_data = UINT32_MAX;
                reading.gain = sensor_state.gain;
                reading.result_status = SENSOR_RESULT_SATURATED_ANALOG;
            } else {
                tsl2585_gain_t als_gain = (als_status2 & 0x0F);

                /* If AGC is enabled, then update the configured gain value */
                if (sensor_state.agc_enabled) {
                    sensor_state.gain = als_gain;
                }

                reading.als_data = als_data;
                reading.gain = als_gain;
                reading.result_status = SENSOR_RESULT_VALID;

                /* If in UV mode, apply the UV calibration value */
                if (sensor_state.sensor_mode == SENSOR_MODE_UV) {
                    als_data = lroundf(
                        (float)als_data
                        / (1.0F - (((float)sensor_state.uv_calibration - 127.0F) / 100.0F)));
                }
            }

            if (!sensor_state.discard_next_reading) {
                /* Fill out other reading fields */
                reading.sample_time = sensor_state.sample_time;
                reading.sample_count = sensor_state.sample_count;
                reading.reading_ticks = params->sensor_ticks;
                reading.elapsed_ticks = elapsed_ticks;
                reading.light_ticks = params->light_ticks;
                reading.reading_count = params->reading_count;

                has_reading = true;
            } else {
                sensor_state.discard_next_reading = false;
            }

            /*
             * If AGC was just disabled, then reset the gain to its last
             * known value and ignore the reading. This is necessary because
             * disabling AGC on its own seems to reset the gain to a low
             * default, and attempting to set it immediately after setting
             * the registers to disable AGC does not seem to take.
             */
            if (sensor_state.agc_disabled_reset_gain) {
                ret = tsl2585_set_mod_gain(&hi2c1, TSL2585_MOD0, TSL2585_STEP0, sensor_state.gain);
                if (ret != HAL_OK) { break; }
                sensor_state.agc_disabled_reset_gain = false;
                sensor_state.discard_next_reading = true;
            }
        }

        /* Clear the interrupt status */
        ret = tsl2585_set_status(&hi2c1, status);
        if (ret != HAL_OK) { break; }
    } while (0);

    if (has_reading) {
        log_d("TSL2585[%d]: MOD0=%ld, Gain=[%s], Time=%.2fms",
            reading.reading_count,
            reading.als_data, tsl2585_gain_str(reading.gain),
            tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count));

        //FIXME cdc_send_raw_sensor_reading(&reading);

        QueueHandle_t queue = (QueueHandle_t)sensor_reading_queue;
        xQueueOverwrite(queue, &reading);
    }

    return hal_to_os_status(ret);
}

HAL_StatusTypeDef sensor_control_read_fifo(uint8_t *als_status, uint8_t *als_status2, uint8_t *als_status3, uint32_t *als_data)
{
    HAL_StatusTypeDef ret;
    tsl2585_fifo_status_t fifo_status;
    uint8_t data[7];

    do {
        ret = tsl2585_get_fifo_status(&hi2c1, &fifo_status);
        if (ret != HAL_OK) { break; }

#if 0
        log_d("FIFO_STATUS: OVERFLOW=%d, UNDERFLOW=%d, LEVEL=%d", fifo_status.overflow, fifo_status.underflow, fifo_status.level);
#endif

        if (fifo_status.level != 7) {
            log_w("Unexpected size of data in FIFO: %d != 7", fifo_status.level);
            ret = HAL_ERROR;
            break;
        }

        ret = tsl2585_read_fifo(&hi2c1, data, sizeof(data));
        if (ret != HAL_OK) { break; }

        if (als_status) {
            *als_status = data[4];
        }
        if (als_status2) {
            *als_status2 = data[5];
        }
        if (als_status3) {
            *als_status3 = data[6];
        }
        if (als_data) {
            *als_data =
                (uint32_t)data[3] << 24
                | (uint32_t)data[2] << 16
                | (uint32_t)data[1] << 8
                | (uint32_t)data[0];
        }

    } while (0);

    return ret;
}
