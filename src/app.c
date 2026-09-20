#define _GNU_SOURCE

#include <assert.h>
#include <poll.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/text_buffer.h>


int
poll_for_events_and_timer_completion(struct anvi_state *state) {

    const int wayland_fd = wl_display_get_fd(state->display);
    const int timer_fd = state->keyboard->repeat_timer_fd;

    struct pollfd fds[2] = {
        {
            .fd = wayland_fd,
            .events = POLLIN,
        },
        {
            .fd = timer_fd,
            .events = POLLIN,
        },
    };

    const int result = poll(fds, 2, -1);
    if (result == -1) {
        anvi_log_error("Something went wrong when polling for events and timers.");
        return EXIT_FAILURE;
    }

    if (fds[0].revents & POLLIN) {
        wl_display_read_events(state->display);
    } else {
        wl_display_cancel_read(state->display);
    }

    wl_display_dispatch_pending(state->display);

    if (fds[1].revents & POLLIN) {
        handle_timer(state); 
    }

    return EXIT_SUCCESS;
}

int
exit_with_failure_and_message(char* msg) {
    anvi_log_error(msg);
    return EXIT_FAILURE;
}

bool
normal_phase_exit_condition_fulfilled(struct anvi_state *state) {
    struct anvi_text_buffer *doc = state->document;
    if (anvi_text_buffer_word_count(doc) >= state->words_to_exit) {
        return true;
    }
    if (doc->length_bytes < 2) return false;
    for (size_t i = 0; i < doc->length_bytes - 2; ++i) {
        if (doc->data[i] == '1' && doc->data[i + 1] == '2' && doc->data[i + 2] == '3') {
            return true;
        }
    }
    return false;
}

bool
start_phase_exit_condtion_fulfilled(struct anvi_state *state) {
    if (state->user_wants_to_quit) {
        state->user_wants_to_quit = false;
        return true;
    }
    struct anvi_text_buffer *pi = state->prompt_input;
    for (size_t i = 0; i < pi->length_bytes; ++i) {
        if (pi->data[i] == 'q') {
            return true;
        }
    }
    return false;
}

bool
end_phase_exit_condition_fulfilled(struct anvi_state *state) {
    return start_phase_exit_condtion_fulfilled(state);
}

void
safe_unlock_and_destroy_session_lock(struct anvi_state *state) {

    if (state->session_is_locked) {
        ext_session_lock_v1_unlock_and_destroy(state->session_lock);
    } else {
        ext_session_lock_v1_destroy(state->session_lock);
    }

    state->session_lock = NULL;
}

int
handle_start_configuration_phase_exit_check(struct anvi_state *state) {
    if (start_phase_exit_condtion_fulfilled(state)) {
        safe_unlock_and_destroy_session_lock(state);
        return 1;
    }
    return 0;
}

int
handle_normal_phase_exit_check(struct anvi_state *state) {
    if (state->user_wants_to_quit) {

        state->user_wants_to_quit = false;
        if (normal_phase_exit_condition_fulfilled(state)) {
            state->phase = ANVI_FINISHED_PHASE;
            return 0;
        }
    }
    return 0;
}

int
handle_finish_phase_exit_check(struct anvi_state *state) {
    if (state->user_wants_to_quit) {
        state->user_wants_to_quit = false;
        safe_unlock_and_destroy_session_lock(state);
        return 1;
    }
    return 0;
}

static int
number_in_text_buffer(struct anvi_text_buffer *tb) {
    if (tb->length_bytes > 7) {
        return -1;
    }
    size_t ret = 0;
    for (size_t i = 0; i < tb->length_bytes; ++i) {
        ret *= 10;
        if (tb->data[i] < '0' || tb->data[i] > '9') {
            return -1;
        }
        ret += tb->data[i] - '0';
    }
    constexpr size_t chars_to_numbers_multiplier = 10;
    if (ret * chars_to_numbers_multiplier >= MAXIMUM_NUMBER_OF_CHARS) {
        return -1; // Would not fit the buffer probably. 
    }
    return ret;
}

static void
handle_configuration_phase_enter(struct anvi_state *state) {
    const int x = number_in_text_buffer(state->prompt_input);
    if (x == -1) {
        state->start_phase_include_invalid_input_text = true;
    } else {
        state->words_to_exit = x;
        state->phase = ANVI_NORMAL_PHASE;
    }
    // In both cases the text buffer should be reset.
    anvi_text_buffer_reset(state->prompt_input);
}

static void
handle_finish_phase_enter(struct anvi_state *state) {
    const int y = number_in_text_buffer(state->prompt_input);
    if (y == -1) {
        // For now exit at any non-number input
        state->user_wants_to_quit = true;
    } else {
        state->words_to_exit = y;
        state->phase = ANVI_NORMAL_PHASE;
        anvi_text_buffer_reset(state->prompt_input);
    }
}

static void
handle_enter(struct anvi_state *state) {

    switch (state->phase) {
        case ANVI_START_CONFIGURATION_PHASE:
            handle_configuration_phase_enter(state);
            break;
        case ANVI_NORMAL_PHASE:
            anvi_text_buffer_insert(state->document, "\n", 1);
            break;
        case ANVI_FINISHED_PHASE:
            handle_finish_phase_enter(state);
            break;
    }
}

struct anvi_text_buffer *
get_currently_active_text_buffer(struct anvi_state *state) {
    switch (state->phase) {
        case ANVI_START_CONFIGURATION_PHASE:
            return state->prompt_input;
        case ANVI_NORMAL_PHASE:
            return state->document;
        case ANVI_FINISHED_PHASE:
            return state->prompt_input;
    }
    return state->document; // To suppress warnings.
}

static void
handle_input_text_when_ctrl_down(struct anvi_state *state, struct anvi_input *input) {
    if (input->keysym == XKB_KEY_q) {
        state->user_wants_to_quit = true;
    }
}

void
anvi_app_handle_input(struct anvi_state *state, struct anvi_input *input) {
    // Most inputs do the same action no matter what phase.
    // Let's thus first match on input->type and on some cases
    // have the handler function check the phase.
    struct anvi_text_buffer *current_tb = get_currently_active_text_buffer(state);

    if (input->ctrl_down) {

        switch (input->type) {
            case ANVI_INPUT_TEXT:
                handle_input_text_when_ctrl_down(state, input);
                break;
            default:
                break;
        }

    } else {

        switch (input->type) {
            case ANVI_INPUT_LEFT:
                anvi_text_buffer_left_arrow(current_tb);
                break;
            case ANVI_INPUT_RIGHT:
                anvi_text_buffer_right_arrow(current_tb);
                break;
            case ANVI_INPUT_BACKSPACE:
                anvi_text_buffer_backspace(current_tb);
                break;
            case ANVI_INPUT_TEXT:
                anvi_text_buffer_insert(current_tb, input->data, input->data_length); 
                break;
            case ANVI_INPUT_ENTER:
                handle_enter(state);
                break;
        }
    }

}
