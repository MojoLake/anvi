#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include <stdint.h>
#include <stddef.h>

constexpr uint32_t TEXT_BUFFER_SIZE = 2048; // Can be increased by a lot I think

struct anvi_text_buffer {
    char data[TEXT_BUFFER_SIZE];
    size_t length_bytes;
    size_t cursor_bytes;
};

int anvi_text_buffer_backspace(struct anvi_text_buffer *text_buffer);
int anvi_text_buffer_insert(struct anvi_text_buffer *text_buffer, const char *text, size_t byte_length);

#endif
