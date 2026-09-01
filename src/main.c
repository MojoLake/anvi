#define _GNU_SOURCE 200112L // Right now Linux only? (gnu only)

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"

#include "app.h"
#include "keyboard.h"
#include "session_setup.h"


int exit_with_failure_and_message(char* msg) {
    fprintf(stderr, "%s", msg);
    return EXIT_FAILURE;
}


int main(void) {


    struct coldwrite_state state = {0};

    if (setup_initial_state(&state) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    while (true) {
        if (wl_display_dispatch(state.display) < 0) {
            fprintf(stderr, "Wayland event dispatch failed...\n");
            break;
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

        if (state.session_is_locked && coldwrite_keyboard_key_was_pressed(state.keyboard)) {
            ext_session_lock_v1_unlock_and_destroy(state.session_lock);

            state.session_lock = NULL;

            wl_display_roundtrip(state.display);
            break;
        }
    }


    printf(
        "Built with support for %s version %d\n",
        ext_session_lock_manager_v1_interface.name,
        ext_session_lock_manager_v1_interface.version
    );

    destroy_coldwrite_state(&state);

	return EXIT_SUCCESS;
}
