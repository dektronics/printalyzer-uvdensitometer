/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * -------------------------------------------------------------------------
 * Note: Code has been extensively modified from the initial example that
 * it is based upon.
 */

#include "tusb.h"
#include "stm32l0xx_hal.h"
#include "usb_descriptors.h"
#include "printf.h"
#include "settings.h"

/*
 * A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                          _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

/**
 * Device Descriptors
 */
static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    /*
     * Setting these descriptor fields to zero is required for the
     * composite device to work correctly on some hosts, and also
     * helps the CDC device get detected as something a little
     * more obviously named.
     */
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0x16D0,
    .idProduct          = 0x13E7,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

/**
 * Invoked when GET DEVICE DESCRIPTOR is received.
 *
 * @return a pointer to the descriptor
 */
uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

#ifndef __CDT_PARSER__
static uint8_t const desc_hid_report[] =
{
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))
};
#endif

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void) instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID
};

#define ITF_NUM_TOTAL1   2
#define ITF_NUM_TOTAL2   3

#define CONFIG_TOTAL_LEN1 (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
#define CONFIG_TOTAL_LEN2 (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82
#define EPNUM_HID         0x83

static uint8_t const desc_fs_configuration1[] = {
    /* Config number, interface count, string index, total length, attribute, power in mA */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL1, 0, CONFIG_TOTAL_LEN1, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 150),

    /* Interface number, string index, EP notification address and size, EP data address (out, in) and size. */
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

static uint8_t const desc_fs_configuration2[] = {
    /* Config number, interface count, string index, total length, attribute, power in mA */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL2, 0, CONFIG_TOTAL_LEN2, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 150),

    /* Interface number, string index, EP notification address and size, EP data address (out, in) and size. */
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

    /* Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval */
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 5, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 5)
};

/**
 * Invoked when GET CONFIGURATION DESCRIPTOR is received
 *
 * Descriptor contents must exist long enough for the transfer to complete
 *
 * @return pointer to the descriptor
 */
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; /* for multiple configurations */

    settings_user_usb_key_t usb_key;
    settings_get_user_usb_key(&usb_key);

    if (usb_key.enabled) {
        return desc_fs_configuration2;
    } else {
        return desc_fs_configuration1;
    }
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

/**
 * Array of pointer to string descriptors
 */
char const* string_desc_arr[] =
{
    (const char[]) { 0x09, 0x04 },     /*!< 0: Language: English (United States) (0x0409) */
    "Dektronics",                      /*!< 1: Manufacturer */
    "Printalyzer UV/VIS Densitometer", /*!< 2: Product */
    "123456789012",                    /*!< 3: Serials, should use chip ID */
    "CDC Interface",                   /*!< 4: CDC Interface */
    "HID Interface"                    /*!< 5: HID Interface */
};

static uint16_t _desc_str[32];

static void ascii_to_utf16(uint16_t *buf, const char *str, uint8_t len);
static void uint32_to_utf16(uint16_t *buf, uint32_t value, uint8_t len);

/**
 * Invoked when GET STRING DESCRIPTOR is received
 *
 * Descriptor contents must exist long enough for the transfer to complete
 *
 * @return pointer to the descriptor
 */
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count = 0;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == 3) {
        /* Transform the device's unique ID into a USB device serial number */
        const uint32_t deviceserial0 = HAL_GetUIDw0();
        const uint32_t deviceserial1 = HAL_GetUIDw1();
        const uint32_t deviceserial2 = HAL_GetUIDw2();
        uint32_to_utf16(_desc_str + 1, deviceserial0, 8);
        uint32_to_utf16(_desc_str + 9, deviceserial1, 8);
        uint32_to_utf16(_desc_str + 17, deviceserial2, 8);
        chr_count = 24;
    } else {
        /*
         * Note: the 0xEE index string is a Microsoft OS 1.0 Descriptor
         * https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
         */

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }

        const char* str = string_desc_arr[index];

        /* Cap at max char */
        chr_count = strlen(str);
        if (chr_count > 31 ) { chr_count = 31; }

        /* Convert ASCII string into UTF-16 */
        ascii_to_utf16(_desc_str + 1, str, chr_count);
    }

    /* First byte is length (including header), second byte is string type */
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2 * chr_count + 2);

    return _desc_str;
}

/**
 * Convert ASCII string into UTF-16
 */
void ascii_to_utf16(uint16_t *buf, const char *str, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = str[i];
    }
}

/**
 * Convert 32-bit integer into a UTF-16 hex string
 */
void uint32_to_utf16(uint16_t *buf, uint32_t value, uint8_t len)
{
    char str[9];
    sprintf_(str, "%02X%02X%02X%02X",
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 24) & 0xFF));
    ascii_to_utf16(buf, str, len);
}
