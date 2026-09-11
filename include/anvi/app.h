#ifndef APP_H
#define APP_H

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"

#include <anvi/text_buffer.h>
#include <anvi/keyboard.h>

struct anvi_output;

enum anvi_phase {
    ANVI_START_CONFIGURATION_PHASE = 1,
    ANVI_NORMAL_PHASE = 2,
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

    struct anvi_text_buffer *text_buffer;

    enum anvi_phase phase;

    size_t words_to_exit;

    bool initialization_failed;
    bool session_is_locked;
    bool session_is_finished;
};

constexpr size_t WORDS_TO_EXIT = 15;
#endif
