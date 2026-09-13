#include <anvi/app.h>
#include <anvi/text_buffer.h>

static int
number_in_text_buffer(struct anvi_text_buffer *tb) {
    if (tb->length_bytes > 7) {
        return -1; // No user wants to type a million characters...
    }
    int ret = 0;
    for (size_t i = 0; i < tb->length_bytes; ++i) {
        ret *= 10;
        if (tb->data[i] < '0' || tb->data[i] > '9') {
            return -1;
        }
        ret += tb->data[i] - '0';
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

void
anvi_app_handle_input(struct anvi_state *state, struct anvi_input *input) {
    // Most inputs do the same action no matter what phase.
    // Let's thus first match on input->type and on some cases
    // have the handler function check the phase.
    struct anvi_text_buffer *current_tb = get_currently_active_text_buffer(state);
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
