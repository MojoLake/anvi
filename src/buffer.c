#define _GNU_SOURCE 200112L // Right now Linux only? (gnu only)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include <wayland-client.h>

#include "app.h"
#include "log.h"
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

int setup_pool_data(struct anvi_output *output, size_t pool_size, int fd) {

    uint8_t *pool_data = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (pool_data == MAP_FAILED) {
        anvi_log_error("Failed to mmap pool data\n");
        return EXIT_FAILURE;
    }

    output->pool_data = pool_data;
    output->pool_size = pool_size;
    
    return EXIT_SUCCESS;
}

void render_text_to_buffer(struct anvi_state *state, struct anvi_output *output) {
    anvi_log_info("Rendering text to buffer...\n");
    cairo_set_source_rgb(output->cr, 1.0, 1.0, 1.0);
    cairo_move_to(output->cr, 50, 80);
    cairo_set_font_size(output->cr, 48);
    cairo_show_text(output->cr, state->text_buffer);

    // cairo_destroy(output->cr); // TODO: perform this destroy when buffer is cleaned up
    cairo_surface_flush(output->cairo_surface);
}

void present_buffer(struct wl_surface *surface, struct wl_buffer *buffer_proxy) {
    anvi_log_info("Presenting the buffer...\n");
    wl_surface_attach(surface, buffer_proxy, 0, 0);
    wl_surface_damage(surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(surface);
}

void draw_screen(struct anvi_state *state, struct anvi_output *output) {
    render_text_to_buffer(state, output);
    present_buffer(output->surface, output->buffer->proxy);
}

int setup_buffer_and_cairo(struct anvi_output *output, struct wl_shm_pool *wl_shm_pool, uint32_t stride) {

    const int index = 0; // just one buffer right now
    const int offset = output->height * stride * index;

    anvi_log_info("creating buffer from wl_shm_pool\n");

    struct wl_buffer *buffer_proxy = wl_shm_pool_create_buffer(wl_shm_pool, offset, output->width, output->height, stride, WL_SHM_FORMAT_XRGB8888);

    anvi_log_info("created buffer from wl_shm_pool\n");

    if (buffer_proxy == NULL) {
        anvi_log_error("Failed to create wl_buffer\n");
        return EXIT_FAILURE;
    }

    output->buffer = calloc(1, sizeof(struct anvi_buffer));

    output->buffer->proxy = buffer_proxy;

    unsigned char *buffer_data = output->pool_data + offset;

    output->cairo_surface = cairo_image_surface_create_for_data(
            buffer_data, CAIRO_FORMAT_RGB24, output->width, output->height, stride
    );

    anvi_log_info("cairo_surface created!\n");

    if (cairo_surface_status(output->cairo_surface) != CAIRO_STATUS_SUCCESS) {
        anvi_log_error("Cairo surface status is not success\n");
        return EXIT_FAILURE;
    }

    output->cr = cairo_create(output->cairo_surface);

    return EXIT_SUCCESS;
}

// TODO: rename this to something more sensible.
int setup_initial_lock_screen(struct anvi_state *state, struct anvi_output *output, uint32_t width, uint32_t height) {

    const uint32_t stride = width * 4;
    const size_t pool_size = height * stride;

    int fd = allocate_shm_file(pool_size);
    if (fd < 0) {
        anvi_log_error("Failed to allocate shm file\n");
        return EXIT_FAILURE;
    }

    if (setup_pool_data(output, pool_size, fd) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    anvi_log_info("Pool data was setup successfully!\n");

    struct wl_shm_pool *wl_shm_pool = wl_shm_create_pool(state->wl_shm, fd, pool_size);
    close(fd); // Not needed anymore.

    if (wl_shm_pool == NULL) {
        anvi_log_error("Failed to create wl_shm_pool\n");
        return EXIT_FAILURE;
    }

    anvi_log_info("wl_shm_pool was created successfully!\n");

    if (setup_buffer_and_cairo(output, wl_shm_pool, stride) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    anvi_log_info("buffer and cairo were setup successfully!\n");

    wl_shm_pool_destroy(wl_shm_pool);

    // Initial screen drawing.
    draw_screen(state, output);

    return EXIT_SUCCESS;
}
