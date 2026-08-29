#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"


struct coldwrite_state {
    struct ext_session_lock_manager_v1 *session_lock_manager;
    struct wl_compositor *wl_compositor;
};

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

    printf("global: name=%" PRIu32 ", interface=%s, version=%" PRIu32 "\n", name, interface, version);
}

static void registry_global_remove(
      void *data,
      struct wl_registry *registry,
      uint32_t name
) {
      (void)data;
      (void)registry;

      printf("global removed: name=%" PRIu32 "\n", name);
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
