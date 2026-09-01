#include <stdlib.h>
#include <stdio.h>

#include <wayland-client.h>

#include "app.h"
#include "output.h"

void destroy_output_proxy(struct wl_output *output) {
    if (output == NULL) {
        return;
    }

    if (wl_output_get_version(output) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
        wl_output_release(output);
    } else {
        wl_output_destroy(output);
    }
}

void destroy_outputs(struct anvi_state *state) {
    struct anvi_output *current_output = state->outputs;

    while (current_output != NULL) {
        struct anvi_output *next_output = current_output->next;

        if (current_output->buffer != NULL) {
            wl_buffer_destroy(current_output->buffer);
        }

        if (current_output->lock_surface != NULL) {
            ext_session_lock_surface_v1_destroy(current_output->lock_surface);
        }

        if (current_output->surface) {
            wl_surface_destroy(current_output->surface);
        }

        if (current_output->proxy) {
            destroy_output_proxy(current_output->proxy);
        }

        free(current_output);

        current_output = next_output;
    }

    state->outputs = NULL;
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
