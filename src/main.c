#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"

struct coldwrite_output {
    uint32_t registry_name;
    struct wl_output *proxy;
    struct coldwrite_output *next;
};

struct coldwrite_state {
    struct ext_session_lock_manager_v1 *session_lock_manager;
    struct wl_compositor *wl_compositor;
    struct coldwrite_output *outputs;
    bool initialization_failed;
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

   	struct wl_display *display = wl_display_connect(NULL);
  	if (display == NULL) {
		fprintf(stderr, "Unable to connect to the Wayland compositor\n");

		return EXIT_FAILURE;
   	}
    

   	printf("Connected to the Wayland compositor!\n");
	struct wl_registry *registry = wl_display_get_registry(display);

	if (registry == NULL) {
		fprintf(stderr, "Unable to obtain the Wayland registry\n");
		wl_display_disconnect(display);
		return EXIT_FAILURE;
   	}

    if (wl_registry_add_listener(registry, &registry_listener, &state) < 0) {
        fprintf(stderr, "Unable to install the registry listener\n");
        wl_registry_destroy(registry);
        wl_display_disconnect(display);    
        return EXIT_FAILURE;
    }

    if (wl_display_roundtrip(display) < 0) {
        fprintf(stderr, "Wayland communication failed.\n");
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        return EXIT_FAILURE;
    }

    if (state.initialization_failed) {
        fprintf(stderr, "Something went wrong with initialization, exiting...\n");
        // TODO: remember to clean-up here. Not important now when protoptyping and building MVP.
        return 1;
    }

    if (state.session_lock_manager == NULL) {
        fprintf(stderr, "Session locking is not supported\n");
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        return EXIT_FAILURE;
    }

    if (state.wl_compositor == NULL) {
        fprintf(stderr, "The wl_compositor was not found\n");
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        return EXIT_FAILURE;
    }

    printf(
        "Built with support for %s version %d\n",
        ext_session_lock_manager_v1_interface.name,
        ext_session_lock_manager_v1_interface.version
    );

    ext_session_lock_manager_v1_destroy(state.session_lock_manager);
    wl_compositor_destroy(state.wl_compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

	return EXIT_SUCCESS;
}
