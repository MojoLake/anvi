#include "keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include <wayland-client.h>

struct coldwrite_keyboard {
    struct wl_keyboard *proxy;
};

static void keymap(void *data,
		       struct wl_keyboard *keyboard,
		       uint32_t format,
		       int32_t fd,
		       uint32_t size) {
   (void)data;
   (void)keyboard;
   printf(
       "Format, fd, size: %" PRIu32 " %" PRId32 " %" PRIu32 "\n",
       format,
       fd,
       size
   );
   close(fd);
}

static void enter(void *data,
		      struct wl_keyboard *keyboard,
		      uint32_t serial,
		      struct wl_surface *surface,
		      struct wl_array *keys) {
    (void)data;
    (void)keyboard;
    (void)surface;
    (void)keys;
    printf("Serial: %" PRIu32 "\n", serial);
}

static void leave(void *data,
		      struct wl_keyboard *keyboard,
		      uint32_t serial,
		      struct wl_surface *surface) {
    (void)data;
    (void)keyboard;
    (void)surface;
    printf("Serial: %" PRIu32 "\n", serial);
}

static void key(void *data,
		    struct wl_keyboard *keyboard,
		    uint32_t serial,
		    uint32_t time,
		    uint32_t key,
		    uint32_t state) {
    (void)data;
    (void)keyboard;
    printf(
        "Key %" PRIu32 " with serial = %" PRIu32
        " was pressed at time %" PRIu32 " with state %" PRIu32 "\n",
        key,
        serial,
        time,
        state
    );
}

static void modifiers(void *data,
			  struct wl_keyboard *keyboard,
			  uint32_t serial,
			  uint32_t mods_depressed,
			  uint32_t mods_latched,
			  uint32_t mods_locked,
			  uint32_t group) {
    (void)data;
    (void)keyboard;
    printf(
        "Inside modifiers we have %" PRIu32 " %" PRIu32 " %" PRIu32
        " %" PRIu32 " %" PRIu32 "\n",
        serial,
        mods_depressed,
        mods_latched,
        mods_locked,
        group
    );
}

static void repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
    (void)data;
    (void)keyboard;
    printf("Repeat rate: %" PRId32 ", delay: %" PRId32 "\n", rate, delay);
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keymap,
    .enter = enter,
    .leave = leave,
    .key = key,
    .modifiers = modifiers,
    .repeat_info = repeat_info,
};

static void release_or_destroy_keyboard(struct wl_keyboard* keyboard) {
    if (keyboard == NULL) {
        return;
    }

    if (wl_keyboard_get_version(keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION) {
        wl_keyboard_release(keyboard);
    } else {
        wl_keyboard_destroy(keyboard);
    }
}

struct coldwrite_keyboard *coldwrite_keyboard_create(struct wl_seat *seat) {
    struct coldwrite_keyboard *keyboard = calloc(1, sizeof(struct coldwrite_keyboard)); 

    if (keyboard == NULL) {
        fprintf(stderr, "Failed to allocate memory for keyboard struct...\n");
        return NULL;
    }

    keyboard->proxy = wl_seat_get_keyboard(seat);

    if (keyboard->proxy == NULL) {
        free(keyboard);
        return NULL;
    }

    if (wl_keyboard_add_listener(keyboard->proxy, &keyboard_listener, keyboard) < 0) {
        fprintf(stderr, "Failed to add keyboard listener...\n");
        release_or_destroy_keyboard(keyboard->proxy);        
        free(keyboard);
        return NULL;
    }

    return keyboard;
}


void coldwrite_keyboard_destroy(struct coldwrite_keyboard *keyboard) {
    if (keyboard == NULL) {
        return;
    }
    if (keyboard->proxy != NULL) {
        release_or_destroy_keyboard(keyboard->proxy);
    }
    free(keyboard);
}
