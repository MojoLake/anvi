#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"

#include "keyboard.h"

struct coldwrite_output {
    uint32_t registry_name;
    struct wl_output *proxy;
    struct coldwrite_output *next;
};

struct coldwrite_state {
    struct ext_session_lock_manager_v1 *session_lock_manager;
    struct wl_compositor *wl_compositor;
    struct coldwrite_output *outputs;
    struct wl_shm *wl_shm;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_seat *seat;
    struct coldwrite_keyboard *keyboard;
    bool initialization_failed;
};


void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {

    struct coldwrite_state *state = data;
    
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        printf("Seat has a keyboard\n");
        if (state->keyboard == NULL) {
            state->keyboard = coldwrite_keyboard_create(seat);

            if (state->keyboard == NULL) {
                state->initialization_failed = true;
            }
        }
    } else {
        printf("Seat doesn't have a keyboard (anymore?)\n");
        if (state->keyboard != NULL) {
            coldwrite_keyboard_destroy(state->keyboard);
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

static void destroy_output_proxy(struct wl_output *output) {
    if (output == NULL) {
        return;
    }

    if (wl_output_get_version(output) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
        wl_output_release(output);
    } else {
        wl_output_destroy(output);
    }
}

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

static void destroy_outputs(struct coldwrite_state *state) {
    struct coldwrite_output *current_output = state->outputs;

    while (current_output != NULL) {
        struct coldwrite_output *next_output = current_output->next;

        destroy_output_proxy(current_output->proxy);
        free(current_output);

        current_output = next_output;
    }

    state->outputs = NULL;
}

static void destroy_coldwrite_state(struct coldwrite_state *state) {
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
        coldwrite_keyboard_destroy(state->keyboard);
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

static void registry_global(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
) {
    struct coldwrite_state *state = data;

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
        uint32_t client_version = (uint32_t)wl_output_interface.version;

        if (client_version > bind_version) {
            client_version = bind_version;
        }

        struct coldwrite_output *new_output = calloc(1, sizeof(struct coldwrite_output));

        if (new_output == NULL) {
            fprintf(stderr, "Failed to allocate memory for a new output\n");
            state->initialization_failed = true;
            return;
        }

        new_output->registry_name = name;
        
        new_output->proxy = wl_registry_bind(
            registry,
            name,
            &wl_output_interface,
            client_version
        );

        if (new_output->proxy == NULL) {
            fprintf(stderr, "Binding wl_output returned NULL\n");
            state->initialization_failed = true;
            free(new_output);
            return;
        }

        new_output->next = state->outputs;
        state->outputs = new_output;
    }

    if (strcmp(interface, wl_shm_interface.name) == 0) {
        uint32_t client_version = wl_shm_interface.version;

        if (client_version > bind_version) {
            client_version = bind_version;
        }

        state->wl_shm = wl_registry_bind(
            registry,
            name,
            &wl_shm_interface,
            client_version
        );

        if (state->wl_shm == NULL) {
            state->initialization_failed = true;
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

    struct coldwrite_state* state = data;

    struct coldwrite_output *current_output = state->outputs;

    struct coldwrite_output *previous_output = nullptr;

    while (current_output != NULL) {
        if (current_output->registry_name == name) {
            if (previous_output != NULL) {
                previous_output->next = current_output->next;
            } else {
                state->outputs = current_output->next;
            }

            destroy_output_proxy(current_output->proxy);
            free(current_output);
            printf("global removed: name=%" PRIu32 "\n", name);
            return;
        }

        previous_output = current_output;
        current_output = current_output->next;
    }

}


static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main(void) {

    struct coldwrite_state state = {0};

   	state.display = wl_display_connect(NULL);
  	if (state.display == NULL) {
		fprintf(stderr, "Unable to connect to the Wayland compositor\n");

		return EXIT_FAILURE;
   	}
    

   	printf("Connected to the Wayland compositor!\n");
	state.registry = wl_display_get_registry(state.display);

	if (state.registry == NULL) {
		fprintf(stderr, "Unable to obtain the Wayland registry\n");
        destroy_coldwrite_state(&state);
		return EXIT_FAILURE;
   	}

    if (wl_registry_add_listener(state.registry, &registry_listener, &state) < 0) {
        fprintf(stderr, "Unable to install the registry listener\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (wl_display_roundtrip(state.display) < 0) {
        fprintf(stderr, "Wayland communication failed.\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (state.outputs == NULL) {
        fprintf(stderr, "No outputs were found...\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (state.wl_shm == NULL) {
        fprintf(stderr, "Wayland shared memory (wl_shm) not available...\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (state.initialization_failed) {
        fprintf(stderr, "Something went wrong with initialization, exiting...\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (state.session_lock_manager == NULL) {
        fprintf(stderr, "Session locking is not supported\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (state.wl_compositor == NULL) {
        fprintf(stderr, "The wl_compositor was not found\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (wl_display_roundtrip(state.display) < 0) {
        fprintf(stderr, "Failed to receive initial Wayland object events\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (state.initialization_failed) {
        fprintf(stderr, "Something went wrong with initialization, exiting...\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (state.keyboard == NULL) {
        fprintf(stderr, "Keyboard state is NULL...\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (wl_display_roundtrip(state.display) < 0) {
        fprintf(stderr, "Failed to receive initial keyboard events");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    if (state.initialization_failed) {
        fprintf(stderr, "Something went wrong with initialization, exiting...\n");
        destroy_coldwrite_state(&state);
        return EXIT_FAILURE;
    }

    printf(
        "Built with support for %s version %d\n",
        ext_session_lock_manager_v1_interface.name,
        ext_session_lock_manager_v1_interface.version
    );

    // printf("%", state. & WL_SEAT_CAPABILITY_KEYBOARD);

    destroy_coldwrite_state(&state);

	return EXIT_SUCCESS;
}
