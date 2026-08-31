#ifndef COLDWRITE_KEYBOARD_H
#define COLDWRITE_KEYBOARD_H

struct wl_seat;
struct coldwrite_keyboard;

struct coldwrite_keyboard *
coldwrite_keyboard_create(struct wl_seat *seat);

void
coldwrite_keyboard_destroy(struct coldwrite_keyboard *keyboard);

bool coldwrite_keyboard_is_ready(const struct coldwrite_keyboard *keyboard);
bool coldwrite_keyboard_key_was_pressed(const struct coldwrite_keyboard *keyboard);

#endif
