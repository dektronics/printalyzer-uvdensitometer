#ifndef LIGHT_H
#define LIGHT_H

#include "stm32l0xx_hal.h"

typedef enum {
    LIGHT_FREQUENCY_DEFAULT = 0,
    LIGHT_FREQUENCY_HIGH
} light_frequency_t;

void light_init(TIM_HandleTypeDef *htim, uint32_t r_channel, uint32_t tv_channel, uint32_t tu_channel);

/**
 * Change the PWM frequency and max value of the measurement lights.
 *
 * This function is only exposed to support very specific calibration
 * use cases, and is not intended to be used generally. It should only
 * be called while the lights are off and within code paths where
 * synchronization is not a concern.
 *
 * @param frequency Frequency setting
 */
void light_set_frequency(light_frequency_t frequency);

uint16_t light_get_max_value();

void light_set_vis_reflection(uint16_t val);
void light_set_vis_transmission(uint16_t val);
void light_set_uv_transmission(uint16_t val);

void light_int_handler(uint32_t channel, uint32_t interrupt);

#endif /* LIGHT_H */
