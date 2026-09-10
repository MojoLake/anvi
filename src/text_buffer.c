#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <grapheme.h>

#include <anvi/text_buffer.h>
#include <anvi/log.h>

static size_t
find_length_of_grapheme_to_left_of_cursor(struct anvi_text_buffer *tb) {
    for (size_t i = 0; i < tb->length_bytes;) {
        const size_t adv = grapheme_next_character_break_utf8(tb->data + i, tb->length_bytes - i);
        if (adv == 0) {
            return 0;
        }
        if (i + adv == tb->cursor_bytes) {
            return adv;
        }
        i  += adv;
    }
    return 0; // Couldn't find the position so something went wrong.
}

size_t
anvi_text_buffer_word_count(struct anvi_text_buffer *tb) {
    size_t amount = 0;
    for (size_t i = 0; i < tb->length_bytes;) {
        const size_t adv1 = grapheme_next_word_break_utf8(tb->data + i, tb->length_bytes - i);
        i += adv1;
        const size_t adv2 = grapheme_next_word_break_utf8(tb->data + i, tb->length_bytes - i);
        i  += adv2;
        amount++;
        // anvi_log_info("moi");
        fprintf(stderr, "adv: %zu\n", adv1);
    }
    fprintf(stderr, "amount: %zu\n", amount);
    return amount;
}

int
anvi_text_buffer_move_cursor_in_dir(struct anvi_text_buffer *tb, enum anvi_cursor_direction dir) {
    assert(dir == ANVI_CURSOR_LEFT || dir == ANVI_CURSOR_RIGHT);
    if (dir == ANVI_CURSOR_LEFT) {

        // We need to find the length of the grapheme before the cursor.
        const size_t len = find_length_of_grapheme_to_left_of_cursor(tb);
        if (len == 0) {
            return EXIT_FAILURE;
        }

        assert(tb->cursor_bytes >= len);
        tb->cursor_bytes -= len;
        return EXIT_SUCCESS;
    } else if (dir == ANVI_CURSOR_RIGHT){
        size_t advance = grapheme_next_character_break_utf8(tb->data + tb->cursor_bytes, tb->length_bytes - tb->cursor_bytes);
        if (tb->cursor_bytes + advance <= tb->length_bytes) {
            tb->cursor_bytes += advance;
        }
        return EXIT_SUCCESS;
    } else {
        // Should never even get here.
        return EXIT_FAILURE;
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

    size_t dec = find_length_of_grapheme_to_left_of_cursor(tb);

    size_t amount_to_move = tb->length_bytes - tb->cursor_bytes;
    memmove(tb->data + tb->cursor_bytes - dec, tb->data + tb->cursor_bytes, amount_to_move + 1);
    
    assert(tb->data[tb->length_bytes - dec] == '\0');

    tb->length_bytes -= dec;
    tb->cursor_bytes -= dec;

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
    return anvi_text_buffer_move_cursor_in_dir(tb, ANVI_CURSOR_RIGHT);
}

int
anvi_text_buffer_left_arrow(struct anvi_text_buffer *tb) {
    
    return anvi_text_buffer_move_cursor_in_dir(tb, ANVI_CURSOR_LEFT);
}
