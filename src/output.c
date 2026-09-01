#include <stdlib.h>

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

        wl_buffer_destroy(current_output->buffer);
        ext_session_lock_surface_v1_destroy(current_output->lock_surface);
        wl_surface_destroy(current_output->surface);

        destroy_output_proxy(current_output->proxy);
        free(current_output);

        current_output = next_output;
    }

    state->outputs = NULL;
}
