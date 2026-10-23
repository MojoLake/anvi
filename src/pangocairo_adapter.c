#include <anvi/app.h>
#include <anvi/pangocairo_adapter.h>

#include <cairo.h>
#include <pango/pangocairo.h>

static void
draw_cursor_at_pos_with_height(double x, double y, double h, cairo_t *cr) {
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x, y + h);
    cairo_stroke(cr);
}

void
anvi_pangocairo_draw_cursor(size_t cursor_bytes, cairo_t *cr, PangoLayout *layout, size_t dx, size_t dy) {

    PangoRectangle pos;
    pango_layout_get_cursor_pos(layout, cursor_bytes, &pos, NULL);

    const double x = dx + pos.x / (double)PANGO_SCALE;
    const double y = dy + pos.y / (double)PANGO_SCALE;
    const double h = pos.height / (double)PANGO_SCALE;
    draw_cursor_at_pos_with_height(x, y, h, cr);
}

void
anvi_pangocairo_draw_text_starting_at(cairo_t *cr, PangoLayout *layout, char* text, const size_t x, const size_t y) {

    pango_layout_set_text(layout, text, -1);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
}

static void
create_or_update_pango_layout(struct anvi_state *state, cairo_t *cr) {
    if (state->layout == NULL) {
        state->layout = pango_cairo_create_layout(cr);
        PangoFontDescription *font = pango_font_description_from_string("Sans 16");
        pango_layout_set_font_description(state->layout, font);
        pango_font_description_free(font);
    } else {
        pango_cairo_update_layout(cr, state->layout);
    }
}

static void
setup_pango_layout_for_current_output(const size_t width, PangoLayout *layout) {

    pango_layout_set_width(layout, width * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
}

void
anvi_pangocairo_begin_frame(struct anvi_state *state, const size_t document_width, cairo_t *cr) {

    create_or_update_pango_layout(state, cr);
    setup_pango_layout_for_current_output(document_width, state->layout);
}


cursor_top_and_bottom
get_cursor_top_and_bottom(PangoLayout *layout, const char *data, const size_t cursor_bytes) {
    pango_layout_set_text(layout, data, -1);
    PangoRectangle cursor;
    pango_layout_get_cursor_pos(layout, (int)cursor_bytes, &cursor, NULL);
    
    const double cursor_top = cursor.y / (double)PANGO_SCALE;
    const double cursor_height = cursor.height / (double)PANGO_SCALE;
    const double cursor_bottom = cursor_top + cursor_height;

    cursor_top_and_bottom result;
    result.cursor_top = cursor_top;
    result.cursor_bottom = cursor_bottom;

    return result;
}
