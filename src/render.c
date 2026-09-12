#include <string.h>

#include <cairo.h>
#include <pango/pangocairo.h>
#include <wayland-client.h>

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/output.h>
#include <anvi/buffer.h>
#include <anvi/text_buffer.h>

static constexpr size_t MARGIN_WDITH = 50;
static constexpr size_t PADDING_TOP = 40;

static void
draw_cursor_at_pos_with_height(double x, double y, double h, cairo_t *cr) {
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x, y + h);
    cairo_stroke(cr);
}

static void
draw_cursor(size_t cursor_bytes, cairo_t *cr, PangoLayout *layout, size_t dx, size_t dy) {

    PangoRectangle pos;
    pango_layout_get_cursor_pos(layout, cursor_bytes, &pos, NULL);

    const double x = dx + pos.x / (double)PANGO_SCALE;
    const double y = dy + pos.y / (double)PANGO_SCALE;
    const double h = pos.height / (double)PANGO_SCALE;
    draw_cursor_at_pos_with_height(x, y, h, cr);
}


static void
draw_text_starting_at(cairo_t *cr, PangoLayout *layout, char* text, const size_t x, const size_t y) {

    pango_layout_set_text(layout, text, -1);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
}

static void
draw_word_counter(struct anvi_state *state, cairo_t *cr, PangoLayout *layout) {
    const size_t wc = anvi_text_buffer_word_count(state->text_buffer); 

    char text[15];
    sprintf(text, "%ld / %ld", wc, state->words_to_exit);
    draw_text_starting_at(cr, layout, text, 10, 10);
    pango_layout_set_text(layout, text, -1);
}

static void
setup_pango_layout_for_current_output(struct anvi_output *output, PangoLayout *layout) {

    pango_layout_set_width(layout, (output->width - 2 * MARGIN_WDITH) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
}


static void
draw_start_configuration_phase(struct anvi_state *state, cairo_t *cr) {

    char text_prompt[] = "Enter how many words you must before unlocking, or q to quit: "; 
    constexpr size_t MAX_COMBINED_LEN = 256; // Our prompt + user typed word count can never be longer than this.
    char combined_text[MAX_COMBINED_LEN];

    strcpy(combined_text, text_prompt);
    strcpy(combined_text + strlen(text_prompt), state->text_buffer->data);

    draw_text_starting_at(cr, state->layout, combined_text, 10, 10);
    draw_cursor(strlen(text_prompt) + state->text_buffer->cursor_bytes, cr, state->layout, 10, 10);
}

static void
draw_normal_phase(struct anvi_state *state, cairo_t *cr) {
    draw_text_starting_at(cr, state->layout, state->text_buffer->data, MARGIN_WDITH, PADDING_TOP);
    draw_cursor(state->text_buffer->cursor_bytes, cr, state->layout, MARGIN_WDITH, PADDING_TOP);
    draw_word_counter(state, cr, state->layout);
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
            draw_normal_phase(state, cr);
            break;
        case ANVI_FINISHED_PHASE:
            break;
    }
    
    cairo_destroy(cr);
    cairo_surface_flush(buffer->cairo_surface);
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
