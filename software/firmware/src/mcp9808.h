/*
 * MCP9808 - +/- 0.5C Maximum Accuracy Digital Temperature Sensor
 */

#ifndef MCP9808_H
#define MCP9808_H

#include <stdbool.h>

#include "stm32l0xx_hal.h"

typedef enum {
    MCP9808_RESOLUTION_05C = 0,  /*<! 0.05C (Tconv = 30ms) */
    MCP9808_RESOLUTION_025C,     /*<! 0.25C (Tconv = 65ms) */
    MCP9808_RESOLUTION_0125C,    /*<! 0.125C (Tconv = 130ms) */
    MCP9808_RESOLUTION_00625C    /*<! 0.0625C (Tconv = 250ms) */
} mcp9808_resolution_t;

/*
 * At power-up, the sensor is enabled and the resolution is
 * set to the maximum.
 */

HAL_StatusTypeDef mcp9808_init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef mcp9808_set_enable(I2C_HandleTypeDef *hi2c, bool enable);

HAL_StatusTypeDef mcp9808_set_resolution(I2C_HandleTypeDef *hi2c, mcp9808_resolution_t value);
HAL_StatusTypeDef mcp9808_get_resolution(I2C_HandleTypeDef *hi2c, mcp9808_resolution_t *value);

HAL_StatusTypeDef mcp9808_read_temperature(I2C_HandleTypeDef *hi2c, float *temp_c);

#endif /* MCP9808_H */
