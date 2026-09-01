#define _GNU_SOURCE 200112L // Right now Linux only? (gnu only)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include "app.h"

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

        wl_buffer_destroy(current_output->buffer);
        ext_session_lock_surface_v1_destroy(current_output->lock_surface);
        wl_surface_destroy(current_output->surface);

        destroy_output_proxy(current_output->proxy);
        free(current_output);

        current_output = next_output;
    }

    state->outputs = NULL;
}

void destroy_coldwrite_state(struct coldwrite_state *state) {
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

static int allocate_shm_file(size_t size) {
    int fd = memfd_create("coldwrite-buffer", MFD_CLOEXEC);

    if (fd < 0) {
        return -1;
    }
    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void lock_surface_configure(
    void *data,
    struct ext_session_lock_surface_v1 *lock_surface,
    uint32_t serial,
    uint32_t width,
    uint32_t height) {
    
    struct coldwrite_output *output = data;
    struct coldwrite_state *state = output->state;

    output->width = width;
    output->height = height;

    ext_session_lock_surface_v1_ack_configure(
            lock_surface,
            serial
    );

    printf("Lock surface configured: %" PRIu32 " x%" PRIu32 "\n", width, height);

    const uint32_t stride = width * 4;
    const uint32_t buffer_size = height * stride;


    int fd = allocate_shm_file(buffer_size);
    if (fd < 0) {
        fprintf(stderr, "Failed to allocate shm file\n");
        return;
    }
    uint8_t *pool_data = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (pool_data == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap pool data\n");
        return;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, buffer_size);

    if (pool == NULL) {
        fprintf(stderr, "Failed to create wl_shm_pool\n");
        return;
    }

    const int index = 0;
    const int offset = height * stride * index;
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, offset, width, height, stride, WL_SHM_FORMAT_XRGB8888);

    if (buffer == NULL) {
        fprintf(stderr, "Failed to create wl_buffer\n");
        return;
    }

    output->buffer = buffer;

    uint32_t* pixels = (uint32_t *)&pool_data[offset];
    memset(pixels, 0, width * height * 4);

    wl_shm_pool_destroy(pool);
    munmap(pool_data, buffer_size);
    close(fd);


    wl_surface_attach(output->surface, buffer, 0, 0);
    wl_surface_damage(output->surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(output->surface);
}


static const struct ext_session_lock_surface_v1_listener lock_surface_listener = {
    .configure = lock_surface_configure,
};

static void session_locked(void *data, struct ext_session_lock_v1 *ext_session_lock_v1) {
    (void)ext_session_lock_v1;
    struct coldwrite_state *state = data;
    state->session_is_locked = true;
    printf("The session is locked!\n");
}

static void session_finished(void *data, struct ext_session_lock_v1 *ext_session_lock_v1) {
    (void)ext_session_lock_v1;

    struct coldwrite_state *state = data;
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

static int exit_with_failure_and_message(char* msg) {
    fprintf(stderr, "%s", msg);
    return EXIT_FAILURE;
}

static int exit_with_failure_and_message_and_cleanup_state(char* msg, struct coldwrite_state *state) {

    destroy_coldwrite_state(state);
    return exit_with_failure_and_message(msg);
}

int setup_initial_state(struct coldwrite_state *state) {


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

    for (struct coldwrite_output *output = state->outputs; output != NULL; output = output->next) {
        output->surface = wl_compositor_create_surface(state->wl_compositor);

        if (output->surface == NULL) {
            return exit_with_failure_and_message_and_cleanup_state("Received output surface is NULL, exiting...\n", state);
        }

        output->lock_surface = ext_session_lock_v1_get_lock_surface(
                state->session_lock,
                output->surface,
                output->proxy
        );

        if (output->lock_surface == NULL) {
            return exit_with_failure_and_message_and_cleanup_state("Received lock surface is NULL, exiting...\n", state);
        }

        ext_session_lock_surface_v1_add_listener(
                output->lock_surface,
                &lock_surface_listener,
                output
        );
    }

    return EXIT_SUCCESS;
}


