#ifndef APP_H
#define APP_H

#include <pango/pangocairo.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "ext-session-lock-v1-client-protocol.h"

#include <anvi/text_buffer.h>
#include <anvi/keyboard.h>
#include <anvi/storage.h>

struct anvi_output;

enum anvi_phase {
    ANVI_START_CONFIGURATION_PHASE = 1,
    ANVI_NORMAL_PHASE = 2,
    ANVI_FINISHED_PHASE = 3,
};

enum anvi_input_type {
    ANVI_INPUT_TEXT,
    ANVI_INPUT_ENTER,
    ANVI_INPUT_BACKSPACE,
    ANVI_INPUT_LEFT,
    ANVI_INPUT_RIGHT,
};

constexpr size_t ANVI_INPUT_DATA_CAPACITY = 64;
struct anvi_input {
    enum anvi_input_type type;
    bool ctrl_down;
    xkb_keysym_t keysym;
    char data[ANVI_INPUT_DATA_CAPACITY];
    size_t data_length;
};

struct anvi_state {
    struct ext_session_lock_manager_v1 *session_lock_manager;
    struct ext_session_lock_v1 *session_lock;
    struct wl_compositor *wl_compositor;
    struct anvi_output *outputs;
    struct wl_shm *wl_shm;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_seat *seat;
    struct anvi_keyboard *keyboard;

    struct anvi_text_buffer *prompt_input;
    struct anvi_text_buffer *document;

    struct anvi_storage *storage; 

    PangoLayout *layout;

    enum anvi_phase phase;
    bool start_phase_include_invalid_input_text;
    bool user_wants_to_quit;

    size_t words_to_exit;

    bool initialization_failed;
    bool session_is_locked;
    bool session_is_finished;
};

void anvi_app_handle_input(struct anvi_state *state, struct anvi_input *input);
bool normal_phase_exit_condition_fulfilled(struct anvi_state *state);
bool start_phase_exit_condtion_fulfilled(struct anvi_state *state);
bool end_phase_exit_condition_fulfilled(struct anvi_state *state);
int exit_with_failure_and_message(char* msg);
int handle_finish_phase_exit_check(struct anvi_state *state);
int handle_normal_phase_exit_check(struct anvi_state *state);
int handle_start_configuration_phase_exit_check(struct anvi_state *state);
void safe_unlock_and_destroy_session_lock(struct anvi_state *state);
int poll_for_events_and_timer_completion(struct anvi_state *state);
#endif
