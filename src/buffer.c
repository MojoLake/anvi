#define _GNU_SOURCE 200112L // Right now Linux only? (gnu only)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include <wayland-client.h>

#include "app.h"
#include "buffer.h"
#include "output.h"

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

int draw_initial_lock_screen(struct anvi_state *state, struct anvi_output *output, uint32_t width, uint32_t height) {

    const uint32_t stride = width * 4;
    const uint32_t buffer_size = height * stride;


    int fd = allocate_shm_file(buffer_size);
    if (fd < 0) {
        fprintf(stderr, "Failed to allocate shm file\n");
        return EXIT_FAILURE;
    }
    uint8_t *pool_data = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (pool_data == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap pool data\n");
        return EXIT_FAILURE;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, buffer_size);


    if (pool == NULL) {
        fprintf(stderr, "Failed to create wl_shm_pool\n");
        return EXIT_FAILURE;
    }

    const int index = 0;
    const int offset = height * stride * index;
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, offset, width, height, stride, WL_SHM_FORMAT_XRGB8888);

    if (buffer == NULL) {
        fprintf(stderr, "Failed to create wl_buffer\n");
        return EXIT_FAILURE;
    }

    output->buffer = buffer;

    uint32_t* pixels = (uint32_t *)&pool_data[offset];

    for (uint32_t i = 0; i < height; ++i) {
        for (uint32_t j = 0; j < width; ++j) {
            if ((i + j / 32 * 32) % 64  < 32) {
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

    return EXIT_SUCCESS;
}
