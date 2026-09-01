#ifndef ANVI_KEYBOARD_H
#define ANVI_KEYBOARD_H

struct wl_seat;
struct anvi_keyboard;

struct anvi_keyboard *
anvi_keyboard_create(struct wl_seat *seat);

void
anvi_keyboard_destroy(struct anvi_keyboard *keyboard);

bool anvi_keyboard_is_ready(const struct anvi_keyboard *keyboard);
bool anvi_keyboard_key_was_pressed(const struct anvi_keyboard *keyboard);

#endif
