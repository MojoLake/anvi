#ifndef APP_H
#define APP_H

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"

#include "keyboard.h"

struct anvi_output;
constexpr uint32_t TEXT_BUFFER_SIZE = 256;

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

    uint32_t text_buffer_next_free;
    char text_buffer[TEXT_BUFFER_SIZE];

    bool initialization_failed;
    bool session_is_locked;
    bool session_is_finished;
};

#endif
