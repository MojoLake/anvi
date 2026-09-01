#include <stdlib.h>
#include <stdio.h>

#include <wayland-client.h>

#include "app.h"
#include "output.h"

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

void destroy_anvi_output(struct anvi_output *output) {
    if (output->buffer != NULL) {
        wl_buffer_destroy(output->buffer);
    }

    if (output->lock_surface != NULL) {
        ext_session_lock_surface_v1_destroy(output->lock_surface);
    }

    if (output->surface) {
        wl_surface_destroy(output->surface);
    }

    if (output->proxy) {
        destroy_output_proxy(output->proxy);
    }

    free(output);
}

void destroy_outputs(struct anvi_state *state) {
    struct anvi_output *current_output = state->outputs;

    while (current_output != NULL) {
        struct anvi_output *next_output = current_output->next;

        destroy_anvi_output(current_output);

        current_output = next_output;
    }

    state->outputs = NULL;
}

bool remove_anvi_output(struct anvi_state *state, uint32_t registry_name) {
    
    struct anvi_output *current_output = state->outputs;

    struct anvi_output *previous_output = nullptr;

    while (current_output != NULL) {
        if (current_output->registry_name == registry_name) {
            if (previous_output != NULL) {
                previous_output->next = current_output->next;
            } else {
                state->outputs = current_output->next;
            }

            destroy_anvi_output(current_output);
            return true;
        }

        previous_output = current_output;
        current_output = current_output->next;
    }

    return false; // Didn't find the output to be removed
}

int create_and_bind_anvi_output(struct anvi_state* state, struct wl_registry *registry, uint32_t name, uint32_t bind_version) {

    uint32_t client_version = (uint32_t)wl_output_interface.version;

    if (client_version > bind_version) {
        client_version = bind_version;
    }

    struct anvi_output *new_output = calloc(1, sizeof(struct anvi_output));

    if (new_output == NULL) {
        fprintf(stderr, "Failed to allocate memory for a new output\n");
        state->initialization_failed = true;
        return EXIT_FAILURE;
    }

    new_output->registry_name = name;
    new_output->state = state;
    
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
        return EXIT_FAILURE;
    }

    new_output->next = state->outputs;
    state->outputs = new_output;

    return EXIT_SUCCESS;
}
