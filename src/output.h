#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdint.h>
#include <stdbool.h>

#include <cairo.h>
#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"

struct anvi_state;

struct anvi_output {
    struct anvi_state *state;

    uint32_t registry_name;
    struct wl_output *proxy;

    struct wl_surface *surface;
    struct ext_session_lock_surface_v1 *lock_surface; 
    struct wl_buffer *buffer;

    cairo_surface_t *cairo_surface;
    cairo_t *cr;

    uint32_t width;
    uint32_t height;

    struct anvi_output *next;
};

void destroy_outputs(struct anvi_state *state);
int create_and_bind_anvi_output(struct anvi_state* state, struct wl_registry *registry, uint32_t name, uint32_t bind_version);
bool remove_anvi_output(struct anvi_state *state, uint32_t registry_name);
int create_surfaces_for_outputs(struct anvi_state *state);

extern const struct ext_session_lock_surface_v1_listener lock_surface_listener;

#endif
