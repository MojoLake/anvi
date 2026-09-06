#define _GNU_SOURCE 200112L // Right now Linux only? (gnu only)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#include <memory.h>

#include <wayland-client.h>

#include "app.h"
#include "log.h"
#include "buffer.h"
#include "output.h"


static void
buffer_release(void *data, struct wl_buffer *wl_buffer) {
    (void)wl_buffer;
    anvi_log_info("Buffer is being released.");
    struct anvi_buffer *buffer = data;
    buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

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

static int setup_pool_data(struct anvi_output *output, size_t pool_size, int fd) {

    uint8_t *pool_data = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (pool_data == MAP_FAILED) {
        anvi_log_error("Failed to mmap pool data\n");
        return EXIT_FAILURE;
    }

    output->render_state->pool_data = pool_data;
    output->render_state->pool_size = pool_size;
    
    return EXIT_SUCCESS;
}

static void
render_text_to_buffer(struct anvi_state *state, struct anvi_buffer *buffer) {

    // Clear the buffer.
    memset(buffer->data, 0, buffer->size);

    cairo_t *cr = cairo_create(buffer->cairo_surface);

    anvi_log_info("Rendering text to buffer...\n");
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, 50, 80);
    cairo_set_font_size(cr, 48);
    cairo_show_text(cr, state->text_buffer);

    cairo_destroy(cr);
    cairo_surface_flush(buffer->cairo_surface);
}

static void present_buffer(struct anvi_output *output, struct wl_buffer *buffer_proxy) {
    anvi_log_info("Presenting the buffer...\n");
    wl_surface_attach(output->surface, buffer_proxy, 0, 0);
    wl_surface_damage(output->surface, 0, 0, output->width, output->height);
    wl_surface_commit(output->surface);
}

/*
 * Returns a non-busy buffer or NULL if not found.
 */
static struct anvi_buffer *
find_free_buffer(struct anvi_output *output) {
    
    for (size_t i = 0; i < 2; ++i) {
        assert(output->render_state->buffers[i] != NULL);
        if (!output->render_state->buffers[i]->busy) {
            anvi_log_info("Buffer %d is not busy, ", i);
            return output->render_state->buffers[i];
        }
    }
    return NULL;
}

void draw_screen(struct anvi_state *state, struct anvi_output *output) {
    // First we must find a non-busy buffer:
    struct anvi_buffer *free_buffer = find_free_buffer(output);
    if (free_buffer == NULL) {
        anvi_log_error("Could not find a free buffer to draw to...");
        return;
    }
    free_buffer->busy = true;
    render_text_to_buffer(state, free_buffer);
    present_buffer(output, free_buffer->proxy);
}

static int setup_buffer_and_cairo(struct anvi_output *output, struct wl_shm_pool *shm_pool, const size_t stride, const size_t index) {

    output->render_state->buffers[index] = malloc(sizeof(struct anvi_buffer)); 

    if (output->render_state->buffers[index] == NULL) {
        anvi_log_error("Failed to allocate buffer.");
        return EXIT_FAILURE;
    }

    struct anvi_buffer *buffer = output->render_state->buffers[index];
    buffer->busy = false;
    buffer->size = output->height * stride;

    const size_t offset = buffer->size * index;

    struct wl_buffer *buffer_proxy = wl_shm_pool_create_buffer(shm_pool, offset, output->width, output->height, stride, WL_SHM_FORMAT_XRGB8888);

    if (buffer_proxy == NULL) {
        anvi_log_error("Failed to create wl_buffer\n");
        return EXIT_FAILURE;
    }

    if (wl_buffer_add_listener(buffer_proxy, &buffer_listener, buffer) == EXIT_FAILURE) {
        anvi_log_error("Failed to add buffer listener.");
        return EXIT_FAILURE;
    }

    buffer->proxy = buffer_proxy;

    uint8_t *buffer_data = output->render_state->pool_data + offset;

    buffer->data = buffer_data;

    buffer->cairo_surface = cairo_image_surface_create_for_data(
            buffer_data, CAIRO_FORMAT_RGB24, output->width, output->height, stride
    );

    anvi_log_info("cairo_surface created!\n");

    if (cairo_surface_status(buffer->cairo_surface) != CAIRO_STATUS_SUCCESS) {
        anvi_log_error("Cairo surface status is not success\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
    // output->render_state->cr = cairo_create(output->render_state->cairo_surface);
}

static int setup_two_buffers(struct anvi_output *output, struct wl_shm_pool *shm_pool, uint32_t stride) {

    for (size_t i = 0; i < 2; ++i) {

        if (setup_buffer_and_cairo(output, shm_pool, stride, i) == EXIT_FAILURE) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int setup_lock_screen(struct anvi_state *state, struct anvi_output *output) {

    anvi_log_info("Application either started or output got resized so setting up lock screen for output.");

    clean_up_render_state(output->render_state);

    output->render_state = calloc(1, sizeof(struct anvi_render_state));

    if (output->render_state == NULL) {
        anvi_log_error("Failed to allocate render state.");
        return EXIT_FAILURE;
    }

    const uint32_t stride = output->width * 4;
    const size_t pool_size = output->height * stride * 2; // x2 because we have two buffers

    int fd = allocate_shm_file(pool_size);
    if (fd < 0) {
        anvi_log_error("Failed to allocate shm file\n");
        return EXIT_FAILURE;
    }

    if (setup_pool_data(output, pool_size, fd) == EXIT_FAILURE) {
        close(fd);
        clean_up_render_state(output->render_state);
        return EXIT_FAILURE;
    }

    anvi_log_info("Pool data was setup successfully!\n");

    struct wl_shm_pool *shm_pool = wl_shm_create_pool(state->wl_shm, fd, pool_size);
    close(fd); // Not needed anymore.

    if (shm_pool == NULL) {
        anvi_log_error("Failed to create wl_shm_pool\n");
        clean_up_render_state(output->render_state);
        return EXIT_FAILURE;
    }

    anvi_log_info("wl_shm_pool was created successfully!\n");

    if (setup_two_buffers(output, shm_pool, stride) == EXIT_FAILURE) {
        clean_up_render_state(output->render_state);
        wl_shm_pool_destroy(shm_pool);
        return EXIT_FAILURE;
    }

    anvi_log_info("buffer and cairo were setup successfully!\n");

    wl_shm_pool_destroy(shm_pool);

    // Initial screen drawing.
    draw_screen(state, output);

    return EXIT_SUCCESS;
}
