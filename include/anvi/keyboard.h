#ifndef ANVI_KEYBOARD_H
#define ANVI_KEYBOARD_H

#include <stdint.h>

struct wl_seat;
struct anvi_state;
struct anvi_keyboard;

struct anvi_keyboard {
    struct wl_keyboard *proxy;
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;

    int32_t repeat_rate;
    int32_t repeat_delay;

    bool key_pressed;
};


struct anvi_keyboard *anvi_keyboard_create(struct anvi_state *state, struct wl_seat *seat);

void
anvi_keyboard_destroy(struct anvi_keyboard *keyboard);

bool anvi_keyboard_is_ready(const struct anvi_keyboard *keyboard);

#endif
