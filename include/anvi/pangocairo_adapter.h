#ifndef PANGOCAIRO_ADAPTER_H
#define PANGOCAIRO_ADAPTER_H

#include <stdlib.h>

#include <anvi/app.h>
#include <pango/pangocairo.h>

void anvi_pangocairo_draw_cursor(size_t cursor_bytes, cairo_t *cr, PangoLayout *layout, size_t dx, size_t dy);

void anvi_pangocairo_draw_text_starting_at(cairo_t *cr, PangoLayout *layout, char* text, const size_t x, const size_t y);

void
anvi_pangocairo_begin_frame(struct anvi_state *state, const size_t document_width, cairo_t *cr);

typedef struct {
    double cursor_top;
    double cursor_bottom;
} cursor_top_and_bottom;


cursor_top_and_bottom
get_cursor_top_and_bottom(PangoLayout *layout, const char *data, const size_t cursor_bytes);

#endif
