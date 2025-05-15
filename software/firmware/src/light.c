#include "light.h"

#include "elog.h"
#include "stm32l0xx_hal.h"

/*
 * Externally, this provides a simple interface for controlling the LED
 * drivers on in the device.
 * Internally, it implements special handling of the peculiarities of
 * the MIC4811/MIC4812 LED driver.
 *
 * Starting LEDs from an off state requires a 60us pulse prior to normal
 * PWM operation.
 * If LEDs are turned off for more than 10ms (40ms max), then they need
 * the longer pulse to turn on again.
 * There may be additional timing requirements that manifest around
 * the minimum and maximum duty cycle, which will be documented later.
 */

/*
 * Duration of the startup pulse for the LED driver.
 * The driver requires a 60us pulse, so we're setting it to 65us
 * just to be safe.
 */
#define STARTUP_PULSE_US 65

typedef enum {
    LIGHT_STATE_OFF = 0,
    LIGHT_STATE_STARTUP,
    LIGHT_STATE_ON
} light_state_t;

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    volatile uint8_t state;
    volatile uint16_t val;
} light_t;

static uint16_t light_startup_count = 0;
static uint16_t light_val_max = 0;
static light_t light_vis_refl = {0};
static light_t light_vis_tran = {0};
static light_t light_uv_tran = {0};

static void light_set_val(light_t *light, uint16_t val);

void light_init(TIM_HandleTypeDef *htim, uint32_t r_channel, uint32_t tv_channel, uint32_t tu_channel)
{
    light_vis_refl.htim = htim;
    light_vis_refl.channel = r_channel;

    light_vis_tran.htim = htim;
    light_vis_tran.channel = tv_channel;

    light_uv_tran.htim = htim;
    light_uv_tran.channel = tu_channel;

    /* Set us to a known initial state */
    light_set_frequency(LIGHT_FREQUENCY_DEFAULT);
}

void light_set_frequency(light_frequency_t frequency)
{
    TIM_HandleTypeDef *htim = light_vis_tran.htim;

    if (light_vis_refl.state != LIGHT_STATE_OFF
        || light_vis_tran.state != LIGHT_STATE_OFF
        || light_uv_tran.state != LIGHT_STATE_OFF) {
        log_w("Changing light frequency while lights are not off");
    }

    if (frequency == LIGHT_FREQUENCY_HIGH) {
        /*
         * Frequency: 125kHz
         * This frequency works the best for gain calibration use cases, but doesn't
         * provide the best adjustment granularity or a good value-vs-brightness
         * relationship.
         */
        __HAL_TIM_SET_PRESCALER(htim, 1);
        __HAL_TIM_SET_AUTORELOAD(htim, 127);
    } else {
        /*
         * Frequency: 651Hz (default)
         * This frequency works best for adjustable brightness at around 8x sensor gain,
         * and has a sufficiently 1:1 relationship between value and brightness out to
         * an equivalent density reduction of 2.0D.
         * Unfortunately, it does not work well for reducing measured brightness
         * at higher sensor gain settings, and doesn't produce the best results
         * for certain gain calibration pairs.
         */
        __HAL_TIM_SET_PRESCALER(htim, 2);
        __HAL_TIM_SET_AUTORELOAD(htim, 16383);
    }

    const uint32_t clock_freq = HAL_RCC_GetSysClockFreq();
    const uint32_t timer_prescaler = htim->Instance->PSC;
    light_startup_count = (STARTUP_PULSE_US * (clock_freq / 1000000)) / (timer_prescaler + 1);
    light_val_max = __HAL_TIM_GET_AUTORELOAD(htim) + 1;

    /* Reset the counter states */
    __HAL_TIM_DISABLE_OCxPRELOAD(light_vis_refl.htim, light_vis_refl.channel);
    __HAL_TIM_DISABLE_OCxPRELOAD(light_vis_tran.htim, light_vis_tran.channel);
    __HAL_TIM_DISABLE_OCxPRELOAD(light_uv_tran.htim, light_uv_tran.channel);
    __HAL_TIM_SET_COUNTER(light_vis_refl.htim, 0);
    __HAL_TIM_SET_COMPARE(light_vis_refl.htim, light_vis_refl.channel, 0);
    __HAL_TIM_SET_COMPARE(light_vis_tran.htim, light_vis_tran.channel, 0);
    __HAL_TIM_SET_COMPARE(light_uv_tran.htim, light_uv_tran.channel, 0);
}

uint16_t light_get_max_value()
{
    return light_val_max;
}

void light_set_vis_reflection(uint16_t val)
{
    light_set_val(&light_vis_refl, val);
}

void light_set_vis_transmission(uint16_t val)
{
    light_set_val(&light_vis_tran, val);
}

void light_set_uv_transmission(uint16_t val)
{
    light_set_val(&light_uv_tran, val);
}

void light_set_val(light_t *light, uint16_t val)
{
    const uint16_t light_max = light_get_max_value();

    if (val > light_max) { val = light_max; }

    if (light->state == LIGHT_STATE_OFF) {
        if (val >= light_startup_count) {
            __HAL_TIM_ENABLE_OCxPRELOAD(light->htim, light->channel);
            __HAL_TIM_SET_COMPARE(light->htim, light->channel, val);
            HAL_TIM_PWM_Start(light->htim, light->channel);
            light->state = LIGHT_STATE_ON;
        } else if (val > 0) {
            /* Special PWM startup routine */
            light->state = LIGHT_STATE_STARTUP;
            __HAL_TIM_DISABLE_OCxPRELOAD(light->htim, light->channel);
            __HAL_TIM_SET_COUNTER(light->htim, 0);
            __HAL_TIM_SET_COMPARE(light->htim, light->channel, light_startup_count);
            HAL_TIM_PWM_Start_IT(light->htim, light->channel);
        }
        light->val = val;
    } else if (light->state == LIGHT_STATE_ON) {
        if (val == 0) {
            __HAL_TIM_SET_COMPARE(light->htim, light->channel, val);
            HAL_TIM_PWM_Stop(light->htim, light->channel);
            light->state = LIGHT_STATE_OFF;
        } else {
            __HAL_TIM_SET_COMPARE(light->htim, light->channel, val);
        }
        light->val = val;
    } else if (light->state == LIGHT_STATE_STARTUP) {
        // Unsupported case for the moment
    }
}

void light_int_handler(uint32_t channel, uint32_t interrupt)
{
    light_t *light = NULL;
    if (channel == light_vis_refl.channel) {
        light = &light_vis_refl;
    } else if (channel == light_vis_tran.channel) {
        light = &light_vis_tran;
    } else if (channel == light_uv_tran.channel) {
        light = &light_uv_tran;
    }
    if (!light || light->state != LIGHT_STATE_STARTUP) {
        return;
    }

    __HAL_TIM_DISABLE_IT(light->htim, interrupt);
    __HAL_TIM_ENABLE_OCxPRELOAD(light->htim, light->channel);
    __HAL_TIM_SET_COMPARE(light->htim, light->channel, light->val);
    light->state = LIGHT_STATE_ON;
}
