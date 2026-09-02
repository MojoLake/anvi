#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

#include <wayland-client.h>

struct anvi_state;

int create_and_bind_wl_shm(struct anvi_state* state, struct wl_registry *registry, uint32_t name, uint32_t bind_version);

#endif
