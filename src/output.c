#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>

#include "app.h"
#include "log.h"
#include "output.h"
#include "buffer.h"


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

void clean_up_anvi_buffer(struct anvi_buffer *buffer) {

    if (buffer->proxy != NULL) {
        wl_buffer_destroy(buffer->proxy);
        buffer->proxy = NULL;
    }

    if (buffer->cairo_surface != NULL) {
        cairo_surface_destroy(buffer->cairo_surface);
        buffer->cairo_surface = NULL;
    }

    free(buffer);
    buffer = NULL;
}

void clean_up_render_state(struct anvi_render_state *render_state) {

    anvi_log_info("Cleaning up render state");

    if (render_state == NULL) {
        anvi_log_info("Render state is NULL so nothing to clean up.");
        return;
    }

    for (size_t i = 0; i < 2; ++i) {
        if (render_state->buffers[i] != NULL) {
            clean_up_anvi_buffer(render_state->buffers[i]);
        }
    }

    if (render_state->pool_data != NULL) {
        munmap(render_state->pool_data, render_state->pool_size);
        render_state->pool_data = NULL;
    }

    free(render_state);
}

void destroy_anvi_output(struct anvi_output *output) {

    if (output == NULL) {
        return;
    }

    if (output->render_state != NULL) {
        clean_up_render_state(output->render_state);
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
        anvi_log_error("Failed to allocate memory for a new output.");
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
        anvi_log_error("Binding wl_output returned NULL\n");
        state->initialization_failed = true;
        free(new_output);
        return EXIT_FAILURE;
    }

    new_output->next = state->outputs;
    state->outputs = new_output;

    return EXIT_SUCCESS;
}

static void lock_surface_configure(
    void *data,
    struct ext_session_lock_surface_v1 *lock_surface,
    uint32_t serial,
    uint32_t width,
    uint32_t height) {
    
    struct anvi_output *output = data;
    struct anvi_state *state = output->state;

    output->width = width;
    output->height = height;

    ext_session_lock_surface_v1_ack_configure(
            lock_surface,
            serial
    );

    if (setup_lock_screen(state, output) == EXIT_FAILURE) {
        return;
    }

    // What should we do there here?
}

const struct ext_session_lock_surface_v1_listener lock_surface_listener = {
    .configure = lock_surface_configure,
};


int create_surfaces_for_outputs(struct anvi_state *state) {

    for (struct anvi_output *output = state->outputs; output != NULL; output = output->next) {
        output->surface = wl_compositor_create_surface(state->wl_compositor);

        if (output->surface == NULL) {
            return EXIT_FAILURE;
        }

        output->lock_surface = ext_session_lock_v1_get_lock_surface(
                state->session_lock,
                output->surface,
                output->proxy
        );

        if (output->lock_surface == NULL) {
            wl_surface_destroy(output->surface);
            return EXIT_FAILURE;
        }

        ext_session_lock_surface_v1_add_listener(
                output->lock_surface,
                &lock_surface_listener,
                output
        );
    }

    return EXIT_SUCCESS;
}
