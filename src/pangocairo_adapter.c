#include <anvi/app.h>
#include <anvi/pangocairo_adapter.h>

#include <cairo.h>
#include <pango/pangocairo.h>

struct anvi_renderer {
    PangoLayout *layout;
    cairo_t *cr;
    int preferred_cursor_x;
    bool preferred_cursor_x_valid;
};

static void
draw_cursor_at_pos_with_height(double x, double y, double h, struct anvi_renderer *renderer) {
    cairo_move_to(renderer->cr, x, y);
    cairo_line_to(renderer->cr, x, y + h);
    cairo_stroke(renderer->cr);
}

void
anvi_pangocairo_draw_cursor(struct anvi_renderer *renderer, size_t cursor_bytes, size_t dx, size_t dy) {

    PangoRectangle pos;
    pango_layout_get_cursor_pos(renderer->layout, cursor_bytes, &pos, NULL);

    const double x = dx + pos.x / (double)PANGO_SCALE;
    const double y = dy + pos.y / (double)PANGO_SCALE;
    const double h = pos.height / (double)PANGO_SCALE;
    draw_cursor_at_pos_with_height(x, y, h, renderer);
}

void
anvi_pangocairo_draw_text_starting_at(struct anvi_renderer *renderer, char* text, const size_t x, const size_t y) {

    pango_layout_set_text(renderer->layout, text, -1);
    cairo_set_source_rgb(renderer->cr, 1.0, 1.0, 1.0);
    cairo_move_to(renderer->cr, x, y);
    pango_cairo_show_layout(renderer->cr, renderer->layout);
}

static void
create_or_update_pango_layout(struct anvi_renderer *renderer) {
    if (renderer->layout == NULL) {
        renderer->layout = pango_cairo_create_layout(renderer->cr);
        PangoFontDescription *font = pango_font_description_from_string("Sans 16");
        pango_layout_set_font_description(renderer->layout, font);
        pango_font_description_free(font);
    } else {
        pango_cairo_update_layout(renderer->cr, renderer->layout);
    }
}

static void
setup_pango_layout_for_current_output(const size_t width, struct anvi_renderer *renderer) {

    pango_layout_set_width(renderer->layout, width * PANGO_SCALE);
    pango_layout_set_wrap(renderer->layout, PANGO_WRAP_WORD_CHAR);
}

void
anvi_pangocairo_begin_frame(struct anvi_renderer *renderer, cairo_surface_t *cairo_surface, const size_t document_width) {

    renderer->cr = cairo_create(cairo_surface);
    create_or_update_pango_layout(renderer);
    setup_pango_layout_for_current_output(document_width, renderer);
}

void
anvi_pangocairo_end_frame(struct anvi_renderer *renderer, cairo_surface_t *cairo_surface) {
    cairo_destroy(renderer->cr);
    renderer->cr = NULL;
    cairo_surface_flush(cairo_surface);
}


cursor_top_and_bottom
get_cursor_top_and_bottom(struct anvi_renderer *renderer, const char *data, const size_t cursor_bytes) {
    pango_layout_set_text(renderer->layout, data, -1);
    PangoRectangle cursor;
    pango_layout_get_cursor_pos(renderer->layout, (int)cursor_bytes, &cursor, NULL);
    
    const double cursor_top = cursor.y / (double)PANGO_SCALE;
    const double cursor_height = cursor.height / (double)PANGO_SCALE;
    const double cursor_bottom = cursor_top + cursor_height;

    cursor_top_and_bottom result;
    result.cursor_top = cursor_top;
    result.cursor_bottom = cursor_bottom;

    return result;
}

void
anvi_pangocairo_begin_document_area(struct anvi_renderer *renderer, const double x, const double y, const double width, const double height, const double scroll_y) {

    cairo_save(renderer->cr);

    cairo_rectangle(renderer->cr, x, y, width, height);
    cairo_clip(renderer->cr);

    cairo_translate(renderer->cr, x, y - scroll_y);
}


void
anvi_pangocairo_end_document_area(struct anvi_renderer *renderer) {
    cairo_restore(renderer->cr);
}

bool
anvi_pangocairo_renderer_ready(struct anvi_renderer *renderer) {
    if (renderer == NULL || renderer->layout == NULL) {
        // maybe no need to check cr?
        return false;
    } 
    return true;
}


size_t
anvi_pangocairo_move_cursor_vertically(struct anvi_renderer *renderer, const char *text, size_t cursor_bytes, int direction) {

    if (renderer->layout == NULL) {
        return cursor_bytes;
    }
     
    pango_layout_set_text(renderer->layout, text, -1);

    int current_line;
    int current_x;
    pango_layout_index_to_line_x(renderer->layout, (int)cursor_bytes, false, &current_line, &current_x);

    if (!renderer->preferred_cursor_x_valid) {
        renderer->preferred_cursor_x = current_x;
        renderer->preferred_cursor_x_valid = true;
    }

    const int target_line = current_line + direction;
    if (target_line < 0 || target_line >= pango_layout_get_line_count(renderer->layout)) {
        return cursor_bytes;
    }

    PangoLayoutLine *line = pango_layout_get_line_readonly(renderer->layout, target_line);

    int index;
    int trailing;
    pango_layout_line_x_to_index(line, renderer->preferred_cursor_x, &index, &trailing);

    const char *position = g_utf8_offset_to_pointer(text + index, trailing);

    return (size_t)(position - text);
}

void
anvi_pangocairo_end_vertical_movement(struct anvi_renderer *renderer) {
    renderer->preferred_cursor_x_valid = false;
}


struct anvi_renderer *
anvi_pangocairo_renderer_create(void) {
    return calloc(1, sizeof(struct anvi_renderer));
}

void
anvi_pangocairo_free_renderer(struct anvi_renderer *renderer) {
    free(renderer);
}
