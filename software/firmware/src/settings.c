#include "settings.h"

#define LOG_TAG "settings"

#include <printf.h>
#include <string.h>
#include <math.h>
#include <elog.h>

#include "util.h"

extern CRC_HandleTypeDef hcrc;

static HAL_StatusTypeDef settings_read_header(bool *valid);
static HAL_StatusTypeDef settings_write_header();

static bool settings_init_cal_sensor(bool force_clear);
static bool settings_clear_cal_sensor();
static bool settings_init_cal_target(bool force_clear);
static bool settings_clear_cal_target();
static bool settings_init_user_settings(bool force_clear);
static bool settings_clear_user_settings();
static bool settings_init_cal_temp_settings(bool force_clear);
static bool settings_clear_cal_temp_settings();

static void settings_set_cal_gain_defaults(settings_cal_gain_t *cal_gain);
static bool settings_load_cal_gain();
static void settings_set_cal_temperature_defaults(settings_cal_temperature_t *cal_temperature);
static bool settings_load_cal_vis_temperature();
static bool settings_load_cal_uv_temperature();
static void settings_set_cal_reflection_defaults(settings_cal_reflection_t *cal_reflection);
static bool settings_load_cal_vis_reflection();
static void settings_set_cal_transmission_defaults(settings_cal_transmission_t *cal_transmission);
static bool settings_load_cal_vis_transmission();
static bool settings_load_cal_uv_transmission();
static void settings_set_user_usb_key_defaults(settings_user_usb_key_t *usb_key);
static bool settings_load_user_usb_key();
static void settings_set_user_idle_light_defaults(settings_user_idle_light_t *idle_light);
static bool settings_load_user_idle_light();
static void settings_set_user_display_format_defaults(settings_user_display_format_t *display_format);
static bool settings_load_user_display_format();

static HAL_StatusTypeDef settings_read_buffer(uint32_t address, uint8_t *data, size_t data_len);
static HAL_StatusTypeDef settings_write_buffer(uint32_t address, const uint8_t *data, size_t data_len);
static HAL_StatusTypeDef settings_erase_page(uint32_t address, size_t len);
#if 0
static float settings_read_float(uint32_t address);
static HAL_StatusTypeDef settings_write_float(uint32_t address, float val);
#endif
static uint32_t settings_read_uint32(uint32_t address);
static HAL_StatusTypeDef settings_write_uint32(uint32_t address, uint32_t val);

/*
 * Header Page (128b)
 * Mostly unused at the moment, will be populated if any top-level system
 * data needs to be stored. Unlike other pages, it begins with a magic
 * string.
 */
#define PAGE_HEADER        (DATA_EEPROM_BASE + 0x0000UL)
#define PAGE_HEADER_SIZE   (128)
#define HEADER_MAGIC       (PAGE_HEADER + 0U) /* "DENSITOMETER\0" */
#define HEADER_START       (PAGE_HEADER + 16U)
#define HEADER_VERSION     1UL

/*
 * Sensor Calibration Data (128b)
 * This page contains data specific to calibration of the sensor behavior
 * without taking any reference targets into account. It is likely that
 * the data stored here will be considered part of factory calibration,
 * unlikely to be performed by a user.
 */
#define PAGE_CAL_SENSOR             (DATA_EEPROM_BASE + 0x0080UL)
#define PAGE_CAL_SENSOR_SIZE        (128)
#define PAGE_CAL_SENSOR_VERSION     1UL

#define CONFIG_CAL_GAIN             (PAGE_CAL_SENSOR + 4U)
#define CONFIG_CAL_GAIN_SIZE        (44U)

#define CONFIG_CAL_RESERVED         (PAGE_CAL_SENSOR + 48U)
#define CONFIG_CAL_RESERVED_SIZE    (32U)

/*
 * Target Calibration Data (128b)
 * This page contains data specific to calibration against reference targets
 * used to ensure the device is providing the correct readings. It is
 * something end users are expected to update periodically, based on
 * materials that may be included with the device.
 */
#define PAGE_CAL_TARGET                    (DATA_EEPROM_BASE + 0x0100UL)
#define PAGE_CAL_TARGET_SIZE               (128U)
#define PAGE_CAL_TARGET_VERSION            1UL

#define CONFIG_CAL_VIS_REFLECTION          (PAGE_CAL_TARGET + 4U)
#define CONFIG_CAL_VIS_REFLECTION_SIZE     (20U)

#define CONFIG_CAL_VIS_TRANSMISSION        (PAGE_CAL_TARGET + 24U)
#define CONFIG_CAL_VIS_TRANSMISSION_SIZE   (16U)

#define CONFIG_CAL_UV_TRANSMISSION         (PAGE_CAL_TARGET + 40U)
#define CONFIG_CAL_UV_TRANSMISSION_SIZE    (16U)

/*
 * User Settings (128b)
 * This page contains any user settings that the device may need to store.
 */
#define PAGE_USER_SETTINGS         (DATA_EEPROM_BASE + 0x0180UL)
#define PAGE_USER_SETTINGS_SIZE    (128)
#define PAGE_USER_SETTINGS_VERSION 3UL

#define CONFIG_USER_USB_KEY        (PAGE_USER_SETTINGS + 4U)
#define CONFIG_USER_USB_KEY_SIZE   (12U)

#define CONFIG_USER_IDLE_LIGHT      (PAGE_USER_SETTINGS + 16U)
#define CONFIG_USER_IDLE_LIGHT_SIZE (12U)

#define CONFIG_USER_DISPLAY_FORMAT      (PAGE_USER_SETTINGS + 28U)
#define CONFIG_USER_DISPLAY_FORMAT_SIZE (8U)

/*
 * Temperature Calibration Data (128b)
*  This page contains data specific to calibration of the sensor's response
*  to temperature, and can be considered a continuation of the sensor
*  calibration data section.
*  The data stored here will be considered part of factory calibration,
 * as it is the result of a process which requires specialized equipment
 * to perform.
 */
#define PAGE_CAL_TEMPERATURE         (DATA_EEPROM_BASE + 0x0200UL)
#define PAGE_CAL_TEMPERATURE_SIZE    (128)
#define PAGE_CAL_TEMPERATURE_VERSION 1UL
#define CONFIG_CAL_VIS_TEMP          (PAGE_CAL_TEMPERATURE + 4U)
#define CONFIG_CAL_VIS_TEMP_SIZE     (16U)
#define CONFIG_CAL_UV_TEMP           (PAGE_CAL_TEMPERATURE + 20U)
#define CONFIG_CAL_UV_TEMP_SIZE      (16U)

static settings_cal_gain_t setting_cal_gain = {0};
static settings_cal_temperature_t setting_cal_vis_temperature = {0};
static settings_cal_temperature_t setting_cal_uv_temperature = {0};
static settings_cal_reflection_t setting_cal_vis_reflection = {0};
static settings_cal_transmission_t setting_cal_vis_transmission = {0};
static settings_cal_transmission_t setting_cal_uv_transmission = {0};
static settings_user_usb_key_t setting_user_usb_key = {0};
static settings_user_idle_light_t setting_user_idle_light = {0};
static settings_user_display_format_t setting_user_display_format = {0};

HAL_StatusTypeDef settings_init()
{
    HAL_StatusTypeDef ret = HAL_OK;
    bool valid = false;

    do {
        log_i("Settings init");

        /* Certain EEPROM operations can take a long time */
        watchdog_slow();

        /* Read and validate the header page */
        ret = settings_read_header(&valid);
        if (ret != HAL_OK) { break; }

        watchdog_refresh();

        /* Initialize all settings data pages, clearing if header page invalid */
        if (!settings_init_cal_sensor(!valid)) { break; }
        watchdog_refresh();

        if (!settings_init_cal_target(!valid)) { break; }
        watchdog_refresh();

        if (!settings_init_user_settings(!valid)) { break; }
        watchdog_refresh();

        if (!settings_init_cal_temp_settings(!valid)) { break; }
        watchdog_refresh();

        /* Initialize the header page if necessary */
        if (!valid) {
            ret = settings_write_header();
            if (ret != HAL_OK) { break; }
            watchdog_refresh();
        }

        log_i("Settings loaded");

    } while (0);

    /* Return watchdog to normal window */
    watchdog_normal();

    return ret;
}

HAL_StatusTypeDef settings_wipe()
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_i("Wiping all EEPROM settings");

    /* Certain EEPROM operations can take a long time */
    watchdog_slow();

    watchdog_refresh();

    do {
        /*
         * Erase starting with the header page, so that the EEPROM will be
         * considered invalid and will be reinitialized on startup if any
         * subsequent page clearing operations fail.
         */
        ret = settings_erase_page(PAGE_HEADER, PAGE_HEADER_SIZE);
        watchdog_refresh();
        if (ret != HAL_OK) { break; }

        ret = settings_erase_page(PAGE_CAL_SENSOR, PAGE_CAL_SENSOR_SIZE);
        watchdog_refresh();
        if (ret != HAL_OK) { break; }

        ret = settings_erase_page(PAGE_CAL_TARGET, PAGE_CAL_TARGET_SIZE);
        watchdog_refresh();
        if (ret != HAL_OK) { break; }

        ret = settings_erase_page(PAGE_USER_SETTINGS, PAGE_USER_SETTINGS_SIZE);
        watchdog_refresh();
        if (ret != HAL_OK) { break; }

        ret = settings_erase_page(PAGE_CAL_TEMPERATURE, PAGE_CAL_TEMPERATURE_SIZE);
        watchdog_refresh();
        if (ret != HAL_OK) { break; }
    } while (0);

    /* Return watchdog to normal window */
    watchdog_normal();

    log_i("Wipe complete");
    return ret;
}

HAL_StatusTypeDef settings_read_header(bool *valid)
{
    HAL_StatusTypeDef ret = HAL_OK;
    bool is_valid = true;
    uint8_t data[PAGE_HEADER_SIZE];

    do {
        /* Read the header into a buffer */
        ret = settings_read_buffer(PAGE_HEADER, data, sizeof(data));
        if (ret != HAL_OK) {
            log_e("Unable to read settings header: %d", ret);
            break;
        }

        /* Validate the magic bytes at the start of the header */
        if (memcmp(data, "DENSITOMETER\0", 13) != 0) {
            log_w("Invalid magic");
            is_valid = false;
            break;
        }

        /* Validate the header version */
        uint32_t version = copy_to_u32(&data[HEADER_START - PAGE_HEADER]);
        if (version != HEADER_VERSION) {
            log_w("Unexpected version: %d", version);
            /*
             * When there are multiple versions, then this should be handled
             * gracefully and shouldn't cause EEPROM validation to fail.
             * Until then, it is probably okay to treat this as if it were
             * part of the magic string.
             */
            is_valid = false;
            break;
        }
    } while (0);

    if (ret == HAL_OK) {
        *valid = is_valid;
    }
    return ret;
}

HAL_StatusTypeDef settings_write_header()
{
    log_i("Write settings header");
    HAL_StatusTypeDef ret;
    uint8_t data[PAGE_HEADER_SIZE];

    /* Fill the page with the magic bytes and version header */
    memset(data, 0, sizeof(data));
    memcpy(data, "DENSITOMETER\0", 13);
    copy_from_u32(&data[HEADER_START - PAGE_HEADER], HEADER_VERSION);

    /* Write the buffer */
    ret = settings_write_buffer(PAGE_HEADER, data, sizeof(data));
    if (ret != HAL_OK) {
        log_e("Unable to write settings header: %d", ret);
    }
    return ret;
}

bool settings_init_cal_sensor(bool force_clear)
{
    bool result;
    /* Initialize all fields to their default values */
    settings_set_cal_gain_defaults(&setting_cal_gain);

    /* Load settings if the version matches */
    uint32_t version = force_clear ? 0 : settings_read_uint32(PAGE_CAL_SENSOR);
    if (version == PAGE_CAL_SENSOR_VERSION) {
        /* Version is good, load data with per-field validation */
        settings_load_cal_gain();
        result = true;
    } else {
        /* Version is bad, initialize a blank page */
        if (!force_clear) {
            log_w("Unexpected sensor cal version: %d != %d", version, PAGE_CAL_SENSOR_VERSION);
        }
        result = settings_clear_cal_sensor();
    }
    return result;
}

bool settings_clear_cal_sensor()
{
    log_i("Clearing sensor cal page");

    /* Zero the page version */
    if (settings_write_uint32(PAGE_CAL_SENSOR, 0UL) != HAL_OK) {
        return false;
    }

    /* Write an empty gain cal struct */
    settings_cal_gain_t cal_gain;
    settings_set_cal_gain_defaults(&cal_gain);
    if (!settings_set_cal_gain(&cal_gain)) {
        return false;
    }

    /* Write the page version */
    if (settings_write_uint32(PAGE_CAL_SENSOR, PAGE_CAL_SENSOR_VERSION) != HAL_OK) {
        return false;
    }

    return true;
}

bool settings_init_cal_target(bool force_clear)
{
    bool result;
    /* Initialize all fields to their default values */
    settings_set_cal_reflection_defaults(&setting_cal_vis_reflection);
    settings_set_cal_transmission_defaults(&setting_cal_vis_transmission);
    settings_set_cal_transmission_defaults(&setting_cal_uv_transmission);

    /* Load settings if the version matches */
    uint32_t version = force_clear ? 0 : settings_read_uint32(PAGE_CAL_TARGET);
    if (version == PAGE_CAL_TARGET_VERSION) {
        /* Version is good, load data with per-field validation */
        settings_load_cal_vis_reflection();
        settings_load_cal_vis_transmission();
        settings_load_cal_uv_transmission();
        result = true;
    } else {
        /* Version is bad, initialize a blank page */
        if (!force_clear) {
            log_w("Unexpected cal target version: %d != %d", version, PAGE_CAL_TARGET_VERSION);
        }
        result = settings_clear_cal_target();
    }

    return result;
}

bool settings_clear_cal_target()
{
    log_i("Clearing target cal page");

    /* Zero the page version */
    if (settings_write_uint32(PAGE_CAL_TARGET, 0UL) != HAL_OK) {
        return false;
    }

    /* Write an empty reflection cal struct */
    settings_cal_reflection_t cal_reflection;
    settings_set_cal_reflection_defaults(&cal_reflection);
    if (!settings_set_cal_vis_reflection(&cal_reflection)) {
        return false;
    }

    /* Write empty transmission cal structs */
    settings_cal_transmission_t cal_transmission;
    settings_set_cal_transmission_defaults(&cal_transmission);
    if (!settings_set_cal_vis_transmission(&cal_transmission)) {
        return false;
    }
    if (!settings_set_cal_uv_transmission(&cal_transmission)) {
        return false;
    }

    /* Write the page version */
    if (settings_write_uint32(PAGE_CAL_TARGET, PAGE_CAL_TARGET_VERSION) != HAL_OK) {
        return false;
    }

    return true;
}

bool settings_init_user_settings(bool force_clear)
{
    bool result;
    /* Initialize all fields to their default values */
    settings_set_user_usb_key_defaults(&setting_user_usb_key);
    settings_set_user_idle_light_defaults(&setting_user_idle_light);

    /* Load settings if the version matches */
    uint32_t version = force_clear ? 0 : settings_read_uint32(PAGE_USER_SETTINGS);
    if (version == PAGE_USER_SETTINGS_VERSION) {
        /* Version is good, load data with per-field validation */
        settings_load_user_usb_key();
        settings_load_user_idle_light();
        settings_load_user_display_format();
        result = true;
    } else if (version == 1 || version == 2) {
        log_i("Migrating user settings from %d->%d", version, PAGE_USER_SETTINGS_VERSION);
        /* Handle the migration from version 1->2 */
        do {
            if (version == 1) {
                /* Load unchanged settings */
                settings_load_user_usb_key();

                /* Set defaults for new settings */
                settings_user_idle_light_t idle_light;
                settings_set_user_idle_light_defaults(&idle_light);
                if (!settings_set_user_idle_light(&idle_light)) {
                    break;
                }

                settings_user_display_format_t display_format;
                settings_set_user_display_format_defaults(&display_format);
                if (!settings_set_user_display_format(&display_format)) {
                    break;
                }
            } else if (version == 2) {
                /* Load unchanged settings */
                settings_load_user_usb_key();
                settings_load_user_idle_light();

                /* Set defaults for new settings */
                settings_user_display_format_t display_format;
                settings_set_user_display_format_defaults(&display_format);
                if (!settings_set_user_display_format(&display_format)) {
                    break;
                }
            }
            /* Update the page version */
            settings_write_uint32(PAGE_USER_SETTINGS, PAGE_USER_SETTINGS_VERSION);
        } while (0);
        result = true;
    } else {
        /* Version is bad, initialize a blank page */
        if (!force_clear) {
            log_w("Unexpected user settings version: %d != %d", version, PAGE_USER_SETTINGS_VERSION);
        }
        result = settings_clear_user_settings();
    }
    return result;
}

bool settings_clear_user_settings()
{
    log_i("Clearing user settings page");

    /* Zero the entire page */
    uint8_t data[PAGE_USER_SETTINGS_SIZE];
    memset(data, 0, sizeof(data));
    if (settings_write_buffer(PAGE_USER_SETTINGS, data, sizeof(data)) != HAL_OK) {
        return false;
    }

    /* Write an empty usb key user settings struct */
    settings_user_usb_key_t usb_key;
    settings_set_user_usb_key_defaults(&usb_key);
    if (!settings_set_user_usb_key(&usb_key)) {
        return false;
    }

    /* Write an empty idle light user settings struct */
    settings_user_idle_light_t idle_light;
    settings_set_user_idle_light_defaults(&idle_light);
    if (!settings_set_user_idle_light(&idle_light)) {
        return false;
    }

    /* Write an empty display format settings struct */
    settings_user_display_format_t display_format;
    settings_set_user_display_format_defaults(&display_format);
    if (!settings_set_user_display_format(&display_format)) {
        return false;
    }

    /* Write the page version */
    if (settings_write_uint32(PAGE_USER_SETTINGS, PAGE_USER_SETTINGS_VERSION) != HAL_OK) {
        return false;
    }

    return true;
}

bool settings_init_cal_temp_settings(bool force_clear)
{
    bool result;
    /* Initialize all fields to their default values */
    settings_set_cal_temperature_defaults(&setting_cal_vis_temperature);
    settings_set_cal_temperature_defaults(&setting_cal_uv_temperature);

    /* Load settings if the version matches */
    uint32_t version = force_clear ? 0 : settings_read_uint32(PAGE_CAL_TEMPERATURE);
    if (version == PAGE_CAL_TEMPERATURE_VERSION) {
        /* Version is good, load data with per-field validation */
        settings_load_cal_vis_temperature();
        settings_load_cal_uv_temperature();
        result = true;
    } else {
        /* Version is bad, initialize a blank page */
        if (!force_clear) {
            log_w("Unexpected sensor cal temp version: %d != %d", version, PAGE_CAL_TEMPERATURE_VERSION);
        }
        result = settings_clear_cal_temp_settings();
    }
    return result;
}

bool settings_clear_cal_temp_settings()
{
    log_i("Clearing sensor cal temp page");

    /* Zero the page version */
    if (settings_write_uint32(PAGE_CAL_TEMPERATURE, 0UL) != HAL_OK) {
        return false;
    }

    /* Write empty temperature cal structs */
    settings_cal_temperature_t cal_temperature;
    settings_set_cal_temperature_defaults(&cal_temperature);
    if (!settings_set_cal_vis_temperature(&cal_temperature)) {
        return false;
    }
    if (!settings_set_cal_uv_temperature(&cal_temperature)) {
        return false;
    }

    /* Write the page version */
    if (settings_write_uint32(PAGE_CAL_TEMPERATURE, PAGE_CAL_TEMPERATURE_VERSION) != HAL_OK) {
        return false;
    }

    return true;
}

void settings_set_cal_gain_defaults(settings_cal_gain_t *cal_gain)
{
    if (!cal_gain) { return; }
    memset(cal_gain, 0, sizeof(settings_cal_gain_t));
    for (size_t i = 0; i <= TSL2585_GAIN_256X; i++) {
        cal_gain->values[i] = tsl2585_gain_value(i);
    }
}

bool settings_set_cal_gain(const settings_cal_gain_t *cal_gain)
{
    HAL_StatusTypeDef ret = HAL_OK;
    size_t offset = 0;
    if (!cal_gain) { return false; }

    uint8_t buf[CONFIG_CAL_GAIN_SIZE];
    for (size_t i = 0; i <= TSL2585_GAIN_256X; i++) {
        copy_from_f32(&buf[offset], cal_gain->values[i]);
        offset += 4;
    }

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, 10);
    copy_from_u32(&buf[40], crc);

    ret = settings_write_buffer(CONFIG_CAL_GAIN, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_cal_gain, cal_gain, sizeof(settings_cal_gain_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_cal_gain()
{
    uint8_t buf[CONFIG_CAL_GAIN_SIZE];

    if (settings_read_buffer(CONFIG_CAL_GAIN, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    uint32_t crc = copy_to_u32(&buf[40]);
    uint32_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, 10);

    if (crc != calculated_crc) {
        log_w("Invalid cal gain CRC: %08X != %08X", crc, calculated_crc);
        return false;
    } else {
        size_t offset = 0;
        for (size_t i = 0; i <= TSL2585_GAIN_256X; i++) {
            setting_cal_gain.values[i] = copy_to_f32(&buf[offset]);
            offset += 4;
        }

        return true;
    }
}

bool settings_get_cal_gain(settings_cal_gain_t *cal_gain)
{
    if (!cal_gain) { return false; }

    /* Copy over the settings values */
    memcpy(cal_gain, &setting_cal_gain, sizeof(settings_cal_gain_t));

    /* Set default values if validation fails */
    if (!settings_validate_cal_gain(cal_gain)) {
        settings_set_cal_gain_defaults(cal_gain);
        return false;
    } else {
        return true;
    }
}

float settings_get_cal_gain_value(const settings_cal_gain_t *cal_gain, tsl2585_gain_t gain)
{
    float result = NAN;

    if (!cal_gain) { return result; }

    if (gain >= TSL2585_GAIN_0_5X && gain <= TSL2585_GAIN_256X) {
        result = cal_gain->values[gain];
        if (isnan(result)) {
            result = tsl2585_gain_value(gain);
        }
    } else {
        result = tsl2585_gain_value(gain);
    }

    return result;
}

bool settings_validate_cal_gain(const settings_cal_gain_t *cal_gain)
{
    if (!cal_gain) { return false; }

    /* Validate field numeric properties */
    for (size_t i = 0; i <= TSL2585_GAIN_256X; i++) {
        if (isnan(cal_gain->values[i])) {
            return false;
        }
    }

    /* Validate field values */
    for (size_t i = 1; i <= TSL2585_GAIN_256X; i++) {
        if (cal_gain->values[i] <= cal_gain->values[i - 1]) {
            return false;
        }
    }

    return true;
}

void settings_set_cal_temperature_defaults(settings_cal_temperature_t *cal_temperature)
{
    if (!cal_temperature) { return; }
    memset(cal_temperature, 0, sizeof(settings_cal_temperature_t));
    cal_temperature->b0 = NAN;
    cal_temperature->b1 = NAN;
    cal_temperature->b2 = NAN;
}

bool settings_set_cal_vis_temperature(const settings_cal_temperature_t *cal_temperature)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!cal_temperature) { return false; }

    uint8_t buf[CONFIG_CAL_VIS_TEMP_SIZE];
    copy_from_f32(&buf[0], cal_temperature->b0);
    copy_from_f32(&buf[4], cal_temperature->b1);
    copy_from_f32(&buf[8], cal_temperature->b2);

    const uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, (CONFIG_CAL_VIS_TEMP_SIZE - 4) / 4);
    copy_from_u32(&buf[CONFIG_CAL_VIS_TEMP_SIZE - 4], crc);

    ret = settings_write_buffer(CONFIG_CAL_VIS_TEMP, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_cal_vis_temperature, cal_temperature, sizeof(settings_cal_temperature_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_cal_vis_temperature()
{
    uint8_t buf[CONFIG_CAL_VIS_TEMP_SIZE];

    if (settings_read_buffer(CONFIG_CAL_VIS_TEMP, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    const uint32_t crc = copy_to_u32(&buf[CONFIG_CAL_VIS_TEMP_SIZE - 4]);
    const uint32_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, (CONFIG_CAL_VIS_TEMP_SIZE - 4) / 4);

    if (crc != calculated_crc) {
        log_w("Invalid cal VIS temp CRC: %08X != %08X", crc, calculated_crc);
        return false;
    } else {
        setting_cal_vis_temperature.b0 = copy_to_f32(&buf[0]);
        setting_cal_vis_temperature.b1 = copy_to_f32(&buf[4]);
        setting_cal_vis_temperature.b2 = copy_to_f32(&buf[8]);
        return true;
    }
}

bool settings_get_cal_vis_temperature(settings_cal_temperature_t *cal_temperature)
{
    if (!cal_temperature) { return false; }

    /* Copy over the settings values */
    memcpy(cal_temperature, &setting_cal_vis_temperature, sizeof(settings_cal_temperature_t));

    /* Set default values if validation fails */
    if (!settings_validate_cal_temperature(cal_temperature)) {
        settings_set_cal_temperature_defaults(cal_temperature);
        return false;
    } else {
        return true;
    }
}

bool settings_validate_cal_temperature(const settings_cal_temperature_t *cal_temperature)
{
    if (!cal_temperature) { return false; }

    /* Validate field numeric properties */
    if (isnan(cal_temperature->b0) || isinf(cal_temperature->b0)) {
        return false;
    }
    if (isnan(cal_temperature->b1) || isinf(cal_temperature->b1)) {
        return false;
    }
    if (isnan(cal_temperature->b2) || isinf(cal_temperature->b2)) {
        return false;
    }

    return true;
}

bool settings_set_cal_uv_temperature(const settings_cal_temperature_t *cal_temperature)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!cal_temperature) { return false; }

    uint8_t buf[CONFIG_CAL_UV_TEMP_SIZE];
    copy_from_f32(&buf[0], cal_temperature->b0);
    copy_from_f32(&buf[4], cal_temperature->b1);
    copy_from_f32(&buf[8], cal_temperature->b2);

    const uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, (CONFIG_CAL_VIS_TEMP_SIZE - 4) / 4);
    copy_from_u32(&buf[CONFIG_CAL_VIS_TEMP_SIZE - 4], crc);

    ret = settings_write_buffer(CONFIG_CAL_UV_TEMP, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_cal_uv_temperature, cal_temperature, sizeof(settings_cal_temperature_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_cal_uv_temperature()
{
    uint8_t buf[CONFIG_CAL_UV_TEMP_SIZE];

    if (settings_read_buffer(CONFIG_CAL_UV_TEMP, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    const uint32_t crc = copy_to_u32(&buf[CONFIG_CAL_UV_TEMP_SIZE - 4]);
    const uint32_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, (CONFIG_CAL_UV_TEMP_SIZE - 4) / 4);

    if (crc != calculated_crc) {
        log_w("Invalid cal UV temp CRC: %08X != %08X", crc, calculated_crc);
        return false;
    } else {
        setting_cal_uv_temperature.b0 = copy_to_f32(&buf[0]);
        setting_cal_uv_temperature.b1 = copy_to_f32(&buf[4]);
        setting_cal_uv_temperature.b2 = copy_to_f32(&buf[8]);
        return true;
    }
}

bool settings_get_cal_uv_temperature(settings_cal_temperature_t *cal_temperature)
{
    if (!cal_temperature) { return false; }

    /* Copy over the settings values */
    memcpy(cal_temperature, &setting_cal_uv_temperature, sizeof(settings_cal_temperature_t));

    /* Set default values if validation fails */
    if (!settings_validate_cal_temperature(cal_temperature)) {
        settings_set_cal_temperature_defaults(cal_temperature);
        return false;
    } else {
        return true;
    }
}

void settings_set_cal_reflection_defaults(settings_cal_reflection_t *cal_reflection)
{
    if (!cal_reflection) { return; }
    memset(cal_reflection, 0, sizeof(settings_cal_reflection_t));
    cal_reflection->lo_d = NAN;
    cal_reflection->lo_value = NAN;
    cal_reflection->hi_d = NAN;
    cal_reflection->hi_value = NAN;
}

bool settings_set_cal_vis_reflection(const settings_cal_reflection_t *cal_reflection)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!cal_reflection) { return false; }

    uint8_t buf[CONFIG_CAL_VIS_REFLECTION_SIZE];
    copy_from_f32(&buf[0], cal_reflection->lo_d);
    copy_from_f32(&buf[4], cal_reflection->lo_value);
    copy_from_f32(&buf[8], cal_reflection->hi_d);
    copy_from_f32(&buf[12], cal_reflection->hi_value);

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, 4);
    copy_from_u32(&buf[16], crc);

    ret = settings_write_buffer(CONFIG_CAL_VIS_REFLECTION, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_cal_vis_reflection, cal_reflection, sizeof(settings_cal_reflection_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_cal_vis_reflection()
{
    uint8_t buf[CONFIG_CAL_VIS_REFLECTION_SIZE];

    if (settings_read_buffer(CONFIG_CAL_VIS_REFLECTION, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    uint32_t crc = copy_to_u32(&buf[16]);
    uint32_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, 4);

    if (crc != calculated_crc) {
        log_w("Invalid cal VIS reflection CRC: %08X != %08X", crc, calculated_crc);
        return false;
    } else {
        setting_cal_vis_reflection.lo_d = copy_to_f32(&buf[0]);
        setting_cal_vis_reflection.lo_value = copy_to_f32(&buf[4]);
        setting_cal_vis_reflection.hi_d = copy_to_f32(&buf[8]);
        setting_cal_vis_reflection.hi_value = copy_to_f32(&buf[12]);
        return true;
    }
}

bool settings_get_cal_vis_reflection(settings_cal_reflection_t *cal_reflection)
{
    if (!cal_reflection) { return false; }

    /* Copy over the settings values */
    memcpy(cal_reflection, &setting_cal_vis_reflection, sizeof(settings_cal_reflection_t));

    /* Set default values if validation fails */
    if (!settings_validate_cal_reflection(cal_reflection)) {
        log_w("Invalid VIS reflection calibration values");
        log_w("CAL-LO: D=%.2f, VALUE=%f", cal_reflection->lo_d, cal_reflection->lo_value);
        log_w("CAL-HI: D=%.2f, VALUE=%f", cal_reflection->hi_d, cal_reflection->hi_value);
        settings_set_cal_reflection_defaults(cal_reflection);
        return false;
    } else {
        return true;
    }
}

bool settings_validate_cal_reflection(const settings_cal_reflection_t *cal_reflection)
{
    if (!cal_reflection) { return false; }

    /* Validate standalone CAL-LO properties */
    if (isnan(cal_reflection->lo_d) || isinf(cal_reflection->lo_d)) {
        return false;
    }
    if (isnan(cal_reflection->lo_value) || isinf(cal_reflection->lo_value)) {
        return false;
    }
    if (cal_reflection->lo_d < -0.5F || cal_reflection->lo_value < 0.0F) {
        return false;
    }

    /* Shortcut out if CAL-HI is not set */
    if (isnan(cal_reflection->hi_d) && isnan(cal_reflection->hi_value)) {
        return true;
    }

    /* Validate standalone CAL-HI properties */
    if (isnan(cal_reflection->hi_d) || isinf(cal_reflection->hi_d)) {
        return false;
    }
    if (isnan(cal_reflection->hi_value) || isinf(cal_reflection->hi_value)) {
        return false;
    }

    /* Validate combined properties */
    if (cal_reflection->hi_d <= cal_reflection->lo_d
        || cal_reflection->hi_value >= cal_reflection->lo_value) {
        return false;
    }

    return true;
}

void settings_set_cal_transmission_defaults(settings_cal_transmission_t *cal_transmission)
{
    if (!cal_transmission) { return; }
    memset(cal_transmission, 0, sizeof(settings_cal_transmission_t));
    cal_transmission->zero_value = NAN;
    cal_transmission->hi_d = NAN;
    cal_transmission->hi_value = NAN;
}

bool settings_set_cal_vis_transmission(const settings_cal_transmission_t *cal_transmission)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!cal_transmission) { return false; }

    uint8_t buf[CONFIG_CAL_VIS_TRANSMISSION_SIZE];
    copy_from_f32(&buf[0], cal_transmission->zero_value);
    copy_from_f32(&buf[4], cal_transmission->hi_d);
    copy_from_f32(&buf[8], cal_transmission->hi_value);

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, 3);
    copy_from_u32(&buf[12], crc);

    ret = settings_write_buffer(CONFIG_CAL_VIS_TRANSMISSION, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_cal_vis_transmission, cal_transmission, sizeof(settings_cal_transmission_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_cal_vis_transmission()
{
    uint8_t buf[CONFIG_CAL_VIS_TRANSMISSION_SIZE];

    if (settings_read_buffer(CONFIG_CAL_VIS_TRANSMISSION, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    uint32_t crc = copy_to_u32(&buf[12]);
    uint32_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, 3);

    if (crc != calculated_crc) {
        log_w("Invalid cal VIS transmission CRC: %08X != %08X", crc, calculated_crc);
        return false;
    } else {
        setting_cal_vis_transmission.zero_value = copy_to_f32(&buf[0]);
        setting_cal_vis_transmission.hi_d = copy_to_f32(&buf[4]);
        setting_cal_vis_transmission.hi_value = copy_to_f32(&buf[8]);
        return true;
    }
}

bool settings_get_cal_vis_transmission(settings_cal_transmission_t *cal_transmission)
{
    if (!cal_transmission) { return false; }

    /* Copy over the settings values */
    memcpy(cal_transmission, &setting_cal_vis_transmission, sizeof(settings_cal_transmission_t));

    /* Set default values if validation fails */
    if (!settings_validate_cal_transmission(cal_transmission)) {
        log_w("Invalid VIS transmission calibration values");
        log_w("CAL-ZERO: VALUE=%f", cal_transmission->zero_value);
        log_w("CAL-HI: D=%.2f, VALUE=%f", cal_transmission->hi_d, cal_transmission->hi_value);
        settings_set_cal_transmission_defaults(cal_transmission);
        return false;
    } else {
        return true;
    }
}

bool settings_set_cal_uv_transmission(const settings_cal_transmission_t *cal_transmission)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!cal_transmission) { return false; }

    uint8_t buf[CONFIG_CAL_UV_TRANSMISSION_SIZE];
    copy_from_f32(&buf[0], cal_transmission->zero_value);
    copy_from_f32(&buf[4], cal_transmission->hi_d);
    copy_from_f32(&buf[8], cal_transmission->hi_value);

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, 3);
    copy_from_u32(&buf[12], crc);

    ret = settings_write_buffer(CONFIG_CAL_UV_TRANSMISSION, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_cal_uv_transmission, cal_transmission, sizeof(settings_cal_transmission_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_cal_uv_transmission()
{
    uint8_t buf[CONFIG_CAL_UV_TRANSMISSION_SIZE];

    if (settings_read_buffer(CONFIG_CAL_UV_TRANSMISSION, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    uint32_t crc = copy_to_u32(&buf[12]);
    uint32_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buf, 3);

    if (crc != calculated_crc) {
        log_w("Invalid cal UV transmission CRC: %08X != %08X", crc, calculated_crc);
        return false;
    } else {
        setting_cal_uv_transmission.zero_value = copy_to_f32(&buf[0]);
        setting_cal_uv_transmission.hi_d = copy_to_f32(&buf[4]);
        setting_cal_uv_transmission.hi_value = copy_to_f32(&buf[8]);
        return true;
    }
}

bool settings_get_cal_uv_transmission(settings_cal_transmission_t *cal_transmission)
{
    if (!cal_transmission) { return false; }

    /* Copy over the settings values */
    memcpy(cal_transmission, &setting_cal_uv_transmission, sizeof(settings_cal_transmission_t));

    /* Set default values if validation fails */
    if (!settings_validate_cal_transmission(cal_transmission)) {
        log_w("Invalid UV transmission calibration values");
        log_w("CAL-ZERO: VALUE=%f", cal_transmission->zero_value);
        log_w("CAL-HI: D=%.2f, VALUE=%f", cal_transmission->hi_d, cal_transmission->hi_value);
        settings_set_cal_transmission_defaults(cal_transmission);
        return false;
    } else {
        return true;
    }
}

bool settings_validate_cal_transmission(const settings_cal_transmission_t *cal_transmission)
{
    if (!cal_transmission) { return false; }

    /* Validate field numeric properties */
    if (isnan(cal_transmission->zero_value) || isinf(cal_transmission->zero_value)) {
        return false;
    }
    if (isnan(cal_transmission->hi_d) || isinf(cal_transmission->hi_d)) {
        return false;
    }
    if (isnan(cal_transmission->hi_value) || isinf(cal_transmission->hi_value)) {
        return false;
    }

    /* Validate field values */
    if (cal_transmission->zero_value <= 0.0F
        || cal_transmission->hi_d <= 0.0F || cal_transmission->hi_value <= 0.0F
        || cal_transmission->hi_value >= cal_transmission->zero_value) {
        return false;
    }

    return true;
}

void settings_set_user_usb_key_defaults(settings_user_usb_key_t *usb_key)
{
    if (!usb_key) { return; }
    memset(usb_key, 0, sizeof(settings_user_usb_key_t));
    usb_key->enabled = false;
    usb_key->format = SETTING_KEY_FORMAT_NUMBER;
    usb_key->separator = SETTING_KEY_SEPARATOR_NONE;
}

bool settings_set_user_usb_key(const settings_user_usb_key_t *usb_key)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!usb_key) { return false; }

    uint8_t buf[CONFIG_USER_USB_KEY_SIZE];
    copy_from_u32(&buf[0], (uint32_t)usb_key->enabled);
    copy_from_u32(&buf[4], (uint32_t)usb_key->format);
    copy_from_u32(&buf[8], (uint32_t)usb_key->separator);

    ret = settings_write_buffer(CONFIG_USER_USB_KEY, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_user_usb_key, usb_key, sizeof(settings_user_usb_key_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_user_usb_key()
{
    uint8_t buf[CONFIG_USER_USB_KEY_SIZE];

    if (settings_read_buffer(CONFIG_USER_USB_KEY, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    setting_user_usb_key.enabled = (bool)copy_to_u32(&buf[0]);
    setting_user_usb_key.format = (setting_key_format_t)copy_to_u32(&buf[4]);
    setting_user_usb_key.separator = (setting_key_separator_t)copy_to_u32(&buf[8]);
    return true;
}

bool settings_get_user_usb_key(settings_user_usb_key_t *usb_key)
{
    if (!usb_key) { return false; }

    /* Copy over the settings values */
    memcpy(usb_key, &setting_user_usb_key, sizeof(settings_user_usb_key_t));

    /* Set default values if validation fails */
    if (usb_key->format < 0 || usb_key->format >= SETTING_KEY_FORMAT_MAX
        || usb_key->separator < 0 || usb_key->separator >= SETTING_KEY_SEPARATOR_MAX) {
        log_w("Invalid USB key user settings values");
        settings_set_user_usb_key_defaults(usb_key);
        return false;
    } else {
        return true;
    }
}

void settings_set_user_idle_light_defaults(settings_user_idle_light_t *idle_light)
{
    if (!idle_light) { return; }
    memset(idle_light, 0, sizeof(settings_user_idle_light_t));
    idle_light->reflection = SETTING_IDLE_LIGHT_REFL_DEFAULT;
    idle_light->transmission = SETTING_IDLE_LIGHT_TRAN_DEFAULT;
    idle_light->timeout = 0;
}

bool settings_set_user_idle_light(const settings_user_idle_light_t *idle_light)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!idle_light) { return false; }

    uint8_t buf[CONFIG_USER_IDLE_LIGHT_SIZE];
    copy_from_u32(&buf[0], (uint32_t)idle_light->reflection);
    copy_from_u32(&buf[4], (uint32_t)idle_light->transmission);
    copy_from_u32(&buf[8], (uint32_t)idle_light->timeout);

    ret = settings_write_buffer(CONFIG_USER_IDLE_LIGHT, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_user_idle_light, idle_light, sizeof(settings_user_idle_light_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_user_idle_light()
{
    uint8_t buf[CONFIG_USER_IDLE_LIGHT_SIZE];

    if (settings_read_buffer(CONFIG_USER_IDLE_LIGHT, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    setting_user_idle_light.reflection = (uint16_t)copy_to_u32(&buf[0]);
    setting_user_idle_light.transmission = (uint16_t)copy_to_u32(&buf[4]);
    setting_user_idle_light.timeout = (uint8_t)copy_to_u32(&buf[8]);
    return true;
}

bool settings_get_user_idle_light(settings_user_idle_light_t *idle_light)
{
    if (!idle_light) { return false; }

    /* Copy over the settings values */
    memcpy(idle_light, &setting_user_idle_light, sizeof(settings_user_idle_light_t));

    /* Set default values if validation fails */
    if (idle_light->reflection > SETTING_IDLE_LIGHT_REFL_HIGH
        || idle_light->transmission > SETTING_IDLE_LIGHT_TRAN_HIGH) {
        log_w("Invalid idle light user settings values");
        settings_set_user_idle_light_defaults(idle_light);
        return false;
    } else {
        return true;
    }
}

void settings_set_user_display_format_defaults(settings_user_display_format_t *display_format)
{
    if (!display_format) { return; }
    memset(display_format, 0, sizeof(settings_user_display_format_t));
    display_format->separator = SETTING_DECIMAL_SEPARATOR_PERIOD;
    display_format->unit = SETTING_DISPLAY_UNIT_DENSITY;
}

bool settings_set_user_display_format(const settings_user_display_format_t *display_format)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!display_format) { return false; }

    uint8_t buf[CONFIG_USER_DISPLAY_FORMAT_SIZE];
    copy_from_u32(&buf[0], (uint32_t)display_format->separator);
    copy_from_u32(&buf[4], (uint32_t)display_format->unit);

    ret = settings_write_buffer(CONFIG_USER_DISPLAY_FORMAT, buf, sizeof(buf));

    if (ret == HAL_OK) {
        memcpy(&setting_user_display_format, display_format, sizeof(settings_user_display_format_t));
        return true;
    } else {
        return false;
    }
}

bool settings_load_user_display_format()
{
    uint8_t buf[CONFIG_USER_DISPLAY_FORMAT_SIZE];

    if (settings_read_buffer(CONFIG_USER_DISPLAY_FORMAT, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    setting_user_display_format.separator = (uint8_t)copy_to_u32(&buf[0]);
    setting_user_display_format.unit = (uint8_t)copy_to_u32(&buf[4]);
    return true;
}

bool settings_get_user_display_format(settings_user_display_format_t *display_format)
{
    if (!display_format) { return false; }

    /* Copy over the settings values */
    memcpy(display_format, &setting_user_display_format, sizeof(settings_user_display_format_t));

    /* Set default values if validation fails */
    if (display_format->separator >= SETTING_DECIMAL_SEPARATOR_MAX
        || display_format->unit >= SETTING_DISPLAY_UNIT_MAX) {
        log_w("Invalid display format user settings values");
        settings_set_user_display_format_defaults(display_format);
        return false;
    } else {
        return true;
    }
}

char settings_get_decimal_separator()
{
    char ch;
    if (setting_user_display_format.separator == SETTING_DECIMAL_SEPARATOR_PERIOD) {
        ch = '.';
    } else if (setting_user_display_format.separator == SETTING_DECIMAL_SEPARATOR_COMMA) {
        ch = ',';
    } else {
        ch = '.';
    }
    return ch;
}

char settings_get_unit_suffix()
{
    char ch;
    if (setting_user_display_format.unit == SETTING_DISPLAY_UNIT_FSTOP) {
        ch = 'F';
    } else {
        ch = 'D';
    }
    return ch;
}

HAL_StatusTypeDef settings_read_buffer(uint32_t address, uint8_t *data, size_t data_len)
{
    if (!IS_FLASH_DATA_ADDRESS(address)) {
        log_e("Invalid EEPROM address");
        return HAL_ERROR;
    }
    if (!IS_FLASH_DATA_ADDRESS((address + data_len) - 1)) {
        log_e("Invalid length");
        return HAL_ERROR;
    }
    if (!data || data_len == 0) {
        log_e("Invalid buffer");
        return HAL_ERROR;
    }

    for (size_t i = 0; i < data_len; i++) {
        data[i] = *(__IO uint8_t *)(address + i);
    }

    return HAL_OK;
}

HAL_StatusTypeDef settings_write_buffer(uint32_t address, const uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!IS_FLASH_DATA_ADDRESS(address)) {
        log_e("Invalid EEPROM address");
        return HAL_ERROR;
    }
    if (!IS_FLASH_DATA_ADDRESS((address + data_len) - 1)) {
        log_e("Invalid length");
        return HAL_ERROR;
    }
    if (!data || data_len == 0) {
        log_e("Invalid buffer");
        return HAL_ERROR;
    }

    ret = HAL_FLASHEx_DATAEEPROM_Unlock();
    if (ret != HAL_OK) {
        log_e("Unable to unlock EEPROM: %d", ret);
        return ret;
    }

    /* Clear all possible error flags */
    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR |
        FLASH_FLAG_OPTVERR | FLASH_FLAG_RDERR | FLASH_FLAG_FWWERR |
        FLASH_FLAG_NOTZEROERR);

    if (address % 4 == 0 && (data_len % 4) == 0) {
        /* If the buffer can be written in word-sized increments, doing that is a lot faster */
        for (size_t i = 0; i < data_len; i += 4) {
            ret = HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, address + i, *(uint32_t *)(data + i));
            if (ret != HAL_OK) {
                log_e("EEPROM write error: %d [%d]", ret, i);
                log_e("FLASH last error: %d", HAL_FLASH_GetError());
                break;
            }
        }
    } else {
        for (size_t i = 0; i < data_len; i++) {
            ret = HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_BYTE, address + i, data[i]);
            if (ret != HAL_OK) {
                log_e("EEPROM write error: %d [%d]", ret, i);
                log_e("FLASH last error: %d", HAL_FLASH_GetError());
                break;
            }
        }
    }
    HAL_FLASHEx_DATAEEPROM_Lock();
    return ret;
}

HAL_StatusTypeDef settings_erase_page(uint32_t address, size_t len)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!IS_FLASH_DATA_ADDRESS(address)) {
        log_e("Invalid EEPROM address");
        return HAL_ERROR;
    }
    if (!IS_FLASH_DATA_ADDRESS((address + len) - 1)) {
        log_e("Invalid length");
        return HAL_ERROR;
    }
    if (address % 4 != 0 || (len % 4) != 0) {
        log_e("Erase is not word aligned");
        return HAL_ERROR;
    }

    ret = HAL_FLASHEx_DATAEEPROM_Unlock();
    if (ret != HAL_OK) {
        log_e("Unable to unlock EEPROM: %d", ret);
        return ret;
    }

    /* Clear all possible error flags */
    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR |
        FLASH_FLAG_OPTVERR | FLASH_FLAG_RDERR | FLASH_FLAG_FWWERR |
        FLASH_FLAG_NOTZEROERR);

    log_d("Wiping page: 0x%08lX - 0x%08lX", address, (address + len) - 1);

    for (size_t i = 0; i < len; i += 4) {
        ret = HAL_FLASHEx_DATAEEPROM_Erase(address + i);
        if (ret != HAL_OK) {
            log_e("EEPROM write error: %d [%d]", ret, i);
            log_e("FLASH last error: %d", HAL_FLASH_GetError());
            break;
        }
    }
    HAL_FLASHEx_DATAEEPROM_Lock();
    return ret;
}

#if 0
float settings_read_float(uint32_t address)
{
    __IO uint32_t *data = (__IO uint32_t *)(address);
    return copy_to_f32((uint8_t *)data);
}

HAL_StatusTypeDef settings_write_float(uint32_t address, float val)
{
    uint8_t data[4];
    copy_from_f32(data, val);
    return settings_write_buffer(address, data, sizeof(data));
}
#endif

uint32_t settings_read_uint32(uint32_t address)
{
    __IO uint32_t *data = (__IO uint32_t *)(address);
    return copy_to_u32((uint8_t *)data);
}

HAL_StatusTypeDef settings_write_uint32(uint32_t address, uint32_t val)
{
    uint8_t data[4];
    copy_from_u32(data, val);
    return settings_write_buffer(address, data, sizeof(data));
}
