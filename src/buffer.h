#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

#include <wayland-client.h>

struct anvi_state;
struct anvi_output;

int create_and_bind_wl_shm(struct anvi_state* state, struct wl_registry *registry, uint32_t name, uint32_t bind_version);
int draw_initial_lock_screen(struct anvi_state *state, struct anvi_output *output, uint32_t width, uint32_t height);

#endif
