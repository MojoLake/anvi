#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "app.h"
#include "output.h"
#include "buffer.h"

void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {

    struct anvi_state *state = data;
    
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        printf("Seat has a keyboard\n");
        if (state->keyboard == NULL) {
            state->keyboard = anvi_keyboard_create(state, seat);

            if (state->keyboard == NULL) {
                state->initialization_failed = true;
            }
        }
    } else {
        printf("Seat doesn't have a keyboard (anymore?)\n");
        if (state->keyboard != NULL) {
            anvi_keyboard_destroy(state->keyboard);
            state->keyboard = NULL;
        }
    }

    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        printf("Seat has a pointer\n");
    }
}

void seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data;
    (void)seat;

    printf("Seat name: %s\n", name);
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void destroy_seat_proxy(struct wl_seat* seat) {
    if (seat == NULL) {
        return;
    }

    if (wl_seat_get_version(seat) >= WL_SEAT_RELEASE_SINCE_VERSION) {
        wl_seat_release(seat);
    } else {
        wl_seat_destroy(seat);
    }
}


void destroy_anvi_state(struct anvi_state *state) {
    destroy_outputs(state);
    if (state->wl_shm != NULL) {
        wl_shm_destroy(state->wl_shm);
        state->wl_shm = NULL;
    }

    if (state->wl_compositor != NULL) {
        wl_compositor_destroy(state->wl_compositor);
        state->wl_compositor = NULL;
    }

    if (state->session_lock_manager != NULL) {
        ext_session_lock_manager_v1_destroy(state->session_lock_manager);
        state->session_lock_manager = NULL;
    }

    if (state->keyboard != NULL) {
        anvi_keyboard_destroy(state->keyboard);
        state->keyboard = NULL;
    }

    if (state->seat != NULL) {
        destroy_seat_proxy(state->seat);
        state->seat = NULL;
    }

    if (state->registry != NULL) {
        wl_registry_destroy(state->registry);
        state->registry = NULL;
    }

    if (state->display != NULL) {
        wl_display_disconnect(state->display);    
        state->display = NULL;
    }
}


static void session_locked(void *data, struct ext_session_lock_v1 *ext_session_lock_v1) {
    (void)ext_session_lock_v1;
    struct anvi_state *state = data;
    state->session_is_locked = true;
    printf("The session is locked!\n");
}

static void session_finished(void *data, struct ext_session_lock_v1 *ext_session_lock_v1) {
    (void)ext_session_lock_v1;

    struct anvi_state *state = data;
    state->session_is_finished = true;

    printf("The lock has been rejected or terminated.\n");
}

static const struct ext_session_lock_v1_listener session_lock_listener = {
    .locked = session_locked,
    .finished = session_finished
};

static void registry_global(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
) {
    struct anvi_state *state = data;

    uint32_t bind_version = version;

    if (strcmp(interface, ext_session_lock_manager_v1_interface.name) == 0) {
        uint32_t client_version = (uint32_t)ext_session_lock_manager_v1_interface.version;

        if (client_version > bind_version) {
            client_version = bind_version;
        }

        state->session_lock_manager = wl_registry_bind(
            registry,
            name,
            &ext_session_lock_manager_v1_interface,
            client_version
        );
    }

    if (strcmp(interface, wl_compositor_interface.name) == 0) {

        uint32_t client_version = (uint32_t)wl_compositor_interface.version;

        if (client_version > bind_version) {
            client_version = bind_version;
        }

        state->wl_compositor = wl_registry_bind(
            registry,
            name,
            &wl_compositor_interface,
            client_version
        );
    }

    if (strcmp(interface, wl_output_interface.name) == 0) {
        if (create_and_bind_anvi_output(state, registry, name, bind_version) == EXIT_FAILURE) {
            return;
        }
    }

    if (strcmp(interface, wl_shm_interface.name) == 0) {
        if (create_and_bind_wl_shm(state, registry, name, bind_version) == EXIT_FAILURE) {
            fprintf(stderr, "Failed to create and bind wl_shm\n");
            return;
        }
    }

    if (strcmp(interface, wl_seat_interface.name) == 0) {
        uint32_t client_version = wl_seat_interface.version;
        if (client_version > bind_version) {
            client_version = bind_version;
        }

        state->seat = wl_registry_bind(
            registry,
            name,
            &wl_seat_interface,
            client_version
        );

        wl_seat_add_listener(state->seat, &seat_listener, state);
    }

    printf("global: name=%" PRIu32 ", interface=%s, version=%" PRIu32 "\n", name, interface, version);
}

static void registry_global_remove(
      void *data,
      struct wl_registry *registry,
      uint32_t name
) {
    (void)registry;

    struct anvi_state* state = data;

    if (remove_anvi_output(state, name)) {
        // yay we removed it. What do we do with this information though? 
        printf("global removed: name=%" PRIu32 "\n", name);
    }

    // TODO: handle also other things than outputs.
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static int exit_with_failure_and_message(char* msg) {
    fprintf(stderr, "%s", msg);
    return EXIT_FAILURE;
}

static int exit_with_failure_and_message_and_cleanup_state(char* msg, struct anvi_state *state) {

    destroy_anvi_state(state);
    return exit_with_failure_and_message(msg);
}

int setup_initial_state(struct anvi_state *state) {


   	state->display = wl_display_connect(NULL);
  	if (state->display == NULL) {
		return exit_with_failure_and_message("Unable to connect to the Wayland compositor\n");
   	}
    
   	printf("Connected to the Wayland compositor!\n");
	state->registry = wl_display_get_registry(state->display);

	if (state->registry == NULL) {
        return exit_with_failure_and_message_and_cleanup_state("Unable to obtain the Wayland registry\n", state);
   	}


    if (wl_registry_add_listener(state->registry, &registry_listener, state) < 0) {
        return exit_with_failure_and_message_and_cleanup_state("Unable to install the registry listener\n", state);
    }


    if (wl_display_roundtrip(state->display) < 0) {
        return exit_with_failure_and_message_and_cleanup_state("Wayland communication failed.\n", state);
    } 
    if (state->outputs == NULL) {
        return exit_with_failure_and_message_and_cleanup_state("No outputs were found...\n", state);
    }
    if (state->wl_shm == NULL) {
        return exit_with_failure_and_message_and_cleanup_state("Wayland shared memory (wl_shm) not available...\n", state);
    }
    if (state->initialization_failed) {
        return exit_with_failure_and_message_and_cleanup_state("Something went wrong with initialization, exiting...\n", state);
    }
    if (state->session_lock_manager == NULL) {
        return exit_with_failure_and_message_and_cleanup_state("Session locking is not supported\n", state);
    }
    if (state->wl_compositor == NULL) {
        return exit_with_failure_and_message_and_cleanup_state("The wl_compositor was not found\n", state);
    }
    if (wl_display_roundtrip(state->display) < 0) {
        return exit_with_failure_and_message_and_cleanup_state("Failed to receive initial Wayland object events\n", state);
    }
    if (state->initialization_failed) {
        return exit_with_failure_and_message_and_cleanup_state("Something went wrong with initialization, exiting...\n", state);
    }
    if (state->keyboard == NULL) {
        return exit_with_failure_and_message_and_cleanup_state("Wasn't able to find keyboard...\n", state);
    }


    if (wl_display_roundtrip(state->display) < 0) {
        return exit_with_failure_and_message_and_cleanup_state("Failed to receive initial keyboard events", state);
    }
    if (state->initialization_failed) {
        return exit_with_failure_and_message_and_cleanup_state("Something went wrong with initialization, exiting...\n", state);
    }


    state->session_lock = ext_session_lock_manager_v1_lock(state->session_lock_manager);
    if (state->session_lock == NULL) {
        return exit_with_failure_and_message_and_cleanup_state("Failed to acquire session lock.\n", state);
    }

    ext_session_lock_v1_add_listener(state->session_lock, &session_lock_listener, state);

    if (create_surfaces_for_outputs(state) == EXIT_FAILURE) {
        return exit_with_failure_and_message_and_cleanup_state("Something went wrong with creating surfaces for outputs...\n", state);
    }
    return EXIT_SUCCESS;
}

