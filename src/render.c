#include <string.h>

#include <cairo.h>
#include <pango/pangocairo.h>
#include <wayland-client.h>

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/output.h>
#include <anvi/buffer.h>
#include <anvi/text_buffer.h>
#include <anvi/pangocairo_adapter.h>

static constexpr size_t MARGIN_WIDTH = 50;
static constexpr size_t PADDING_TOP = 40;
static constexpr size_t PADDING_BOTTOM = 20;



static void
draw_word_counter(struct anvi_state *state, cairo_t *cr, PangoLayout *layout) {
    const size_t wc = anvi_text_buffer_word_count(state->document); 

    char text[15];
    sprintf(text, "%ld / %ld", wc, state->words_to_exit);
    anvi_pangocairo_draw_text_starting_at(cr, layout, text, 10, 10);
    pango_layout_set_text(layout, text, -1);
}

static void
draw_enough_words_text_button(cairo_t *cr, PangoLayout *layout) {
    char text[] = "Click here or ctrl + q to finish.";
    anvi_pangocairo_draw_text_starting_at(cr, layout, text, 100, 10);
    pango_layout_set_text(layout, text, -1);
}

static void
setup_pango_layout_for_current_output(struct anvi_output *output, PangoLayout *layout) {

    pango_layout_set_width(layout, (output->width - 2 * MARGIN_WIDTH) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
}

static constexpr size_t MAX_COMBINED_LEN = 256; // Our prompt + user typed word count can never be longer than this.
typedef struct {
    char data[MAX_COMBINED_LEN];
    size_t prompt_text_length;
} start_string_combined;

static start_string_combined
construct_start_configuration_phase_text(struct anvi_state *state) {
    char text_prompt[] = "Enter how many words you must type before unlocking, or q to quit: "; 

    start_string_combined result;
    result.prompt_text_length = strlen(text_prompt);

    const size_t available = MAX_COMBINED_LEN - strlen(text_prompt) - 1;
    snprintf(
        result.data,
        MAX_COMBINED_LEN,
        "%s%.*s",
        text_prompt,
        (int)available,
        state->prompt_input->data // TODO: give only the text as argument to this function
    );

    if (state->start_phase_include_invalid_input_text) {
        // TODO: put the actual max number.
        char invalid_text_warning[] = "\nPlease enter an integer between 1 and 999999.";
        const size_t used = strlen(result.data);
        const size_t remaining = MAX_COMBINED_LEN - used;
        snprintf(
            result.data + used,
            remaining,
            "%s",
            invalid_text_warning
        );
    } 

    return result;
}

constexpr size_t MAX_FINISH_TEXT_COMBINED_LEN = 256;
typedef struct {
    char data[MAX_FINISH_TEXT_COMBINED_LEN];
} finish_string_combined;

static finish_string_combined
construct_finish_phase_text(struct anvi_state *state) {

    char congrats_prompt_start[] = "Congratulations, you wrote ";
    char congrats_prompt_end[] = " words!\n";
    char instruction_prompt[] = "Type q to quit, or a new, higher word count to continue: ";

    finish_string_combined result;

    const size_t space_left_for_text_buffer_data = MAX_FINISH_TEXT_COMBINED_LEN - sizeof(congrats_prompt_start) - sizeof(congrats_prompt_end) - sizeof(instruction_prompt) - 10;

    snprintf(
        result.data,
        MAX_FINISH_TEXT_COMBINED_LEN,
        "%s%zu%s%s%.*s",
        congrats_prompt_start,
        anvi_text_buffer_word_count(state->document),
        congrats_prompt_end,
        instruction_prompt,
        (int)space_left_for_text_buffer_data,
        state->prompt_input->data
    );

    return result;
}


static void
draw_start_configuration_phase(struct anvi_state *state, cairo_t *cr) {

    start_string_combined combined = construct_start_configuration_phase_text(state);

    anvi_pangocairo_draw_text_starting_at(cr, state->layout, combined.data, 10, 10);
    anvi_pangocairo_draw_cursor(combined.prompt_text_length + state->prompt_input->cursor_bytes, cr, state->layout, 10, 10);
}

static double
calculate_scroll_y(const struct anvi_state *state, PangoLayout* layout, double current_scroll_y, const double viewport_height) {

    pango_layout_set_text(layout, state->document->data, -1);
    PangoRectangle cursor;
    pango_layout_get_cursor_pos(layout, (int)state->document->cursor_bytes, &cursor, NULL);

    const double cursor_top = cursor.y / (double)PANGO_SCALE;
    const double cursor_height = cursor.height / (double)PANGO_SCALE;
    const double cursor_bottom = cursor_top + cursor_height;

    if (cursor_bottom > current_scroll_y + viewport_height) {
        current_scroll_y = cursor_bottom - viewport_height;
    }

    if (cursor_top < current_scroll_y) {
        current_scroll_y = cursor_top;
    }

    if (current_scroll_y < 0) {
        current_scroll_y = 0.0;
    }
    return current_scroll_y;
}

static void
draw_document(struct anvi_state *state, struct anvi_output *output, cairo_t *cr) {

    PangoLayout *layout = state->layout;

    const double top = PADDING_TOP;
    const double bottom_padding = PADDING_BOTTOM;
    const double viewport_height = (double)output->height - top - bottom_padding;
    const double viewport_width = (double)output->width - 2.0 * MARGIN_WIDTH;

    if (viewport_height <= 0 || viewport_width <= 0) {
        return;
    }

    output->scroll_y = calculate_scroll_y(state, layout, output->scroll_y, viewport_height);

    cairo_save(cr);

    cairo_rectangle(cr, MARGIN_WIDTH, top, viewport_width, viewport_height);
    cairo_clip(cr);

    cairo_translate(cr, MARGIN_WIDTH, top - output->scroll_y);

    anvi_pangocairo_draw_text_starting_at(cr, state->layout, state->document->data, 0, 0);
    anvi_pangocairo_draw_cursor(state->document->cursor_bytes, cr, state->layout, 0, 0);

    cairo_restore(cr);
}

static void
draw_normal_phase(struct anvi_state *state, struct anvi_output *output, cairo_t *cr) {

    draw_document(state, output, cr);
    draw_word_counter(state, cr, state->layout);

    const size_t wc = anvi_text_buffer_word_count(state->document);
    if (wc >= state->words_to_exit) {
        draw_enough_words_text_button(cr, state->layout);
    }
}

static void
draw_finish_phase(struct anvi_state *state, cairo_t *cr) {
    finish_string_combined combined = construct_finish_phase_text(state);
    anvi_pangocairo_draw_text_starting_at(cr, state->layout, combined.data, 10, 10);
}

static void
create_or_update_pango_layout(struct anvi_state *state, cairo_t *cr) {
    if (state->layout == NULL) {
        state->layout = pango_cairo_create_layout(cr);
        PangoFontDescription *font = pango_font_description_from_string("Sans 16");
        pango_layout_set_font_description(state->layout, font);
        pango_font_description_free(font);
    } else {
        pango_cairo_update_layout(cr, state->layout);
    }
}

static void
render_to_buffer(struct anvi_state *state, struct anvi_output *output, struct anvi_buffer *buffer) {

    // Clear the buffer.
    memset(buffer->data, 0, buffer->size);

    cairo_t *cr = cairo_create(buffer->cairo_surface);

    create_or_update_pango_layout(state, cr);
    setup_pango_layout_for_current_output(output, state->layout);

    switch (state->phase) {
        case ANVI_START_CONFIGURATION_PHASE:
            draw_start_configuration_phase(state, cr);
            break;
        case ANVI_NORMAL_PHASE:
            draw_normal_phase(state, output, cr);
            break;
        case ANVI_FINISHED_PHASE:
            draw_finish_phase(state, cr);
            break;
    }
    
    cairo_destroy(cr);
    cairo_surface_flush(buffer->cairo_surface);

    anvi_log_info("Cairo destroyed and surface flushed");
}


void
draw_screen(struct anvi_state *state, struct anvi_output *output) {
    // First we must find a non-busy buffer:
    struct anvi_buffer *free_buffer = find_free_buffer(output);
    if (free_buffer == NULL) {
        anvi_log_error("Could not find a free buffer to draw to...");
        return;
    }
    free_buffer->busy = true;
    render_to_buffer(state, output, free_buffer);
    present_buffer(output, free_buffer->proxy);
}

static const struct wl_callback_listener wl_surface_frame_listener;

static void
surface_frame_done(void *data, struct wl_callback *cb, uint32_t time) {
    (void)time;
    wl_callback_destroy(cb);

    struct anvi_output *output = data;
    cb = wl_surface_frame(output->surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, output);

    draw_screen(output->state, output);
}

static const struct wl_callback_listener wl_surface_frame_listener = {
    .done = surface_frame_done,
};

void
add_surface_frame_listeners_for_outputs(struct anvi_state *state) {
    for (struct anvi_output *output = state->outputs; output != NULL; output = output->next) {
        struct wl_callback *cb = wl_surface_frame(output->surface);
        wl_callback_add_listener(cb, &wl_surface_frame_listener, output);
    }
}
