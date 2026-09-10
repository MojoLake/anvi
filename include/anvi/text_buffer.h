#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include <stdint.h>
#include <stddef.h>

constexpr uint32_t TEXT_BUFFER_SIZE = 2048; // Can be increased by a lot I think

enum anvi_cursor_direction {
    ANVI_CURSOR_LEFT = -1,
    ANVI_CURSOR_RIGHT = 1,
};

struct anvi_text_buffer {
    char data[TEXT_BUFFER_SIZE];
    size_t length_bytes;
    size_t cursor_bytes;
};


int anvi_text_buffer_move_cursor_in_dir(struct anvi_text_buffer *tb, enum anvi_cursor_direction dir);
int anvi_text_buffer_backspace(struct anvi_text_buffer *text_buffer);
int anvi_text_buffer_insert(struct anvi_text_buffer *text_buffer, const char *text, size_t byte_length);
int anvi_text_buffer_right_arrow(struct anvi_text_buffer *tb);
int anvi_text_buffer_left_arrow(struct anvi_text_buffer *tb);
size_t anvi_text_buffer_word_count(struct anvi_text_buffer *tb);

#endif
