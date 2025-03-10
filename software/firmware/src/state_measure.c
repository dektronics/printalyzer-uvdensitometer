#include "state_measure.h"

#define LOG_TAG "state_measure"

#include <stdbool.h>
#include <elog.h>

#include "keypad.h"
#include "display.h"
#include "light.h"
#include "densitometer.h"
#include "settings.h"

typedef struct {
    state_t base;
    bool vis_uv;
    bool display_dirty;
    bool take_measurement;
    densitometer_t *densitometer;
    state_identifier_t display_state;
    const char *display_title;
    display_mode_t display_mode;
} state_measure_t;

static void state_measure_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state);
static void state_reflection_measure_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state);
static void state_transmission_measure_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state);

static void state_measure_process(state_t *state_base, state_controller_t *controller);

static state_measure_t state_vis_reflection_measure_data = {
    .base = {
        .state_entry = state_reflection_measure_entry,
        .state_process = state_measure_process,
        .state_exit = NULL
    },
    .vis_uv = true,
    .display_dirty = true,
    .take_measurement = true,
    .densitometer = NULL,
    .display_state = STATE_VIS_REFLECTION_DISPLAY,
    .display_title = "Reflection",
    .display_mode = DISPLAY_MODE_VIS_REFLECTION
};

static state_measure_t state_vis_transmission_measure_data = {
    .base = {
        .state_entry = state_transmission_measure_entry,
        .state_process = state_measure_process,
        .state_exit = NULL
    },
    .vis_uv = true,
    .display_dirty = true,
    .take_measurement = true,
    .densitometer = NULL,
    .display_state = STATE_VIS_TRANSMISSION_DISPLAY,
    .display_title = "Transmission",
    .display_mode = DISPLAY_MODE_VIS_TRANSMISSION
};

static state_measure_t state_uv_transmission_measure_data = {
    .base = {
        .state_entry = state_transmission_measure_entry,
        .state_process = state_measure_process,
        .state_exit = NULL
    },
    .vis_uv = false,
    .display_dirty = true,
    .take_measurement = true,
    .densitometer = NULL,
    .display_state = STATE_UV_TRANSMISSION_DISPLAY,
    .display_title = "Transmission",
    .display_mode = DISPLAY_MODE_UV_TRANSMISSION
};

static void sensor_read_callback(void *user_data);

state_t *state_vis_reflection_measure()
{
    return (state_t *)&state_vis_reflection_measure_data;
}

state_t *state_vis_transmission_measure()
{
    return (state_t *)&state_vis_transmission_measure_data;
}

state_t *state_uv_transmission_measure()
{
    return (state_t *)&state_uv_transmission_measure_data;
}

void state_measure_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state)
{
    state_measure_t *state = (state_measure_t *)state_base;

    state->display_dirty = true;
    state->take_measurement = true;
}

void state_reflection_measure_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state)
{
    state_measure_entry(state_base, controller, prev_state);

    state_measure_t *state = (state_measure_t *)state_base;
    state->densitometer = densitometer_vis_reflection();
}

void state_transmission_measure_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state)
{
    state_measure_entry(state_base, controller, prev_state);

    state_measure_t *state = (state_measure_t *)state_base;
    state->densitometer = state->vis_uv ? densitometer_vis_transmission() : densitometer_uv_transmission();
}

void state_measure_process(state_t *state_base, state_controller_t *controller)
{
    state_measure_t *state = (state_measure_t *)state_base;

    settings_user_display_format_t display_format;
    settings_get_user_display_format(&display_format);

    char sep = settings_get_decimal_separator();
    display_main_elements_t elements = {
        .title = "Measuring...",
        .mode = state->display_mode,
        .density100 = 0,
        .decimal_sep = sep,
        .frame = 0,
        .f_indicator = (display_format.unit == SETTING_DISPLAY_UNIT_FSTOP)
    };

    if (state->take_measurement) {
        display_draw_main_elements(&elements);

        densitometer_result_t result = densitometer_measure(state->densitometer, sensor_read_callback, &elements);
        if (result == DENSITOMETER_CAL_ERROR) {
            display_static_list(state->display_title,
                "Invalid\n"
                "calibration");
            osDelay(2000);
            state_controller_set_next_state(controller, state->display_state);
        } else if (result == DENSITOMETER_SENSOR_ERROR) {
            display_static_list(state->display_title,
                "Sensor\n"
                "read error");
            osDelay(2000);
            state_controller_set_next_state(controller, state->display_state);
        } else {
            state->take_measurement = false;
            state->display_dirty = true;
        }
    } else {
        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == osOK) {
            if (!keypad_is_key_pressed(&keypad_event, KEYPAD_BUTTON_ACTION)) {
                /* Return to the display state if the measure button was released */
                state_controller_set_next_state(controller, state->display_state);
            }
        }

        if (state->display_dirty) {
            float reading;
            if (display_format.unit == SETTING_DISPLAY_UNIT_FSTOP) {
                reading = densitometer_get_display_f(state->densitometer);
            } else {
                reading = densitometer_get_display_d(state->densitometer);
            }

            bool has_zero = !isnan(densitometer_get_zero_d(state->densitometer));
            elements.density100 = (!isnan(reading)) ? lroundf(reading * 100) : 0;
            elements.zero_indicator = has_zero;
            display_draw_main_elements(&elements);
            state->display_dirty = false;
        }
    }
}

void sensor_read_callback(void *user_data)
{
    display_main_elements_t *elements = (display_main_elements_t *)user_data;
    elements->frame++;
    if (elements->frame > 2) { elements->frame = 0; }
    display_draw_main_elements(elements);
}
