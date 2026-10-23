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
