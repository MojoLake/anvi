#include <stdlib.h>
#include <unistd.h>

#include "app.h"

int create_and_bind_wl_shm(struct anvi_state* state, struct wl_registry *registry, uint32_t name, uint32_t bind_version) {
    
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
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
