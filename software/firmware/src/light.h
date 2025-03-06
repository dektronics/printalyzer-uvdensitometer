#ifndef LIGHT_H
#define LIGHT_H

#include "stm32l0xx_hal.h"

void light_init(TIM_HandleTypeDef *htim, uint32_t r_channel, uint32_t tv_channel, uint32_t tu_channel);

uint16_t light_get_max_value();

void light_set_vis_reflection(uint16_t val);
void light_set_vis_transmission(uint16_t val);
void light_set_uv_transmission(uint16_t val);

void light_int_handler(uint32_t channel, uint32_t interrupt);

#endif /* LIGHT_H */
