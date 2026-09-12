#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/timerfd.h>
#include <assert.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <wayland-client.h>

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/keyboard.h>



static void keymap(void *data,
		       struct wl_keyboard *wl_keyboard,
		       uint32_t format,
		       int32_t fd,
		       uint32_t size) {

    (void)wl_keyboard; // The argument is redundant since data also contains the wl_keyboard object.
    
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        anvi_log_error("Wrong wl keyboard keymap format, exiting keymap callback...");
        close(fd);
        return;
    }
    
    struct anvi_state *state = data;
    struct anvi_keyboard *keyboard = state->keyboard;

    char* map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (map_shm == MAP_FAILED) {
        anvi_log_error("Mmap failed!");
        close(fd);
        return;
    }

    struct xkb_keymap *new_keymap = xkb_keymap_new_from_string(keyboard->xkb_context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

    munmap(map_shm, size);
    close(fd);

    if (new_keymap == NULL) {
        anvi_log_error("Failed to compile XKB keymap.");
        return;
    }

    struct xkb_state *new_state = xkb_state_new(new_keymap);

    if (new_state == NULL) {
        anvi_log_error("Failed to create XKB state.");
        xkb_keymap_unref(new_keymap);
        return;
    }

    xkb_state_unref(keyboard->xkb_state);
    xkb_keymap_unref(keyboard->xkb_keymap);

    keyboard->xkb_keymap = new_keymap;
    keyboard->xkb_state = new_state;

    anvi_log_info("Created new xkb_keymap and xkb_state!\n");
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
    anvi_log_info("Serial: %" PRIu32 "\n", serial);
}

static void leave(void *data,
		      struct wl_keyboard *keyboard,
		      uint32_t serial,
		      struct wl_surface *surface) {
    (void)data;
    (void)keyboard;
    (void)surface;
    anvi_log_info("Serial: %" PRIu32 "\n", serial);
}

static int
number_in_text_buffer(struct anvi_text_buffer *tb) {
    if (tb->length_bytes > 7) {
        return -1; // No user wants to type a million characters...
    }
    int ret = 0;
    for (size_t i = 0; i < tb->length_bytes; ++i) {
        ret *= 10;
        if (tb->data[i] < '0' || tb->data[i] > '9') {
            return -1;
        }
        ret += tb->data[i] - '0';
    }
    return ret;
}

void
reset_text_buffer(struct anvi_text_buffer *tb) {
    for (size_t i = 0; i < tb->length_bytes; ++i) {
        tb->data[i] = '\0';
    }
    tb->length_bytes = 0;
    tb->cursor_bytes = 0;
}

static void
handle_return(struct anvi_state *state) {
    switch (state->phase) {
        case ANVI_START_CONFIGURATION_PHASE:
            const int x = number_in_text_buffer(state->text_buffer);
            if (x == -1) {
                state->start_phase_include_invalid_input_text = true;
                reset_text_buffer(state->text_buffer);
            } else {
                state->words_to_exit = x;
                // TODO: I'm not convinced that it's a good idea to have the phase switch here...
                // Maybe the "return" -key press should put some "flush" -boolean on in the
                // state instead. And then in the main loop we would have the phase-switching logic.
                state->phase = ANVI_NORMAL_PHASE;
                reset_text_buffer(state->text_buffer);
            }
            break;
        case ANVI_NORMAL_PHASE:
            anvi_text_buffer_insert(state->text_buffer, "\n", 1);
            break;
        case ANVI_FINISHED_PHASE:
            // No-op I guess
            break;
    }
}

bool check_and_handle_special_keys(struct anvi_state *state, xkb_keysym_t keysym) {

    switch (keysym) {
        case XKB_KEY_Left:
            anvi_text_buffer_left_arrow(state->text_buffer);
            return true;
        case XKB_KEY_Right:
            anvi_text_buffer_right_arrow(state->text_buffer);
            return true;
        case XKB_KEY_BackSpace:
            anvi_text_buffer_backspace(state->text_buffer);
            return true;
        case XKB_KEY_Return:
            handle_return(state);
            return true;
    }
    return false;
}

void handle_potential_text_input(struct anvi_state *state, xkb_keycode_t xkb_keycode) {

    struct anvi_keyboard *keyboard = state->keyboard;

    char text[64];
    const int length = xkb_state_key_get_utf8(keyboard->xkb_state, xkb_keycode, text, sizeof(text));

    anvi_text_buffer_insert(state->text_buffer, text, length);
}

static void
handle_key_press(struct anvi_state *state, xkb_keycode_t xkb_keycode) {

    struct anvi_keyboard *keyboard = state->keyboard;

    xkb_keysym_t keysym = xkb_state_key_get_one_sym(keyboard->xkb_state, xkb_keycode);
    bool special_key = check_and_handle_special_keys(state, keysym);

    // Even though special_key == false here it might just be that we missed some special key in our `check_and_handle_special_keys` -function. Just a fyi.
    if (!special_key) {
        handle_potential_text_input(state, xkb_keycode);
    }

}

void
handle_timer(struct anvi_state *state) {
    struct anvi_keyboard *kb = state->keyboard;
    assert(kb->repeat_timer_fd >= 0);

    uint64_t expirations;

    ssize_t bytes_read = read(kb->repeat_timer_fd, &expirations, sizeof(expirations));

    if (!kb->repeat_active) {
        return;
    }
    
    if (bytes_read == sizeof(expirations)) {
        handle_key_press(state, kb->repeating_keycode);
    }

    if (kb->repeat_rate == 0) {
        return; // repeat rate of 0 means no repeat.
    }
    // Start new timer.
    struct itimerspec timer = {0};

    int64_t interval_ns = 1000000000LL / kb->repeat_rate;
    timer.it_value.tv_sec = interval_ns / 1000000000LL;
    timer.it_value.tv_nsec = interval_ns % 1000000000LL;

    if (timerfd_settime(kb->repeat_timer_fd, 0, &timer, NULL) < 0) {
        anvi_log_error("Failed to start repeat time.");
    }
}

static void
start_repeat_timer(struct anvi_keyboard *keyboard) {
    struct itimerspec timer = {0};

    timer.it_value.tv_sec = keyboard->repeat_delay / 1000;
    timer.it_value.tv_nsec = (keyboard->repeat_delay % 1000) * 1000000L;

    if (timerfd_settime(keyboard->repeat_timer_fd, 0, &timer, NULL) < 0) {
        anvi_log_error("Failed to start repeat time.");
    }
}

static void
stop_key_repeat(struct anvi_keyboard *keyboard) {
    struct itimerspec timer = {0};

    timerfd_settime(
            keyboard->repeat_timer_fd,
            0,
            &timer,
            NULL
    );

    keyboard->repeat_active = false;
}


static void
key(void *data,
		    struct wl_keyboard *wl_keyboard,
		    uint32_t serial,
		    uint32_t time,
		    uint32_t wayland_keycode,
		    uint32_t key_state) {
    (void)wl_keyboard;
    (void)serial;
    (void)time;

    struct anvi_state *state = (struct anvi_state *)data;
    struct anvi_keyboard *keyboard = state->keyboard;

    anvi_log_info("Some key was pressed with keycode = %zu", wayland_keycode);

    if (keyboard->xkb_state == NULL) {
        anvi_log_error("No xkb_state found for keyboard...");
        return;
    }

    xkb_keycode_t xkb_keycode = wayland_keycode + 8;


    if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        handle_key_press(state, xkb_keycode);

        if (keyboard->repeat_rate > 0 && xkb_keymap_key_repeats(keyboard->xkb_keymap, xkb_keycode)) {
            keyboard->repeating_keycode = xkb_keycode;
            keyboard->repeat_active = true;
            start_repeat_timer(keyboard);
        }
    } else if (key_state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (keyboard->repeat_active && keyboard->repeating_keycode == xkb_keycode) {
            stop_key_repeat(keyboard);
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
    (void)serial;
    struct anvi_state *state = data;
    struct anvi_keyboard *keyboard = state->keyboard;

    if (keyboard->xkb_state == NULL) {
        anvi_log_error("Keyboard xkb_state is NULL...");
        return;
    }

    xkb_state_update_mask(keyboard->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
    (void)wl_keyboard;

    struct anvi_state *state = data;
    struct anvi_keyboard *keyboard = state->keyboard;

    keyboard->repeat_rate = rate;
    keyboard->repeat_delay = delay;
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

    if (keyboard->repeat_timer_fd >= 0) {
        close(keyboard->repeat_timer_fd);
    }

    free(keyboard);
}

struct anvi_keyboard *anvi_keyboard_create(struct anvi_state *state, struct wl_seat *seat) {
    struct anvi_keyboard *keyboard = calloc(1, sizeof(struct anvi_keyboard));

    if (keyboard == NULL) {
        anvi_log_error("Failed to allocate memory for keyboard struct...");
        return NULL;
    }
    keyboard->repeat_timer_fd = -1; // Because calloc might initialise it as 0 which is interpreted as a valid file descriptor.

    keyboard->proxy = wl_seat_get_keyboard(seat);

    if (keyboard->proxy == NULL) {
        free(keyboard);
        return NULL;
    }

    if (wl_keyboard_add_listener(keyboard->proxy, &keyboard_listener, state)) {
        anvi_log_error("Failed to add keyboard listener...");
        anvi_keyboard_destroy(keyboard);
        return NULL;
    }

    keyboard->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    
    if (keyboard->xkb_context == NULL) {
        anvi_keyboard_destroy(keyboard);
        return NULL;
    }

    keyboard->repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);

    if (keyboard->repeat_timer_fd < 0) {
        anvi_log_error("Failed to create keyboard repeat timer.");
        anvi_keyboard_destroy(keyboard);
    }

    return keyboard;
}


bool anvi_keyboard_is_ready(const struct anvi_keyboard *keyboard) {
    if (keyboard == NULL) {
        return false;
    }
    return keyboard->xkb_state != NULL && keyboard->proxy != NULL && keyboard->xkb_context != NULL && keyboard->xkb_keymap != NULL;
}
