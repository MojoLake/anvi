#ifndef STORAGE_H
#define STORAGE_H

struct anvi_text_buffer;

struct anvi_storage {
    int document_fd;
    char path_for_document_fd[4096];
};

int create_document_fd(struct anvi_storage *storage);
int write_bytes_to_document_fd(struct anvi_storage *storage, struct anvi_text_buffer *doc);

#endif
