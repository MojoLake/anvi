#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <poll.h>

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/output.h>
#include <anvi/buffer.h>
#include <anvi/keyboard.h>
#include <anvi/session_setup.h>


static constexpr size_t WORDS_TO_EXIT = 5;

int exit_with_failure_and_message(char* msg) {
    fprintf(stderr, "%s", msg);
    return EXIT_FAILURE;
}

bool
exit_condition_fulfilled(struct anvi_text_buffer *tb) {
    if (anvi_text_buffer_word_count(tb) >= WORDS_TO_EXIT) {
        return true;
    }
    if (tb->length_bytes < 2) return false;
    for (size_t i = 0; i < tb->length_bytes - 2; ++i) {
        if (tb->data[i] == '1' && tb->data[i + 1] == '2' && tb->data[i + 2] == '3') {
            return true;
        }
    }
    return false;
}


int main(void) {

    struct anvi_state state = {0};

    if (setup_initial_state(&state) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    anvi_log_info("Initial state setup successfully.\n");

    while (true) {

        wl_display_dispatch_pending(state.display);
        wl_display_prepare_read(state.display);
        wl_display_flush(state.display);

        const int wayland_fd = wl_display_get_fd(state.display);
        const int timer_fd = state.keyboard->repeat_timer_fd;

        struct pollfd fds[2] = {
            {
                .fd = wayland_fd,
                .events = POLLIN,
            },
            {
                .fd = timer_fd,
                .events = POLLIN,
            },
        };
        // if (wl_display_dispatch(state.display) < 0) {
        //     anvi_log_error("Wayland event dispatch failed...");
        //     break;
        // }
        const int result = poll(fds, 2, -1);
        anvi_log_info("Result: %d", result);

        if (fds[0].revents & POLLIN) {
            wl_display_read_events(state.display);
        } else {
            wl_display_cancel_read(state.display);
        }

        wl_display_dispatch_pending(state.display);

        if (fds[1].revents & POLLIN) {
            handle_timer(&state); 
        }

        if (state.session_is_finished) {
            if (state.session_is_locked) {
                ext_session_lock_v1_unlock_and_destroy(state.session_lock);
            } else {
                ext_session_lock_v1_destroy(state.session_lock);
            }
            state.session_lock = NULL;
            break;
        }

        if (state.session_is_locked && exit_condition_fulfilled(state.text_buffer)) {
            ext_session_lock_v1_unlock_and_destroy(state.session_lock);

            state.session_lock = NULL;

            wl_display_roundtrip(state.display);
            break;
        }
    }

    destroy_anvi_state(&state);
	return EXIT_SUCCESS;
}
