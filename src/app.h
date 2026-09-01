#ifndef APP_H
#define APP_H

#include <inttypes.h>

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"

#include "keyboard.h"


struct anvi_state;

struct anvi_output {
    struct anvi_state *state;

    uint32_t registry_name;
    struct wl_output *proxy;

    struct wl_surface *surface;
    struct ext_session_lock_surface_v1 *lock_surface; 
    struct wl_buffer *buffer;

    uint32_t width;
    uint32_t height;

    struct anvi_output *next;
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
    bool initialization_failed;
    bool session_is_locked;
    bool session_is_finished;
};

#endif
