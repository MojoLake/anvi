#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <anvi/text_buffer.h>
#include <anvi/log.h>

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

void
test_backspace_does_nothing_when_empty_text_buffer() {
    struct anvi_text_buffer tb = {0};
    initialise_text_buffer_with_text(&tb, "", 0);

    assert(anvi_text_buffer_backspace(&tb) == EXIT_SUCCESS);

    assert(tb.cursor_bytes == 0 && tb.length_bytes == 0 && tb.data[0] == '\0');
}

void test_backspace_works_small_example() {
    struct anvi_text_buffer tb = {0};

    char* text = "moi";
    const int len = strlen(text);

    initialise_text_buffer_with_text(&tb, text, len);

    assert(anvi_text_buffer_backspace(&tb) == EXIT_SUCCESS);

    fprintf(stderr, "Data in buffer: %s\n", tb.data);

    assert(strcmp(tb.data, "mo") == 0);
}

void test_backspace_works_when_cursor_in_the_middle() {
    struct anvi_text_buffer tb = {0};

    char* text = "moi";
    const int len = strlen(text);

    initialise_text_buffer_with_text(&tb, text, len);

    anvi_text_buffer_move_cursor_in_dir(&tb, ANVI_CURSOR_LEFT);
    assert(anvi_text_buffer_backspace(&tb) == EXIT_SUCCESS);
    assert(strcmp(tb.data, "mi") == 0);
}

void test_backspace_works_with_umlaut() {
    struct anvi_text_buffer tb = {0};

    char* text = "minä";
    const int len = strlen(text);

    initialise_text_buffer_with_text(&tb, text, len);

    assert(anvi_text_buffer_backspace(&tb) == EXIT_SUCCESS);
    assert(strcmp(tb.data, "min") == 0);
}

void test_backspace_works_with_umlaut_with_cursor_in_middle() {
    struct anvi_text_buffer tb = {0};

    char* text = "itään";
    const int len = strlen(text);

    initialise_text_buffer_with_text(&tb, text, len);

    anvi_text_buffer_move_cursor_in_dir(&tb, ANVI_CURSOR_LEFT);
    assert(anvi_text_buffer_backspace(&tb) == EXIT_SUCCESS);
    assert(strcmp(tb.data, "itän") == 0);
}

void test_insert_works_with_small_example_with_cursor_at_the_end() {
    struct anvi_text_buffer tb = {0};

    char* text = "moi";
    const int len = strlen(text);

    initialise_text_buffer_with_text(&tb, text, len);

    assert(anvi_text_buffer_insert(&tb, "hei", 3) == EXIT_SUCCESS);
    assert(strcmp(tb.data, "moihei") == 0);
}

void test_insert_works_with_small_example_with_cursor_at_the_middle() {
    struct anvi_text_buffer tb = {0};

    char* text = "moi";
    const int len = strlen(text);

    initialise_text_buffer_with_text(&tb, text, len);

    anvi_text_buffer_move_cursor_in_dir(&tb, ANVI_CURSOR_LEFT);

    assert(anvi_text_buffer_insert(&tb, "hei", 3) == EXIT_SUCCESS);
    assert(strcmp(tb.data, "moheii") == 0);
}

int
main(void) {
    
    test_backspace_does_nothing_when_empty_text_buffer();
    test_backspace_works_small_example();
    test_backspace_works_when_cursor_in_the_middle();
    test_insert_works_with_small_example_with_cursor_at_the_end();
    test_insert_works_with_small_example_with_cursor_at_the_middle();
    test_backspace_works_with_umlaut();
    test_backspace_works_with_umlaut_with_cursor_in_middle();
}
