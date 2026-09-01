#define _GNU_SOURCE 200112L // Right now Linux only? (gnu only)

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <wayland-client.h>

#include "app.h"
#include "output.h"

static int allocate_shm_file(size_t size) {
    int fd = memfd_create("anvi-buffer", MFD_CLOEXEC);

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
    
    for (uint32_t i = 0; i < height; ++i) {
        for (uint32_t j = 0; j < width; ++j) {
            if ((i + j / 16 * 16) % 32  < 16) {
                pixels[i * width + j] = 0xFF666666;
            } else {
                pixels[i * width + j] = 0xFFEEEEEE;
            }
        }
    }

    wl_shm_pool_destroy(pool);
    munmap(pool_data, buffer_size);
    close(fd);


    wl_surface_attach(output->surface, buffer, 0, 0);
    wl_surface_damage(output->surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(output->surface);
}

const struct ext_session_lock_surface_v1_listener lock_surface_listener = {
    .configure = lock_surface_configure,
};
