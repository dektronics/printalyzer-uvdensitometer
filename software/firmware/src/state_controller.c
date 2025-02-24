#include "state_controller.h"

#define LOG_TAG "state_controller"
#include <elog.h>

#include "stm32l0xx_hal.h"
#include <printf.h>
#include <cmsis_os.h>

#include "state_home.h"
#include "state_display.h"
#include "state_measure.h"
#include "state_main_menu.h"
#include "state_remote.h"
#include "state_suspend.h"

struct __state_controller_t {
    state_identifier_t current_state;
    state_identifier_t next_state;
    state_identifier_t home_state;
};

static state_controller_t state_controller = {0};
static state_t *state_map[STATE_MAX] = {0};

void state_controller_init()
{
    state_controller.current_state = STATE_MAX;
    state_controller.next_state = STATE_HOME;
    state_controller.home_state = STATE_VIS_REFLECTION_DISPLAY;

    state_map[STATE_HOME] = state_home();
    state_map[STATE_VIS_REFLECTION_DISPLAY] = state_vis_reflection_display();
    state_map[STATE_VIS_REFLECTION_MEASURE] = state_vis_reflection_measure();
    state_map[STATE_VIS_TRANSMISSION_DISPLAY] = state_vis_transmission_display();
    state_map[STATE_VIS_TRANSMISSION_MEASURE] = state_vis_transmission_measure();
    state_map[STATE_UV_TRANSMISSION_DISPLAY] = state_uv_transmission_display();
    state_map[STATE_UV_TRANSMISSION_MEASURE] = state_uv_transmission_measure();
    state_map[STATE_MAIN_MENU] = state_main_menu();
    state_map[STATE_REMOTE] = state_remote();
    state_map[STATE_SUSPEND] = state_suspend();
}

void state_controller_loop()
{
    state_t *state = NULL;
    for (;;) {
        /* Check if we need to do a state transition */
        if (state_controller.next_state != state_controller.current_state) {
            /* Transition to the new state */
            log_i("State transition: %d -> %d",
                state_controller.current_state, state_controller.next_state);
            state_identifier_t prev_state = state_controller.current_state;
            state_controller.current_state = state_controller.next_state;

            if (state_controller.current_state < STATE_MAX && state_map[state_controller.current_state]) {
                state = state_map[state_controller.current_state];
            } else {
                state = NULL;
            }

            /* Call the state entry function */
            if (state && state->state_entry) {
                state->state_entry(state, &state_controller, prev_state);
            }
        }

        /* Call the process function for the state */
        if (state && state->state_process) {
            state->state_process(state, &state_controller);
        }

        /* Check if a thread notification should trigger a state transition */
        uint32_t flags = osThreadFlagsWait(0x7FFFFFFF, osFlagsWaitAny, 0);
        if ((flags & 0x80000000) == 0 && (flags & 0x40000000)) {
            uint32_t next_state = flags & 0x00FFFFFF;
            log_i("Notify switch to state: %d", next_state);
            if (next_state < STATE_MAX) {
                state_controller.next_state = next_state;
            }
        }

        /* Check if we will do a state transition on the next loop */
        if (state_controller.next_state != state_controller.current_state) {
            if (state && state->state_exit) {
                state->state_exit(state, &state_controller, state_controller.next_state);
            }
        }
    }
}

state_identifier_t state_controller_get_current_state(const state_controller_t *controller)
{
    if (!controller) { return STATE_MAX; }
    return controller->current_state;
}

void state_controller_set_next_state(state_controller_t *controller, state_identifier_t next_state)
{
    if (!controller) { return; }
    controller->next_state = next_state;
}

void state_controller_set_home_state(state_controller_t *controller, state_identifier_t home_state)
{
    if (!controller) { return; }
    controller->home_state = home_state;
}

state_identifier_t state_controller_get_home_state(state_controller_t *controller)
{
    if (!controller) { return STATE_MAX; }
    return controller->home_state;
}
