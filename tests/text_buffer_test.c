#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "text_buffer.h"

void
clear_text_buffer(struct anvi_text_buffer *tb) {
    for (size_t i = 0; i < tb->length_bytes; ++i) {
        tb->data[i] = '\0';
    }
    tb->length_bytes = tb->cursor_bytes = 0;
}

void
initialise_text_buffer_with_text(struct anvi_text_buffer *tb, char *text, size_t len) {

    clear_text_buffer(tb);

    memcpy(tb->data, text, len + 1); 

    tb->length_bytes = len;
    // Moves cursor to the end.
    tb->cursor_bytes = len;
}

void test_backspace_does_nothing_when_empty_text_buffer() {
    struct anvi_text_buffer tb = {0};
    initialise_text_buffer_with_text(&tb, "", 0);

    assert(anvi_text_buffer_backspace(&tb) == EXIT_SUCCESS);

    assert(tb.cursor_bytes == 0 && tb.length_bytes == 0 && tb.data[0] == '\0');
}

int main(void) {
    
    test_backspace_does_nothing_when_empty_text_buffer();
}

