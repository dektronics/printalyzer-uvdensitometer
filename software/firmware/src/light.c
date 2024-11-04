#include "light.h"

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

typedef enum {
    LIGHT_STATE_OFF = 0,
    LIGHT_STATE_STARTUP,
    LIGHT_STATE_ON
} light_state_t;

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    volatile uint8_t state;
    volatile uint8_t startup_count;
    volatile uint8_t val;
} light_t;

static light_t light_vis_refl = {0};
static light_t light_vis_tran = {0};
static light_t light_uv_tran = {0};

static void light_set_val(light_t *light, uint8_t val);

void light_init(TIM_HandleTypeDef *htim, uint32_t r_channel, uint32_t tv_channel, uint32_t tu_channel)
{
    light_vis_refl.htim = htim;
    light_vis_refl.channel = r_channel;

    light_vis_tran.htim = htim;
    light_vis_tran.channel = tv_channel;

    light_uv_tran.htim = htim;
    light_uv_tran.channel = tu_channel;

    /* Set us to a known initial state */
    __HAL_TIM_DISABLE_OCxPRELOAD(light_vis_refl.htim, light_vis_refl.channel);
    __HAL_TIM_DISABLE_OCxPRELOAD(light_vis_tran.htim, light_vis_tran.channel);
    __HAL_TIM_DISABLE_OCxPRELOAD(light_uv_tran.htim, light_uv_tran.channel);
    __HAL_TIM_SET_COUNTER(light_vis_refl.htim, 0);
    __HAL_TIM_SET_COMPARE(light_vis_refl.htim, light_vis_refl.channel, 0);
    __HAL_TIM_SET_COMPARE(light_vis_tran.htim, light_vis_tran.channel, 0);
    __HAL_TIM_SET_COMPARE(light_uv_tran.htim, light_uv_tran.channel, 0);
}

void light_set_vis_reflection(uint8_t val)
{
    light_set_val(&light_vis_refl, val);
}

void light_set_vis_transmission(uint8_t val)
{
    light_set_val(&light_vis_tran, val);
}

void light_set_uv_transmission(uint8_t val)
{
    light_set_val(&light_uv_tran, val);
}

void light_set_val(light_t *light, uint8_t val)
{
    if (light->state == LIGHT_STATE_OFF) {
        if (val == 128) {
            __HAL_TIM_ENABLE_OCxPRELOAD(light->htim, light->channel);
            __HAL_TIM_SET_COMPARE(light->htim, light->channel, val);
            HAL_TIM_PWM_Start(light->htim, light->channel);
            light->state = LIGHT_STATE_ON;
        } else if (val > 0) {
            /* Special PWM startup routine */
            light->state = LIGHT_STATE_STARTUP;
            light->startup_count = 0;
            __HAL_TIM_DISABLE_OCxPRELOAD(light->htim, light->channel);
            __HAL_TIM_SET_COUNTER(light->htim, 0);
            __HAL_TIM_SET_COMPARE(light->htim, light->channel, 0);
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

    light->startup_count++;
    if (light->startup_count == 1) {
        __HAL_TIM_SET_COMPARE(light->htim, light->channel, 128);
    } else if (light->startup_count >= 3) {
        __HAL_TIM_DISABLE_IT(light->htim, interrupt);
        __HAL_TIM_ENABLE_OCxPRELOAD(light->htim, light->channel);
        __HAL_TIM_SET_COMPARE(light->htim, light->channel, light->val);
        light->state = LIGHT_STATE_ON;
        light->startup_count = 0;
    }
}
