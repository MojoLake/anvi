#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <wayland-client.h>

#include "app.h"
#include "keyboard.h"

struct anvi_keyboard {
    struct wl_keyboard *proxy;
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
    bool key_pressed;
};

static void keymap(void *data,
		       struct wl_keyboard *wl_keyboard,
		       uint32_t format,
		       int32_t fd,
		       uint32_t size) {

    (void)wl_keyboard; // The argument is redundant since data also contains the wl_keyboard object.
    
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        printf("Wrong wl keyboard keymap format, exiting keymap callback...\n");
        close(fd);
        return;
    }
    
    struct anvi_state *state = (struct anvi_state *)data;
    struct anvi_keyboard *keyboard = state->keyboard;

    char* map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (map_shm == MAP_FAILED) {
        fprintf(stderr, "Mmap failed!\n");
        close(fd);
        return;
    }

    struct xkb_keymap *new_keymap = xkb_keymap_new_from_string(keyboard->xkb_context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

    munmap(map_shm, size);
    close(fd);

    if (new_keymap == NULL) {
        fprintf(stderr, "Failed to compile XKB keymap\n");
        return;
    }

    struct xkb_state *new_state = xkb_state_new(new_keymap);

    if (new_state == NULL) {
        fprintf(stderr, "Failed to create XKB state\n");
        xkb_keymap_unref(new_keymap);
        return;
    }

    xkb_state_unref(keyboard->xkb_state);
    xkb_keymap_unref(keyboard->xkb_keymap);

    keyboard->xkb_keymap = new_keymap;
    keyboard->xkb_state = new_state;

    printf("Created new xkb_keymap and xkb_state!\n");
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

int handle_left_arrow(struct anvi_state *state) {
    (void)state;
    return EXIT_SUCCESS;
}

int handle_right_arrow(struct anvi_state *state) {
    (void)state;
    return EXIT_SUCCESS;
}

int handle_backspace(struct anvi_state *state) {

    const uint32_t current_buffer_ind = state->text_buffer_next_free;
    if (current_buffer_ind == 0 || current_buffer_ind > (uint32_t)sizeof(state->text_buffer)) {
        return EXIT_FAILURE;
    }

    state->text_buffer_next_free = current_buffer_ind - 1;
    // We don't even need to overwrite the current character at
    // current_buffer_ind because our program assumes everything after
    // state->text_buffer_next_free to be garbage.
    return EXIT_SUCCESS;
}

static void key(void *data,
		    struct wl_keyboard *wl_keyboard,
		    uint32_t serial,
		    uint32_t time,
		    uint32_t wayland_keycode,
		    uint32_t key_state) {
    (void)wl_keyboard;

    struct anvi_state *state = (struct anvi_state *)data;
    struct anvi_keyboard *keyboard = state->keyboard;

    printf(
        "Key %" PRIu32 " with serial = %" PRIu32
        " was pressed at time %" PRIu32 " with state %" PRIu32 "\n",
        wayland_keycode,
        serial,
        time,
        key_state
    );

    if (keyboard->xkb_state == NULL) {
        fprintf(stderr, "No xkb_state found for keyboard...\n");
        return;
    }

    if (key_state != WL_KEYBOARD_KEY_STATE_PRESSED) {
        printf("Returning from key-callback since the key was not pressed...\n");
        return;
    }

    xkb_keycode_t xkb_keycode = wayland_keycode + 8;

    xkb_keysym_t keysym = xkb_state_key_get_one_sym(keyboard->xkb_state, xkb_keycode);

    printf("keysym: %u\n", keysym);


    switch (keysym) {
        case XKB_KEY_Left:
            handle_left_arrow(state);
            return;
        case XKB_KEY_Right:
            handle_right_arrow(state);
            return;
        case XKB_KEY_BackSpace:
            handle_backspace(state);
            return;
    }

    char text[64];
    int length = xkb_state_key_get_utf8(keyboard->xkb_state, xkb_keycode, text, sizeof(text));

    for (int i = 0; i < length; ++i) {
        printf("key: %c was pressed\n", text[i]);
    }

    if (length > 0  && (size_t)length < sizeof(text)) {
        // A text-producing key was typed!
        if (text[0] == 'a') {
            keyboard->key_pressed = true;
        }
    }

    for (int i = 0; i < length; ++i) {
        const int buffer_ind = state->text_buffer_next_free;
        if (buffer_ind < (int)sizeof(state->text_buffer)) {
            state->text_buffer[buffer_ind] = text[i];
            state->text_buffer_next_free = buffer_ind + 1;
        }
    }
}

static void modifiers(void *data,
			  struct wl_keyboard *wl_keyboard,
			  uint32_t serial,
			  uint32_t mods_depressed,
			  uint32_t mods_latched,
			  uint32_t mods_locked,
			  uint32_t group) {
    (void)wl_keyboard;
    struct anvi_state *state = (struct anvi_state *)data;
    struct anvi_keyboard *keyboard = state->keyboard;

    printf(
        "Inside modifiers we have %" PRIu32 " %" PRIu32 " %" PRIu32
        " %" PRIu32 " %" PRIu32 "\n",
        serial,
        mods_depressed,
        mods_latched,
        mods_locked,
        group
    );

    if (keyboard->xkb_state == NULL) {
        fprintf(stderr, "Keyboard xkb_state is NULL...\n");
        return;
    }

    xkb_state_update_mask(keyboard->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);

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

struct anvi_keyboard *anvi_keyboard_create(struct anvi_state *state, struct wl_seat *seat) {
    struct anvi_keyboard *keyboard = calloc(1, sizeof(struct anvi_keyboard));

    if (keyboard == NULL) {
        fprintf(stderr, "Failed to allocate memory for keyboard struct...\n");
        return NULL;
    }

    keyboard->proxy = wl_seat_get_keyboard(seat);

    if (keyboard->proxy == NULL) {
        free(keyboard);
        return NULL;
    }

    if (wl_keyboard_add_listener(keyboard->proxy, &keyboard_listener, state)) {
        fprintf(stderr, "Failed to add keyboard listener...\n");
        release_or_destroy_keyboard(keyboard->proxy);        
        free(keyboard);
        return NULL;
    }

    keyboard->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    
    if (keyboard->xkb_context == NULL) {
        release_or_destroy_keyboard(keyboard->proxy);
        free(keyboard);
        return NULL;
    }

    return keyboard;
}


void anvi_keyboard_destroy(struct anvi_keyboard *keyboard) {
    if (keyboard == NULL) {
        return;
    }
    if (keyboard->proxy != NULL) {
        release_or_destroy_keyboard(keyboard->proxy);
    }
    if (keyboard->xkb_state != NULL) {
        xkb_state_unref(keyboard->xkb_state);
    }
    if (keyboard->xkb_keymap != NULL) {
        xkb_keymap_unref(keyboard->xkb_keymap);
    }
    if (keyboard->xkb_context != NULL) {
        xkb_context_unref(keyboard->xkb_context);
    }
    free(keyboard);
}

bool anvi_keyboard_is_ready(const struct anvi_keyboard *keyboard) {
    if (keyboard == NULL) {
        return false;
    }
    return keyboard->xkb_state != NULL && keyboard->proxy != NULL && keyboard->xkb_context != NULL && keyboard->xkb_keymap != NULL;
}

bool anvi_keyboard_key_was_pressed(const struct anvi_keyboard *keyboard) {
    if (keyboard == NULL) {
        return false;
    }
    return keyboard->key_pressed;
}
