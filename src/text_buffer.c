#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <anvi/text_buffer.h>

void
anvi_text_buffer_move_cursor_in_dir(struct anvi_text_buffer *tb, enum anvi_cursor_direction dir) {
    assert(dir == ANVI_CURSOR_LEFT || dir == ANVI_CURSOR_RIGHT);
    if (dir == ANVI_CURSOR_LEFT) {
        if (tb->cursor_bytes > 0) {
            tb->cursor_bytes--;
        }
    } else if (dir == ANVI_CURSOR_RIGHT){
        if (tb->cursor_bytes < tb->length_bytes) {
            tb->cursor_bytes++;
        }
    } else {
        assert(false);
    }
}

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


int
anvi_text_buffer_right_arrow(struct anvi_text_buffer *tb) {
    // Go to the right.
    // So we will soon have some function for going one grapheme to the left or right.
    // Then the code would look something like:
    /*
     * try_move_cursor(1) for right and try_move_cursor(-1) for left
     *  and try_move_cursor(int) would be like:[
     * ] int try_move_cursor(int dp) (dp = displacement)
     *      
     */
    if (tb->cursor_bytes < tb->length_bytes) {
        tb->cursor_bytes++;
    }
    return EXIT_SUCCESS;
}

int
anvi_text_buffer_left_arrow(struct anvi_text_buffer *tb) {
    
    if (tb->cursor_bytes > 0) {
        tb->cursor_bytes--;
    }
    return EXIT_SUCCESS;
}
