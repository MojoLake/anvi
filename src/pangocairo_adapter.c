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
