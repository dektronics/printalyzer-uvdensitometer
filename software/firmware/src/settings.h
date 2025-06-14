#ifndef SETTINGS_H
#define SETTINGS_H

#include "stm32l0xx_hal.h"

#include <stdbool.h>

#include "tsl2585.h"

/*
 * Selections and defaults for the idle light user settings
 * These are based on a maximum timer value of 16384.
 */
#define SETTING_IDLE_LIGHT_REFL_LOW    2048
#define SETTING_IDLE_LIGHT_REFL_MEDIUM 4096
#define SETTING_IDLE_LIGHT_REFL_HIGH   8192

#define SETTING_IDLE_LIGHT_TRAN_LOW    1024
#define SETTING_IDLE_LIGHT_TRAN_MEDIUM 2048
#define SETTING_IDLE_LIGHT_TRAN_HIGH   4096

#define SETTING_IDLE_LIGHT_REFL_DEFAULT SETTING_IDLE_LIGHT_REFL_MEDIUM
#define SETTING_IDLE_LIGHT_TRAN_DEFAULT SETTING_IDLE_LIGHT_TRAN_LOW

typedef struct {
    float values[TSL2585_GAIN_256X + 1];
} settings_cal_gain_t;

typedef struct {
    float b0;
    float b1;
    float b2;
} settings_cal_temperature_t;

typedef struct {
    float lo_d;
    float lo_value;
    float hi_d;
    float hi_value;
} settings_cal_reflection_t;

typedef struct {
    float zero_value;
    float hi_d;
    float hi_value;
} settings_cal_transmission_t;

typedef enum {
    SETTING_KEY_FORMAT_NUMBER = 0,
    SETTING_KEY_FORMAT_FULL,
    SETTING_KEY_FORMAT_MAX
} setting_key_format_t;

typedef enum {
    SETTING_KEY_SEPARATOR_NONE = 0,
    SETTING_KEY_SEPARATOR_ENTER,
    SETTING_KEY_SEPARATOR_TAB,
    SETTING_KEY_SEPARATOR_COMMA,
    SETTING_KEY_SEPARATOR_SPACE,
    SETTING_KEY_SEPARATOR_MAX
} setting_key_separator_t;

typedef struct {
    bool enabled;
    setting_key_format_t format;
    setting_key_separator_t separator;
} settings_user_usb_key_t;

typedef struct {
    uint16_t reflection;
    uint16_t transmission;
    uint8_t timeout;
} settings_user_idle_light_t;

typedef enum {
    SETTING_DECIMAL_SEPARATOR_PERIOD = 0,
    SETTING_DECIMAL_SEPARATOR_COMMA,
    SETTING_DECIMAL_SEPARATOR_MAX
} settings_decimal_separator_t;

typedef enum {
    SETTING_DISPLAY_UNIT_DENSITY = 0,
    SETTING_DISPLAY_UNIT_FSTOP,
    SETTING_DISPLAY_UNIT_MAX
} settings_display_unit_t;

typedef struct {
    settings_decimal_separator_t separator;
    settings_display_unit_t unit;
} settings_user_display_format_t;

HAL_StatusTypeDef settings_init();

HAL_StatusTypeDef settings_wipe();

/**
 * Set the gain calibration values.
 *
 * @param cal_gain Struct populated with values to save
 * @return True if saved, false on error
 */
bool settings_set_cal_gain(const settings_cal_gain_t *cal_gain);

/**
 * Get the gain calibration values.
 * If a valid set of values are not available, but the provided struct is
 * usable, default values will be returned.
 *
 * @param cal_gain Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_cal_gain(settings_cal_gain_t *cal_gain);

/**
 * Convenience function to get gain calibration fields for a particular
 * gain setting.
 */
float settings_get_cal_gain_value(const settings_cal_gain_t *cal_gain, tsl2585_gain_t gain);

/**
 * Check if the gain calibration values are valid
 *
 * @param cal_gain Struct to validate
 * @return True if valid, false if invalid
 */
bool settings_validate_cal_gain(const settings_cal_gain_t *cal_gain);

/**
 * Set the VIS temperature calibration values.
 *
 * @param cal_temperature Struct populated with values to save
 * @return True if saved, false on error
 */
bool settings_set_cal_vis_temperature(const settings_cal_temperature_t *cal_temperature);

/**
 * Get the VIS temperature calibration values.
 * If a valid set of values are not available, but the provided struct is
 * usable, it will be initialized to NaN.
 *
 * @param cal_temperature Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_cal_vis_temperature(settings_cal_temperature_t *cal_temperature);

/**
 * Check if VIS temperature calibration values are valid
 *
 * @param cal_temperature Struct to validate
 * @return True if valid, false if invalid
 */
bool settings_validate_cal_temperature(const settings_cal_temperature_t *cal_temperature);

/**
 * Set the UV temperature calibration values.
 *
 * @param cal_temperature Struct populated with values to save
 * @return True if saved, false on error
 */
bool settings_set_cal_uv_temperature(const settings_cal_temperature_t *cal_temperature);

/**
 * Get the UV temperature calibration values.
 * If a valid set of values are not available, but the provided struct is
 * usable, it will be initialized to NaN.
 *
 * @param cal_temperature Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_cal_uv_temperature(settings_cal_temperature_t *cal_temperature);

/**
 * Set the VIS reflection density calibration values.
 *
 * @param cal_reflection Struct populated with values to save
 * @return True if saved, false on error
 */
bool settings_set_cal_vis_reflection(const settings_cal_reflection_t *cal_reflection);

/**
 * Get the VIS reflection density calibration values.
 * If a valid set of values are not available, but the provided struct is
 * usable, it will be initialized to NaN.
 *
 * @param cal_reflection Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_cal_vis_reflection(settings_cal_reflection_t *cal_reflection);

/**
 * Check if the reflection calibration values are valid
 *
 * @param cal_reflection Struct to validate
 * @return True if valid, false if invalid
 */
bool settings_validate_cal_reflection(const settings_cal_reflection_t *cal_reflection);

/**
 * Set the VIS transmission density calibration values.
 *
 * @param cal_transmission Struct populated with values to save
 * @return True if saved, false on error
 */
bool settings_set_cal_vis_transmission(const settings_cal_transmission_t *cal_transmission);

/**
 * Get the VIS transmission density calibration values.
 * If a valid set of values are not available, but the provided struct is
 * usable, it will be initialized to NaN.
 *
 * @param cal_transmission Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_cal_vis_transmission(settings_cal_transmission_t *cal_transmission);

/**
 * Set the UV transmission density calibration values.
 *
 * @param cal_transmission Struct populated with values to save
 * @return True if saved, false on error
 */
bool settings_set_cal_uv_transmission(const settings_cal_transmission_t *cal_transmission);

/**
 * Get the UV transmission density calibration values.
 * If a valid set of values are not available, but the provided struct is
 * usable, it will be initialized to NaN.
 *
 * @param cal_transmission Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_cal_uv_transmission(settings_cal_transmission_t *cal_transmission);

/**
 * Check if the transmission calibration values are valid
 *
 * @param cal_transmission Struct to validate
 * @return True if valid, false if invalid
 */
bool settings_validate_cal_transmission(const settings_cal_transmission_t *cal_transmission);

/**
 * Set the user settings for the USB key output feature
 *
 * @param usb_key Struct populated with the values to save
 * @return True if saved, false on error
 */
bool settings_set_user_usb_key(const settings_user_usb_key_t *usb_key);

/**
 * Get the user settings for the USB key output feature
 *
 * @param usb_key Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_user_usb_key(settings_user_usb_key_t *usb_key);

/**
 * Set the user settings for the idle light behavior
 *
 * @param idle_light Struct populated with the values to save
 * @return True if saved, false on error
 */
bool settings_set_user_idle_light(const settings_user_idle_light_t *idle_light);

/**
 * Get the user settings for the idle light behavior
 *
 * @param idle_light Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_user_idle_light(settings_user_idle_light_t *idle_light);

/**
 * Set the user settings for the display format
 *
 * @param display_format Struct populated with the values to save
 * @return True if saved, false on error
 */
bool settings_set_user_display_format(const settings_user_display_format_t *display_format);

/**
 * Get the user settings for the display format
 *
 * @param display_format Struct to be populated with saved values
 * @return True if valid values are returned, false otherwise.
 */
bool settings_get_user_display_format(settings_user_display_format_t *display_format);

/**
 * Convenience function to get the decimal separator from the display format
 *
 * @return Character that corresponds to the current setting
 */
char settings_get_decimal_separator();

/**
 * Convenience function to get the unit suffix from the display format
 *
 * @return Character that corresponds to the current setting
 */
char settings_get_unit_suffix();

#endif /* SETTINGS_H */
