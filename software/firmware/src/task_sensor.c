#include "task_sensor.h"

#define LOG_TAG "task_sensor"
#include <elog.h>

#include <math.h>
#include <cmsis_os.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "stm32l0xx_hal.h"
#include "board_config.h"
#include "tsl2585.h"
#include "mcp9808.h"
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
    SENSOR_CONTROL_SET_TRIGGER_MODE,
    SENSOR_CONTROL_SET_GAIN,
    SENSOR_CONTROL_SET_INTEGRATION,
    SENSOR_CONTROL_SET_AGC_ENABLED,
    SENSOR_CONTROL_SET_AGC_DISABLED,
    SENSOR_CONTROL_SET_LIGHT_MODE,
    SENSOR_CONTROL_TRIGGER_NEXT_READING,
    SENSOR_CONTROL_READ_TEMPERATURE,
    SENSOR_CONTROL_INTERRUPT
} sensor_control_event_type_t;

typedef struct {
    tsl2585_gain_t gain;
    tsl2585_modulator_t mod;
} sensor_control_gain_params_t;

typedef struct {
    uint16_t sample_time;
    uint16_t sample_count;
} sensor_control_integration_params_t;

typedef struct {
    uint16_t sample_count;
} sensor_control_agc_params_t;

typedef struct {
    sensor_light_t light;
    bool next_cycle;
    uint16_t value;
} sensor_control_light_mode_params_t;

typedef struct {
    float *temp_c;
} sensor_control_read_temperature_params_t;

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
        tsl2585_trigger_mode_t trigger_mode;
        sensor_control_gain_params_t gain;
        sensor_control_integration_params_t integration;
        sensor_control_agc_params_t agc;
        sensor_control_light_mode_params_t light_mode;
        sensor_control_read_temperature_params_t read_temperature;
        sensor_control_interrupt_params_t interrupt;
    };
} sensor_control_event_t;

/**
 * Configuration state for the TSL2585 light sensor
 */
typedef struct {
    bool running;
    bool dual_mod;
    uint8_t uv_calibration;
    sensor_mode_t sensor_mode;
    tsl2585_trigger_mode_t trigger_mode;
    tsl2585_gain_t gain[3];
    uint16_t sample_time;
    uint16_t sample_count;
    bool agc_enabled;
    uint16_t agc_sample_count;
    bool mode_pending;
    bool gain_pending;
    bool integration_pending;
    bool agc_pending;
    bool sai_active;
    bool agc_disabled_reset_gain;
    bool discard_next_reading;
} tsl2585_state_t;

/**
 * Modulator data read out of the FIFO
 */
typedef struct {
    uint8_t als_status;
    uint8_t als_status2;
    uint8_t als_status3;
    uint32_t als_data0;
    uint32_t als_data1;
} tsl2585_fifo_data_t;

/* Global I2C handle for the sensor */
extern I2C_HandleTypeDef hi2c1;

static volatile bool sensor_initialized = false;
static volatile bool temp_sensor_initialized = false;
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

/* Only enable Photopic photodiodes (dual modulators) */
static const tsl2585_modulator_t sensor_phd_mod_vis_dual[] = {
    0, TSL2585_MOD0, 0, 0, 0, TSL2585_MOD1
};

/* Only enable UV-A photodiodes (dual modulators) */
static const tsl2585_modulator_t sensor_phd_mod_uv_dual[] = {
    0, 0, 0, TSL2585_MOD0, TSL2585_MOD1, 0
};

/* Sensor control implementation functions */
static osStatus_t sensor_control_start();
static osStatus_t sensor_control_stop();
static osStatus_t sensor_control_set_mode(sensor_mode_t sensor_mode);
static osStatus_t sensor_control_set_trigger_mode(tsl2585_trigger_mode_t trigger_mode);
static osStatus_t sensor_control_set_gain(const sensor_control_gain_params_t *params);
static osStatus_t sensor_control_set_integration(const sensor_control_integration_params_t *params);
static osStatus_t sensor_control_set_agc_enabled(const sensor_control_agc_params_t *params);
static osStatus_t sensor_control_set_agc_disabled();
static osStatus_t sensor_control_set_light_mode(const sensor_control_light_mode_params_t *params);
static void sensor_light_change_impl(sensor_light_t light, uint16_t value);
static osStatus_t sensor_control_trigger_next_reading();
static osStatus_t sensor_control_read_temperature(sensor_control_read_temperature_params_t *params);
static osStatus_t sensor_control_interrupt(const sensor_control_interrupt_params_t *params);
static HAL_StatusTypeDef sensor_control_read_fifo(tsl2585_fifo_data_t *fifo_data);
static HAL_StatusTypeDef sensor_control_set_mod_photodiode_smux(sensor_mode_t mode);

static void sensor_set_vsync_state(bool state);

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

    /*
     * Initialize the temperature sensor, which will be accessed from
     * the same task as the light sensor to avoid I2C peripheral
     * synchronization issues.
     */
    ret = mcp9808_init(&hi2c1);

    if (ret != HAL_OK) {
        log_e("Temperature sensor initialization failed");
        temp_sensor_initialized = false;
    } else {
        /* Set the initialized flag */
        temp_sensor_initialized = true;
    }

    /*
     * Set some sensible defaults just in case the sensor isn't configured
     * prior to starting. Without this, we could run with an integration
     * time faster than we can deal with and overflow the FIFO.
     * Its safest to keep our default integration time at least 10ms,
     * though setting it to 100ms for a comfortable buffer.
     */
    sensor_state.sample_time = 719;
    sensor_state.sample_count = 99;
    sensor_state.integration_pending = true;

    /* Release the startup semaphore */
    if (osSemaphoreRelease(task_start_semaphore) != osOK) {
        log_e("Unable to release task_start_semaphore");
        return;
    }

    /* Start the main control event loop */
    for (;;) {
        if(osMessageQueueGet(sensor_control_queue, &control_event, NULL, portMAX_DELAY) == osOK) {
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
            case SENSOR_CONTROL_SET_TRIGGER_MODE:
                ret = sensor_control_set_trigger_mode(control_event.trigger_mode);
                break;
            case SENSOR_CONTROL_SET_GAIN:
                ret = sensor_control_set_gain(&control_event.gain);
                break;
            case SENSOR_CONTROL_SET_INTEGRATION:
                ret = sensor_control_set_integration(&control_event.integration);
                break;
            case SENSOR_CONTROL_SET_AGC_ENABLED:
                ret = sensor_control_set_agc_enabled(&control_event.agc);
                break;
            case SENSOR_CONTROL_SET_AGC_DISABLED:
                ret = sensor_control_set_agc_disabled();
                break;
            case SENSOR_CONTROL_SET_LIGHT_MODE:
                ret = sensor_control_set_light_mode(&control_event.light_mode);
                break;
            case SENSOR_CONTROL_TRIGGER_NEXT_READING:
                ret = sensor_control_trigger_next_reading();
                break;
            case SENSOR_CONTROL_READ_TEMPERATURE:
                ret = sensor_control_read_temperature(&control_event.read_temperature);
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

        /* Check whether we should start in single or dual modulator mode */
        if (sensor_state.sensor_mode == SENSOR_MODE_VIS_DUAL || sensor_state.sensor_mode == SENSOR_MODE_UV_DUAL) {
            sensor_state.dual_mod = true;
        } else {
            sensor_state.dual_mod = false;
        }

        /* Clear the FIFO */
        ret = tsl2585_clear_fifo(&hi2c1);
        if (ret != HAL_OK) { break; }

        /* Query the initial state of the sensor */
        if (!sensor_state.gain_pending) {
            ret = tsl2585_get_mod_gain(&hi2c1, TSL2585_MOD0, TSL2585_STEP0, &sensor_state.gain[0]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_mod_gain(&hi2c1, TSL2585_MOD1, TSL2585_STEP0, &sensor_state.gain[1]);
            if (ret != HAL_OK) { break; }
        }
        if (!sensor_state.integration_pending) {
            ret = tsl2585_get_sample_time(&hi2c1, &sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_als_num_samples(&hi2c1, &sensor_state.sample_count);
            if (ret != HAL_OK) { break; }
        }

        if (!sensor_state.agc_pending) {
            ret = tsl2585_get_agc_num_samples(&hi2c1, &sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_agc_calibration(&hi2c1, &sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }
        }

        /* Put the sensor into a known initial state */

        /* Enable writing of ALS status to the FIFO */
        ret = tsl2585_set_fifo_als_status_write_enable(&hi2c1, true);
        if (ret != HAL_OK) { break; }

        /* Enable writing of results to the FIFO */
        ret = tsl2585_set_fifo_data_write_enable(&hi2c1, TSL2585_MOD0, true);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(&hi2c1, TSL2585_MOD1, sensor_state.dual_mod);
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
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_mod_residual_enable(&hi2c1, TSL2585_MOD1, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }

        /* Select alternate gain table, which caps gain at 256x but gives us more residual bits */
        ret = tsl2585_set_mod_gain_table_select(&hi2c1, true);
        if (ret != HAL_OK) { break; }

        /* Set maximum gain to 256x per app note on residual measurement */
        ret = tsl2585_set_max_mod_gain(&hi2c1, TSL2585_GAIN_256X);
        if (ret != HAL_OK) { break; }

        /* Enable modulator(s) */
        ret = tsl2585_enable_modulators(&hi2c1, TSL2585_MOD0 | (sensor_state.dual_mod ? TSL2585_MOD1 : 0));
        if (ret != HAL_OK) { break; }

        /* Enable internal calibration on every sequencer round */
        ret = tsl2585_set_calibration_nth_iteration(&hi2c1, 1);
        if (ret != HAL_OK) { break; }

        /* Set initial state of the VSYNC pin to high */
        sensor_set_vsync_state(true);

        /* Set VSYNC pin configuration */
        ret = tsl2585_set_vsync_config(&hi2c1, TSL2585_VSYNC_CFG_VSYNC_INVERT);
        if (ret != HAL_OK) { break; }

        /* Set VSYNC pin as input */
        ret = tsl2585_set_vsync_gpio_int(&hi2c1, TSL2585_GPIO_INT_VSYNC_GPIO_IN_EN | TSL2585_GPIO_INT_VSYNC_GPIO_INVERT);
        if (ret != HAL_OK) { break; }

        /* Apply any startup settings */
        if (sensor_state.mode_pending) {
            ret = sensor_control_set_mod_photodiode_smux(sensor_state.sensor_mode);
            if (ret != HAL_OK) { break; }

            sensor_state.mode_pending = false;
        }

        if (sensor_state.gain_pending) {
            ret = tsl2585_set_mod_gain(&hi2c1, TSL2585_MOD0, TSL2585_STEP0, sensor_state.gain[0]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_mod_gain(&hi2c1, TSL2585_MOD1, TSL2585_STEP0, sensor_state.gain[1]);
            if (ret != HAL_OK) { break; }

            sensor_state.gain_pending = false;
        }

        if (sensor_state.integration_pending) {
            ret = tsl2585_set_sample_time(&hi2c1, sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_als_num_samples(&hi2c1, sensor_state.sample_count);
            if (ret != HAL_OK) { break; }

            sensor_state.integration_pending = false;
        }

        if (sensor_state.agc_pending) {
            ret = tsl2585_set_agc_num_samples(&hi2c1, sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_calibration(&hi2c1, sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }

            sensor_state.agc_pending = false;
        }

        /* Log initial state */
        const float als_atime = tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count);
        const float agc_atime = tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.agc_sample_count);
        log_d("TSL2585 Initial State: Mode=%d, Gain=%s,%s, ALS_ATIME=%.2fms, AGC_ATIME=%.2fms",
            sensor_state.sensor_mode,
            tsl2585_gain_str(sensor_state.gain[0]), tsl2585_gain_str(sensor_state.gain[1]),
            als_atime, agc_atime);

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

        /* Set the trigger mode */
        ret = tsl2585_set_trigger_mode(&hi2c1, sensor_state.trigger_mode);
        if (ret != HAL_OK) {
            break;
        }

        /* Enable the sensor (ALS Enable and Power ON) */
        ret = tsl2585_enable(&hi2c1);
        if (ret != HAL_OK) {
            break;
        }

        if (sensor_state.trigger_mode == TSL2585_TRIGGER_VSYNC) {
            /* In VSYNC trigger mode, we need to cycle the pin to prime the trigger */
            osDelay(1);
            sensor_set_vsync_state(false);
            osDelay(1);
            sensor_set_vsync_state(true);
            osDelay(1);
            sensor_state.discard_next_reading = false;
        } else {
            /* In continuous modes, discard the first reading */
            sensor_state.discard_next_reading = true;
        }
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
        ret = sensor_control_set_mod_photodiode_smux(sensor_mode);
        if (ret == HAL_OK) {
            sensor_state.sensor_mode = sensor_mode;
        }

        if (sensor_state.trigger_mode != TSL2585_TRIGGER_VSYNC) {
            sensor_state.discard_next_reading = true;
        }
        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.sensor_mode = sensor_mode;
        sensor_state.mode_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t sensor_set_trigger_mode(tsl2585_trigger_mode_t trigger_mode)
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_SET_TRIGGER_MODE,
        .result = &result,
        .trigger_mode = trigger_mode
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_set_trigger_mode(tsl2585_trigger_mode_t trigger_mode)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_set_trigger_mode: %d", trigger_mode);

    if (sensor_state.running) {
        ret = tsl2585_set_trigger_mode(&hi2c1, trigger_mode);

        if (ret == HAL_OK) {
            sensor_state.trigger_mode = trigger_mode;
        }

        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.trigger_mode = trigger_mode;
    }

    return hal_to_os_status(ret);
}

osStatus_t sensor_set_config(tsl2585_gain_t gain, uint16_t sample_time, uint16_t sample_count)
{
    osStatus_t result;

    result = sensor_set_gain(gain, TSL2585_MOD0);

    if (result != osOK) { return result; }

    result = sensor_set_integration(sample_time, sample_count);

    return result;
}

osStatus_t sensor_set_gain(tsl2585_gain_t gain, tsl2585_modulator_t mod)
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_SET_GAIN,
        .result = &result,
        .gain = {
            .gain = gain,
            .mod = mod
        }
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_set_gain(const sensor_control_gain_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_set_gain: %d, 0x%02X", params->gain, params->mod);

    uint8_t mod_index;
    switch (params->mod) {
    case TSL2585_MOD0:
        mod_index = 0;
        break;
    case TSL2585_MOD1:
        mod_index = 1;
        break;
    case TSL2585_MOD2:
        mod_index = 2;
        break;
    default:
        return osErrorParameter;
    }

    if (sensor_state.running) {
        ret = tsl2585_set_mod_gain(&hi2c1, params->mod, TSL2585_STEP0, params->gain);
        if (ret == HAL_OK) {
            sensor_state.gain[mod_index] = params->gain;
        }

        if (sensor_state.trigger_mode != TSL2585_TRIGGER_VSYNC) {
            sensor_state.discard_next_reading = true;
        }
        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.gain[mod_index] = params->gain;
        sensor_state.gain_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t sensor_set_integration(uint16_t sample_time, uint16_t sample_count)
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_SET_INTEGRATION,
        .result = &result,
        .integration = {
            .sample_time = sample_time,
            .sample_count = sample_count
        }
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_set_integration(const sensor_control_integration_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_set_integration: %d, %d", params->sample_time, params->sample_count);

    if (sensor_state.running) {
        ret = tsl2585_set_sample_time(&hi2c1, params->sample_time);
        if (ret == HAL_OK) {
            sensor_state.sample_time = params->sample_time;
        }

        ret = tsl2585_set_als_num_samples(&hi2c1, params->sample_count);
        if (ret == HAL_OK) {
            sensor_state.sample_count = params->sample_count;
        }

        if (sensor_state.trigger_mode != TSL2585_TRIGGER_VSYNC) {
            sensor_state.discard_next_reading = true;
        }
        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.sample_time = params->sample_time;
        sensor_state.sample_count = params->sample_count;
        sensor_state.integration_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t sensor_set_agc_enabled(uint16_t sample_count)
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_SET_AGC_ENABLED,
        .result = &result,
        .agc = {
            .sample_count = sample_count
        }
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_set_agc_enabled(const sensor_control_agc_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_set_agc_enabled: %d", params->sample_count);

    if (sensor_state.running) {
        ret = tsl2585_set_agc_num_samples(&hi2c1, params->sample_count);
        if (ret == HAL_OK) {
            sensor_state.agc_sample_count = sensor_state.sample_count;
        }

        ret = tsl2585_set_agc_calibration(&hi2c1, true);
        if (ret == HAL_OK) {
            sensor_state.agc_enabled = true;
        }
    } else {
        sensor_state.agc_enabled = true;
        sensor_state.agc_sample_count = params->sample_count;
        sensor_state.agc_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t sensor_set_agc_disabled()
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_SET_AGC_DISABLED,
        .result = &result
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_set_agc_disabled()
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("sensor_control_set_agc_disabled");

    if (sensor_state.running) {
        do {
            ret = tsl2585_set_agc_calibration(&hi2c1, false);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_num_samples(&hi2c1, 0);
            if (ret != HAL_OK) { break; }
        } while (0);
        if (ret == HAL_OK) {
            sensor_state.agc_enabled = false;
            sensor_state.agc_disabled_reset_gain = true;
            if (sensor_state.trigger_mode != TSL2585_TRIGGER_VSYNC) {
                sensor_state.discard_next_reading = true;
            }
        }
    } else {
        sensor_state.agc_enabled = false;
        sensor_state.agc_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t sensor_set_light_mode(sensor_light_t light, bool next_cycle, uint16_t value)
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

    taskENTER_CRITICAL();
    if (params->next_cycle) {
        /* Schedule the change for the next ISR invocation */
        pending_int_light_change = 0x80000000
            | (uint8_t)params->light << 16
            | params->value;
    } else {
        /* Apply the change immediately */
        sensor_light_change_impl(params->light, params->value);
        light_change_ticks = osKernelGetTickCount();
        pending_int_light_change = 0;
    }
    taskEXIT_CRITICAL();

    return osOK;
}

void sensor_light_change_impl(sensor_light_t light, uint16_t value)
{
    if (light == SENSOR_LIGHT_VIS_REFLECTION) {
        light_set_vis_transmission(0);
        light_set_uv_transmission(0);
        light_set_vis_reflection(value);
    } else if (light == SENSOR_LIGHT_VIS_TRANSMISSION) {
        light_set_vis_reflection(0);
        light_set_uv_transmission(0);
        light_set_vis_transmission(value);
    } else if (light == SENSOR_LIGHT_UV_TRANSMISSION) {
        light_set_vis_reflection(0);
        light_set_vis_transmission(0);
        light_set_uv_transmission(value);
    } else {
        light_set_vis_reflection(0);
        light_set_vis_transmission(0);
        light_set_uv_transmission(0);
    }
}

osStatus_t sensor_trigger_next_reading()
{
    if (!sensor_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_TRIGGER_NEXT_READING,
        .result = &result
    };
    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_trigger_next_reading()
{
    osStatus_t ret = osOK;

    log_d("sensor_control_trigger_next_reading");

    if (sensor_state.running) {
        if (sensor_state.trigger_mode == TSL2585_TRIGGER_VSYNC) {
            sensor_set_vsync_state(false);
            sensor_set_vsync_state(true);
        } else {
            ret = osErrorResource;
        }
    } else {
        ret = osErrorResource;
    }

    return ret;
}

osStatus_t sensor_get_next_reading(sensor_reading_t *reading, uint32_t timeout)
{
    if (!sensor_initialized) { return osErrorResource; }

    if (!reading) {
        return osErrorParameter;
    }

    return osMessageQueueGet(sensor_reading_queue, reading, NULL, timeout);
}

osStatus_t sensor_read_temperature(float *temp_c)
{
    if (!temp_sensor_initialized) { return osErrorResource; }
    if (!temp_c) { return osErrorParameter; }

    osStatus_t result = osOK;
    sensor_control_event_t control_event = {
        .event_type = SENSOR_CONTROL_READ_TEMPERATURE,
        .result = &result,
        .read_temperature = {
            .temp_c = temp_c
        }
    };

    osMessageQueuePut(sensor_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(sensor_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t sensor_control_read_temperature(sensor_control_read_temperature_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("sensor_control_read_temperature");

    ret = mcp9808_read_temperature(&hi2c1, params->temp_c);

    return hal_to_os_status(ret);
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
        const sensor_light_t light = (pending_int_light_change & 0x00FF0000) >> 16;
        const uint16_t value = pending_int_light_change & 0x0000FFFF;
        sensor_light_change_impl(light, value);
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

    /* Prevent task switching to ensure fast processing of incoming sensor data */
    vTaskSuspendAll();

#if 0
    log_d("sensor_control_interrupt");
#endif

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

            tsl2585_fifo_data_t fifo_data;

            ret = sensor_control_read_fifo(&fifo_data);
            if (ret != HAL_OK) { break; }

            if ((fifo_data.als_status & TSL2585_ALS_DATA0_ANALOG_SATURATION_STATUS) != 0) {
#if 0
                log_d("TSL2585: [0:analog saturation]");
#endif
                reading.mod0.als_data = UINT32_MAX;
                reading.mod0.gain = sensor_state.gain[0];
                reading.mod0.result = SENSOR_RESULT_SATURATED_ANALOG;
            } else {
                tsl2585_gain_t als_gain = (fifo_data.als_status2 & 0x0F);

                /* If AGC is enabled, then update the configured gain value */
                if (sensor_state.agc_enabled) {
                    sensor_state.gain[0] = als_gain;
                }

                reading.mod0.als_data = fifo_data.als_data0;
                reading.mod0.gain = als_gain;
                reading.mod0.result = SENSOR_RESULT_VALID;

                /* If in UV mode, apply the UV calibration value */
                if (sensor_state.sensor_mode == SENSOR_MODE_UV) {
                    reading.mod0.als_data= lroundf(
                        (float)reading.mod0.als_data
                        / (1.0F - (((float)sensor_state.uv_calibration - 127.0F) / 100.0F)));
                }
            }

            if (sensor_state.dual_mod) {
                if ((fifo_data.als_status & TSL2585_ALS_DATA1_ANALOG_SATURATION_STATUS) != 0) {
#if 0
                    log_d("TSL2585: [1:analog saturation]");
#endif
                    reading.mod1.als_data = UINT32_MAX;
                    reading.mod1.gain = sensor_state.gain[1];
                    reading.mod1.result = SENSOR_RESULT_SATURATED_ANALOG;
                } else {
                    tsl2585_gain_t als_gain = (fifo_data.als_status2 & 0xF0) >> 4;

                    reading.mod1.als_data = fifo_data.als_data1;
                    reading.mod1.gain = als_gain;
                    reading.mod1.result = SENSOR_RESULT_VALID;

                    /* If in UV mode, apply the UV calibration value */
                    if (sensor_state.sensor_mode == SENSOR_MODE_UV) {
                        reading.mod1.als_data = lroundf(
                            (float)reading.mod1.als_data
                            / (1.0F - (((float)sensor_state.uv_calibration - 127.0F) / 100.0F)));
                    }
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
                ret = tsl2585_set_mod_gain(&hi2c1, TSL2585_MOD0, TSL2585_STEP0, sensor_state.gain[0]);
                if (ret != HAL_OK) { break; }
                sensor_state.agc_disabled_reset_gain = false;
                if (sensor_state.trigger_mode != TSL2585_TRIGGER_VSYNC) {
                    sensor_state.discard_next_reading = true;
                }
            }
        }

        /* Clear the interrupt status */
        ret = tsl2585_set_status(&hi2c1, status);
        if (ret != HAL_OK) { break; }
    } while (0);

    /* Resume normal task switching */
    xTaskResumeAll();

    if (has_reading) {
#if 1
        if (sensor_state.dual_mod) {
            log_d("TSL2585[%d]: MOD=[%lu,%lu], Gain=[%s,%s], Time=%.2fms",
                reading.reading_count,
                reading.mod0.als_data, reading.mod1.als_data,
                tsl2585_gain_str(reading.mod0.gain), tsl2585_gain_str(reading.mod1.gain),
                tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count));
        } else {
            log_d("TSL2585[%d]: MOD0=%lu, Gain=[%s], Time=%.2fms",
                reading.reading_count,
                reading.mod0.als_data, tsl2585_gain_str(reading.mod0.gain),
                tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count));
        }
#endif
        cdc_send_raw_sensor_reading(&reading);

        QueueHandle_t queue = (QueueHandle_t)sensor_reading_queue;
        xQueueOverwrite(queue, &reading);
    }

    return hal_to_os_status(ret);
}

HAL_StatusTypeDef sensor_control_read_fifo(tsl2585_fifo_data_t *fifo_data)
{
    HAL_StatusTypeDef ret;
    tsl2585_fifo_status_t fifo_status;
    uint8_t data[11];
    const uint8_t data_size = sensor_state.dual_mod ? 11 : 7;

    do {
        ret = tsl2585_get_fifo_status(&hi2c1, &fifo_status);
        if (ret != HAL_OK) { break; }

#if 0
        log_d("FIFO_STATUS: OVERFLOW=%d, UNDERFLOW=%d, LEVEL=%d", fifo_status.overflow, fifo_status.underflow, fifo_status.level);
#endif

        if (fifo_status.level != data_size) {
            log_w("Unexpected size of data in FIFO: %d != %d", fifo_status.level, data_size);
            ret = HAL_ERROR;
            break;
        }

        ret = tsl2585_read_fifo(&hi2c1, data, data_size);
        if (ret != HAL_OK) { break; }

        if (fifo_data) {
            fifo_data->als_data0 =
                (uint32_t)data[3] << 24
                | (uint32_t)data[2] << 16
                | (uint32_t)data[1] << 8
                | (uint32_t)data[0];

            if (sensor_state.dual_mod) {
                fifo_data->als_data1 =
                    (uint32_t)data[7] << 24
                    | (uint32_t)data[6] << 16
                    | (uint32_t)data[5] << 8
                    | (uint32_t)data[4];
                fifo_data->als_status = data[8];
                fifo_data->als_status2 = data[9];
                fifo_data->als_status3 = data[10];
            } else {
                fifo_data->als_data1 = 0;
                fifo_data->als_status = data[4];
                fifo_data->als_status2 = data[5];
                fifo_data->als_status3 = data[6];
            }
        }
    } while (0);

    return ret;
}

HAL_StatusTypeDef sensor_control_set_mod_photodiode_smux(sensor_mode_t mode)
{
    const tsl2585_modulator_t *sensor_phd_mod;
    switch (mode) {
    case SENSOR_MODE_DEFAULT:
        sensor_phd_mod = sensor_phd_mod_default;
        break;
    case SENSOR_MODE_VIS:
        sensor_phd_mod = sensor_phd_mod_vis;
        break;
    case SENSOR_MODE_UV:
        sensor_phd_mod = sensor_phd_mod_uv;
        break;
    case SENSOR_MODE_VIS_DUAL:
        sensor_phd_mod = sensor_phd_mod_vis_dual;
        break;
    case SENSOR_MODE_UV_DUAL:
        sensor_phd_mod = sensor_phd_mod_uv_dual;
        break;
    default:
        log_w("Invalid smux setting: %d", mode);
        return HAL_ERROR;
    }

    return tsl2585_set_mod_photodiode_smux(&hi2c1, TSL2585_STEP0, sensor_phd_mod);
}

void sensor_set_vsync_state(bool state)
{
    HAL_GPIO_WritePin(SENSOR_VSYNC_GPIO_Port, SENSOR_VSYNC_Pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}