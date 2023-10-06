#include "mcp9808.h"

#define LOG_TAG "mcp9808"

#include "stm32l0xx_hal.h"

#include <elog.h>

/* I2C device address */
static const uint8_t MCP9808_ADDRESS = 0x18 << 1; // Use 8-bit address

/* Registers */
#define MCP9808_CONFIG     0x01 /*!< Configuration register */
#define MCP9808_TUPPER     0x02 /*!< Alert Temperature Upper Boundary Trip register */
#define MCP9808_TLOWER     0x03 /*!< Alert Temperature Lower Boundary Trip register */
#define MCP9808_TCRIT      0x04 /*!< Critical Temperature Trip register */
#define MCP9808_TA         0x05 /*!< Ambient temperature register */
#define MCP9808_MFG_ID     0x06 /*!< Manufacturer ID register */
#define MCP9808_DEVICE_ID  0x07 /*!< Device ID/Revision register */
#define MCP9808_RESOLUTION 0x08 /*!< Resolution register */

HAL_StatusTypeDef mcp9808_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret;
    uint8_t data[2];

    log_i("Initializing MCP9808");

    /* Read manufacturer ID */
    ret = HAL_I2C_Mem_Read(hi2c, MCP9808_ADDRESS,
        MCP9808_MFG_ID, I2C_MEMADD_SIZE_8BIT,
        data, 2, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Manufacturer ID: 0x%02X%02X", data[0], data[1]);

    if (data[0] != 0x00 && data[1] != 0x54) {
        log_e("Invalid manufacturer ID");
        return HAL_ERROR;
    }

    /* Read Device ID and revision register */
    ret = HAL_I2C_Mem_Read(hi2c, MCP9808_ADDRESS,
        MCP9808_DEVICE_ID, I2C_MEMADD_SIZE_8BIT,
        data, 2, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Device ID: 0x%02X", data[0]);
    log_i("Revision: 0x%02X", data[1]);

    if (data[0] != 0x04) {
        log_e("Invalid device ID");
        return HAL_ERROR;
    }

    /* Read startup configuration */
    ret = HAL_I2C_Mem_Read(hi2c, MCP9808_ADDRESS,
        MCP9808_CONFIG, I2C_MEMADD_SIZE_8BIT,
        data, 2, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("CONFIG: 0x%02X%02X", data[0], data[1]);


    log_i("MCP9808 Initialized");

    return HAL_OK;
}

HAL_StatusTypeDef mcp9808_set_enable(I2C_HandleTypeDef *hi2c, bool enable)
{
    HAL_StatusTypeDef ret;
    uint8_t data[2];

    ret = HAL_I2C_Mem_Read(hi2c, MCP9808_ADDRESS,
        MCP9808_CONFIG, I2C_MEMADD_SIZE_8BIT,
        data, 2, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data[0] &= 0xFE;
    if (!enable) {
        data[0] |= 0x01;
    }

    return HAL_I2C_Mem_Write(hi2c, MCP9808_ADDRESS,
        MCP9808_CONFIG, I2C_MEMADD_SIZE_8BIT,
        data, 2, HAL_MAX_DELAY);
}

HAL_StatusTypeDef mcp9808_set_resolution(I2C_HandleTypeDef *hi2c, mcp9808_resolution_t value)
{
    uint8_t data;
    data = (uint8_t)value & 0x03;

    return HAL_I2C_Mem_Write(hi2c, MCP9808_ADDRESS,
        MCP9808_RESOLUTION, I2C_MEMADD_SIZE_8BIT,
        &data, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef mcp9808_get_resolution(I2C_HandleTypeDef *hi2c, mcp9808_resolution_t *value)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    if (!value) { return HAL_ERROR; }

    ret = HAL_I2C_Mem_Read(hi2c, MCP9808_ADDRESS,
        MCP9808_RESOLUTION, I2C_MEMADD_SIZE_8BIT,
        &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    *value = (mcp9808_resolution_t)(data & 0x03);
    return ret;
}

HAL_StatusTypeDef mcp9808_read_temperature(I2C_HandleTypeDef *hi2c, float *temp_c)
{
    HAL_StatusTypeDef ret;
    uint8_t data[2];

    if (!temp_c) {
        return HAL_ERROR;
    }

    /* Read the ambient temperature register */
    ret = HAL_I2C_Mem_Read(hi2c, MCP9808_ADDRESS,
        MCP9808_TA, I2C_MEMADD_SIZE_8BIT,
        data, 2, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    /* Convert result to a temperature value */

    data[0] &= 0x1F; /* clear flag bits */

    if ((data[0] & 0x10) == 0x10) {
        /* TA < 0C */
        data[0] &= 0x0F; /* clear sign */
        *temp_c = 256.0F - ((data[0] * 16.0F) + (data[1] / 16.0F));
    } else {
        /* TA >= 0C */
        *temp_c = ((data[0] * 16.0F) + (data[1] / 16.0F));
    }

    return HAL_OK;
}
