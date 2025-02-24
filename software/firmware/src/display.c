#include "display.h"

#include <printf.h>
#include <stdlib.h>
#include <string.h>

#include "u8g2_stm32_hal.h"
#include "u8g2.h"
#include "display_segments.h"
#include "display_assets.h"
#include "keypad.h"
#include "cdc_handler.h"
#include "util.h"

static u8g2_t u8g2;
static uint8_t display_contrast = 0x7F;
static bool menu_event_timeout = false;

#define MENU_TIMEOUT_MS 30000

/* Library function declarations */
void u8g2_DrawSelectionList(u8g2_t *u8g2, u8sl_t *u8sl, u8g2_uint_t y, const char *s);

static void display_set_freq(uint8_t value);

HAL_StatusTypeDef display_init(SPI_HandleTypeDef *hspi)
{
    /* Configure the SPI parameters for the STM32 HAL */
    u8g2_stm32_hal_init(hspi);

    /* Initialize the display driver */
    u8g2_Setup_ssd1306_128x64_noname_f(&u8g2, U8G2_R2, u8g2_stm32_spi_byte_cb, u8g2_stm32_gpio_and_delay_cb);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetContrast(&u8g2, display_contrast);

    /*
     * Slightly increase the display refresh frequency
     * Oscillator Frequency (A[7:4])
     * Display Clock Divide Ratio (D)(A[3:0])
     * Default = 0x80 (div=1, Fosc=8)
     */
    display_set_freq(0xF0);

    return HAL_OK;
}

void display_set_freq(uint8_t value)
{
    /* This command sequence is specific to the SSD1306 */
    u8x8_t *u8x8 = &(u8g2.u8x8);
    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x0D5);
    u8x8_cad_SendArg(u8x8, value);
    u8x8_cad_EndTransfer(u8x8);
}

void display_clear()
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);
}

void display_enable(bool enabled)
{
    u8g2_SetPowerSave(&u8g2, enabled ? 0 : 1);
}

void display_set_contrast(uint8_t value)
{
    u8g2_SetContrast(&u8g2, value);
    display_contrast = value;
}

uint8_t display_get_contrast()
{
    return display_contrast;
}

static void display_capture_screenshot_callback(const char *s)
{
    size_t len = strlen(s);
    if (s > 0) {
        cdc_write(s, len);
        watchdog_refresh();
    }
}

void display_capture_screenshot()
{
    u8g2_WriteBufferXBM(&u8g2, display_capture_screenshot_callback);
}

void display_draw_test_pattern(bool mode)
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);

    int h = u8g2_GetDisplayHeight(&u8g2);
    int w = u8g2_GetDisplayWidth(&u8g2);

    bool draw = mode;
    for (int y = 0; y < h; y += 16) {
        for (int x = 0; x < w; x += 16) {
            draw = !draw;
            if (draw) {
                u8g2_DrawBox(&u8g2, x, y, 16, 16);
            }
        }
        draw = !draw;
    }

    u8g2_SendBuffer(&u8g2);
}

static void display_prepare_menu_font()
{
    /*
     * This font can show 14 characters per line,
     * and 4 lines (including the title) in a list.
     */
    u8g2_SetFont(&u8g2, u8g2_font_pxplusibmvga9_tf);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
}

uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8)
{
    /*
     * This function should override a u8g2 framework function with the
     * same name, due to its declaration with the "weak" pragma.
     */

    keypad_event_t keypad_event;
    osStatus_t ret = keypad_wait_for_event(&keypad_event, MENU_TIMEOUT_MS);
    if (ret == osOK) {
        if (keypad_event.pressed) {
            switch (keypad_event.key) {
            case KEYPAD_BUTTON_ACTION:
                return U8X8_MSG_GPIO_MENU_SELECT;
            case KEYPAD_BUTTON_UP:
                return U8X8_MSG_GPIO_MENU_UP;
            case KEYPAD_BUTTON_DOWN:
                return U8X8_MSG_GPIO_MENU_DOWN;
            case KEYPAD_BUTTON_MENU:
                return U8X8_MSG_GPIO_MENU_HOME;
            case KEYPAD_FORCE_TIMEOUT:
                menu_event_timeout = true;
                return U8X8_MSG_GPIO_MENU_HOME;
            default:
                break;
            }
        }
    } else if (ret == osErrorTimeout) {
        menu_event_timeout = true;
        return U8X8_MSG_GPIO_MENU_HOME;
    }

    return 0;
}

void display_static_list(const char *title, const char *list)
{
    /*
     * Based off u8g2_UserInterfaceSelectionList() with changes to use
     * full frame buffer mode and to remove actual menu functionality.
     */

    display_prepare_menu_font();
    u8g2_ClearBuffer(&u8g2);

    u8sl_t u8sl;
    u8g2_uint_t yy;

    u8g2_uint_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;

    uint8_t title_lines = u8x8_GetStringLineCnt(title);
    uint8_t display_lines;

    if (title_lines > 0) {
        display_lines = (u8g2_GetDisplayHeight(&u8g2) - 3) / line_height;
        u8sl.visible = display_lines;
        u8sl.visible -= title_lines;
    }
    else {
        display_lines = u8g2_GetDisplayHeight(&u8g2) / line_height;
        u8sl.visible = display_lines;
    }

    u8sl.total = u8x8_GetStringLineCnt(list);
    u8sl.first_pos = 0;
    u8sl.current_pos = -1;

    u8g2_SetFontPosBaseline(&u8g2);

    yy = u8g2_GetAscent(&u8g2);
    if (title_lines > 0) {
        yy += u8g2_DrawUTF8Lines(&u8g2, 0, yy, u8g2_GetDisplayWidth(&u8g2), line_height, title);
        u8g2_DrawHLine(&u8g2, 0, yy - line_height - u8g2_GetDescent(&u8g2) + 1, u8g2_GetDisplayWidth(&u8g2));
        yy += 3;
    }
    u8g2_DrawSelectionList(&u8g2, &u8sl, yy, list);

    u8g2_SendBuffer(&u8g2);
}

void display_static_message(const char *msg)
{
    uint8_t height;
    uint8_t line_height;
    u8g2_uint_t pixel_height;
    u8g2_uint_t y;

    display_prepare_menu_font();

    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontPosBaseline(&u8g2);

    /* Calculate line height */
    line_height = u8g2_GetAscent(&u8g2);
    line_height -= u8g2_GetDescent(&u8g2);

    /* calculate overall height of the message box in lines*/
    height = u8x8_GetStringLineCnt(msg);

    /* Calculate the height in pixels */
    pixel_height = height;
    pixel_height *= line_height;

    /* Calculate offset from top */
    y = 0;
    if (pixel_height < u8g2_GetDisplayHeight(&u8g2)) {
        y = u8g2_GetDisplayHeight(&u8g2);
        y -= pixel_height;
        y /= 2;
    }
    y += u8g2_GetAscent(&u8g2);

    /* Draw the text */
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawUTF8Lines(&u8g2, 0, y, u8g2_GetDisplayWidth(&u8g2), line_height, msg);
    u8g2_SendBuffer(&u8g2);
}

uint8_t display_selection_list(const char *title, uint8_t start_pos, const char *list)
{
    display_prepare_menu_font();
    keypad_clear_events();
    menu_event_timeout = false;

    uint8_t option = u8g2_UserInterfaceSelectionList(&u8g2, title, start_pos, list);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_message(const char *title1, const char *title2, const char *title3, const char *buttons)
{
    display_prepare_menu_font();
    keypad_clear_events();
    menu_event_timeout = false;

    uint8_t option = u8g2_UserInterfaceMessage(&u8g2, title1, title2, title3, buttons);

    return menu_event_timeout ? UINT8_MAX : option;
}

static const char *display_f1_2toa(uint16_t v, char sep)
{
    static char buf[5];

    buf[0] = '0' + (v % 1000 / 100);
    buf[1] = sep;
    buf[2] = '0' + (v % 100 / 10);
    buf[3] = '0' + (v % 10);
    buf[4] = '\0';

    return buf;
}

uint8_t display_input_value_f1_2(const char *title, const char *pre, uint16_t *value, uint16_t lo, uint16_t hi, char sep, const char *post)
{
    /*
     * Based off u8g2_UserInterfaceInputValue() with changes to use
     * full frame buffer mode and to support the N.DD number format.
     */

    /* Do initial state setup */
    display_prepare_menu_font();
    keypad_clear_events();
    menu_event_timeout = false;

    uint8_t line_height;
    uint8_t height;
    u8g2_uint_t pixel_height;
    u8g2_uint_t y, yy;
    u8g2_uint_t pixel_width;
    u8g2_uint_t x, xx;

    uint16_t local_value = *value;
    uint8_t event;

    /* Explicitly constrain input values */
    if (hi > 999) { hi = 999; }
    if (lo > 0 && lo > hi) { lo = hi; }
    if (local_value < lo) { local_value = lo; }
    else if (local_value > hi) { local_value = hi; }

    /* Only horizontal strings are supported, so force this here */
    u8g2_SetFontDirection(&u8g2, 0);

    /* Force baseline position */
    u8g2_SetFontPosBaseline(&u8g2);

    /* Calculate line height */
    line_height = u8g2_GetAscent(&u8g2);
    line_height -= u8g2_GetDescent(&u8g2);

    /* Calculate overall height of the input value box */
    height = 1; /* value input line */
    height += u8x8_GetStringLineCnt(title);

    /* Calculate the height in pixels */
    pixel_height = height;
    pixel_height *= line_height;

    /* Calculate offset from top */
    y = 0;
    if (pixel_height < u8g2_GetDisplayHeight(&u8g2)) {
        y = u8g2_GetDisplayHeight(&u8g2);
        y -= pixel_height;
        y /= 2;
    }

    /* Calculate offset from left for the label */
    x = 0;
    pixel_width = u8g2_GetUTF8Width(&u8g2, pre);
    pixel_width += u8g2_GetUTF8Width(&u8g2, "0") * 4;
    pixel_width += u8g2_GetUTF8Width(&u8g2, post);
    if (pixel_width < u8g2_GetDisplayWidth(&u8g2)) {
        x = u8g2_GetDisplayWidth(&u8g2);
        x -= pixel_width;
        x /= 2;
    }

    /* Event loop */
    for(;;) {
        /* Render */
        u8g2_ClearBuffer(&u8g2);
        yy = y;
        yy += u8g2_DrawUTF8Lines(&u8g2, 0, yy, u8g2_GetDisplayWidth(&u8g2), line_height, title);
        xx = x;
        xx += u8g2_DrawUTF8(&u8g2, xx, yy, pre);
        xx += u8g2_DrawUTF8(&u8g2, xx, yy, display_f1_2toa(local_value, sep));
        u8g2_DrawUTF8(&u8g2, xx, yy, post);
        u8g2_SendBuffer(&u8g2);

        for(;;) {
            event = u8x8_GetMenuEvent(u8g2_GetU8x8(&u8g2));
            if (event == U8X8_MSG_GPIO_MENU_SELECT) {
                *value = local_value;
                return 1;
            } else if (event == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            } else if (event == U8X8_MSG_GPIO_MENU_NEXT || event == U8X8_MSG_GPIO_MENU_UP) {
                if (local_value >= hi) {
                    local_value = lo;
                } else {
                    local_value++;
                }
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_PREV || event == U8X8_MSG_GPIO_MENU_DOWN) {
                if (local_value <= lo) {
                    local_value = hi;
                } else {
                    local_value--;
                }
                break;
            }
        }
    }
}

static bool display_get_mode_icon(display_mode_t mode, asset_info_t *asset)
{
    if (!asset) { return false; }

    asset_name_t name;

    switch (mode) {
    case DISPLAY_MODE_VIS_REFLECTION:
    case DISPLAY_MODE_VIS_TRANSMISSION:
        name = ASSET_VIS_ICON;
        break;
    case DISPLAY_MODE_UV_TRANSMISSION:
        name = ASSET_UV_ICON;
        break;
    default:
        name = ASSET_MAX;
        break;
    }

    return display_asset_get(asset, name) == 1;
}

static bool display_get_main_icon(display_mode_t mode, uint8_t frame, asset_info_t *asset)
{
    if (!asset) { return false; }

    asset_name_t name = ASSET_MAX;

    if (mode == DISPLAY_MODE_VIS_REFLECTION) {
        switch (frame) {
        case 0:
            name = ASSET_REFLECTION_ICON_40;
            break;
        case 1:
            name = ASSET_REFLECTION_ICON_40_1;
            break;
        case 2:
            name = ASSET_REFLECTION_ICON_40_2;
            break;
        default:
            name = ASSET_REFLECTION_ICON_40;
            break;
        }
    } else if (mode == DISPLAY_MODE_VIS_TRANSMISSION || mode == DISPLAY_MODE_UV_TRANSMISSION) {
        switch (frame) {
        case 0:
            name = ASSET_TRANSMISSION_ICON_40;
            break;
        case 1:
            name = ASSET_TRANSMISSION_ICON_40_1;
            break;
        case 2:
            name = ASSET_TRANSMISSION_ICON_40_2;
            break;
        default:
            name = ASSET_TRANSMISSION_ICON_40;
            break;
        }
    }

    return display_asset_get(asset, name) == 1;
}

void display_draw_main_elements(const display_main_elements_t *elements)
{
    if (!elements) { return; }

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_pxplusibmvga9_tf);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    u8g2_uint_t x = u8g2_GetDisplayWidth(&u8g2) - 22;
    u8g2_uint_t y = 18;

    if (elements->density100 != INT16_MAX) {
        int d100 = abs(elements->density100);
        if (d100 > 999) { d100 = 999; }

        display_draw_mdigit(&u8g2, x, y, d100 % 10);
        x -= 22;

        display_draw_mdigit(&u8g2, x, y, d100 % 100 / 10);
        x -= 8;

        if (elements->decimal_sep == '.') {
            u8g2_DrawBox(&u8g2, x, y + 33, 4, 4);
        } else if (elements->decimal_sep == ',') {
            u8g2_DrawBox(&u8g2, x, y + 36, 2, 3);
            u8g2_DrawBox(&u8g2, x + 1, y + 34, 2, 3);
            u8g2_DrawBox(&u8g2, x + 2, y + 32, 2, 3);
        }
        x -= 22;

        display_draw_mdigit(&u8g2, x, y, d100 % 1000 / 100);
        x -= 12;

        if (elements->density100 < 0) {
            u8g2_DrawLine(&u8g2, x + 1, y + 17, x + 8, y + 17);
            u8g2_DrawLine(&u8g2, x, y + 18, x + 9, y + 18);
            u8g2_DrawLine(&u8g2, x + 1, y + 19, x + 8, y + 19);
        }

        if (elements->f_indicator) {
            u8g2_DrawUTF8(&u8g2,
                (x - u8g2_GetMaxCharWidth(&u8g2)) + 1,
                (y + u8g2_GetAscent(&u8g2)) - 3,
                "f/");
        }
    }

    asset_info_t asset;
    if (display_get_main_icon(elements->mode, elements->frame, &asset)) {
        u8g2_DrawXBM(&u8g2, 0, y - 1, asset.width, asset.height, asset.bits);
    }

    if (elements->title) {
        u8g2_DrawUTF8(&u8g2, 0, u8g2_GetAscent(&u8g2), elements->title);
    }

    if (display_get_mode_icon(elements->mode, &asset)) {
        u8g2_DrawXBM(&u8g2, u8g2_GetDisplayWidth(&u8g2) - asset.width - 1, 0, asset.width, asset.height, asset.bits);
    }

    if (elements->zero_indicator) {
        display_asset_get(&asset, ASSET_ZERO_INDICATOR);
        u8g2_DrawXBM(&u8g2,
            41, 43,
            asset.width, asset.height, asset.bits);
    }

    u8g2_SendBuffer(&u8g2);
}
