#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#include <cairo.h>
#include <wayland-client.h>

struct anvi_state;
struct anvi_output;

struct anvi_buffer {
    struct wl_buffer *proxy;
    uint8_t *data;
    cairo_surface_t *cairo_surface;
    size_t size;
    bool busy;
};

int create_and_bind_wl_shm(struct anvi_state* state, struct wl_registry *registry, uint32_t name, uint32_t bind_version);
int setup_lock_screen(struct anvi_state *state, struct anvi_output *output);
void draw_screen(struct anvi_state *state, struct anvi_output *output);

#endif
