#include <string.h>

#include <wayland-client.h>

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/output.h>
#include <anvi/buffer.h>

static void
render_text_to_buffer(struct anvi_state *state, struct anvi_buffer *buffer) {

    // Clear the buffer.
    memset(buffer->data, 0, buffer->size);

    cairo_t *cr = cairo_create(buffer->cairo_surface);

    anvi_log_info("Rendering text to buffer...\n");
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, 50, 80);
    cairo_set_font_size(cr, 34);
    cairo_show_text(cr, state->text_buffer->data);

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
    render_text_to_buffer(state, free_buffer);
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
