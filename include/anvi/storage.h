#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

struct anvi_text_buffer;

constexpr int32_t SAVE_EVERY_MS = 5000; // Every second!
struct anvi_storage {
    int document_fd;
    char path_for_document_fd[4096];
    int save_timer_fd;
};

int create_document_fd(struct anvi_storage *storage);
int write_bytes_to_document_fd(struct anvi_storage *storage, struct anvi_text_buffer *doc);
int create_save_timer_fd(struct anvi_storage *storage);
int start_save_timer(struct anvi_storage *storage);
int handle_save_timer(struct anvi_storage *storage, struct anvi_text_buffer *doc);

#endif
