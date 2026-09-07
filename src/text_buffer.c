#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "text_buffer.h"


int
anvi_text_buffer_backspace(struct anvi_text_buffer *tb) {
    assert(tb != NULL);

    if (tb->cursor_bytes == 0) {
        return EXIT_SUCCESS;
    }

    if (tb->cursor_bytes > tb->length_bytes) {
        return EXIT_FAILURE;
    }

    // NOTE: this doesn't yet handle graphemes well and
    // basically just assumes every grapheme is one byte.

    size_t amount_to_move = tb->length_bytes - tb->cursor_bytes;
    memmove(tb->data + tb->cursor_bytes - 1, tb->data + tb->cursor_bytes, amount_to_move + 1);
    
    // tb->data[tb->length_bytes - 1] = '\0';
    assert(tb->data[tb->length_bytes - 1] == '\0');

    tb->length_bytes--;
    tb->cursor_bytes--;

    return EXIT_SUCCESS;
}

int
anvi_text_buffer_insert(struct anvi_text_buffer *tb, const char *text, size_t byte_length) {
    assert(tb != NULL);

    // First we have to move the things to the right of the cursor
    // byte_length to the right:
    assert(tb->cursor_bytes <= tb->length_bytes);
    size_t amount_to_move = tb->length_bytes - tb->cursor_bytes;
    // We need to add some capacity checks as well!
    memmove(tb->data + tb->cursor_bytes + byte_length, tb->data + tb->cursor_bytes, amount_to_move + 1);

    // Then put the text to the middle.
    memcpy(tb->data + tb->cursor_bytes, text, byte_length);

    tb->length_bytes += byte_length;
    tb->cursor_bytes += byte_length;

    return EXIT_SUCCESS;
}
