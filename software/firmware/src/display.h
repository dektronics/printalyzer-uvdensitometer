#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include "stm32l0xx_hal.h"

typedef enum {
    DISPLAY_MODE_VIS_REFLECTION,
    DISPLAY_MODE_VIS_TRANSMISSION,
    DISPLAY_MODE_UV_TRANSMISSION
} display_mode_t;

typedef struct {
    const char *title;
    display_mode_t mode;
    uint8_t frame;
    int16_t density100;
    char decimal_sep;
    bool zero_indicator;
    bool f_indicator;
} display_main_elements_t;

HAL_StatusTypeDef display_init(SPI_HandleTypeDef *hspi);

void display_clear();
void display_enable(bool enabled);
void display_set_contrast(uint8_t value);
uint8_t display_get_contrast();

void display_capture_screenshot();

void display_draw_test_pattern(bool mode);
void display_static_list(const char *title, const char *list);
void display_static_message(const char *msg);
uint8_t display_selection_list(const char *title, uint8_t start_pos, const char *list);
uint8_t display_message(const char *title1, const char *title2, const char *title3, const char *buttons);
uint8_t display_input_value_f1_2(const char *title, const char *pre, uint16_t *value, uint16_t lo, uint16_t hi, char sep, const char *post);

void display_draw_main_elements(const display_main_elements_t *elements);

#endif /* DISPLAY_H */
