#ifndef PANGOCAIRO_ADAPTER_H
#define PANGOCAIRO_ADAPTER_H

#include <stdlib.h>

#include <anvi/app.h>
#include <pango/pangocairo.h>

struct anvi_renderer;

void anvi_pangocairo_draw_cursor(struct anvi_renderer *renderer, size_t cursor_bytes, size_t dx, size_t dy);

void anvi_pangocairo_draw_text_starting_at(struct anvi_renderer *renderer, char* text, const size_t x, const size_t y);

void
anvi_pangocairo_begin_frame(struct anvi_renderer *renderer, cairo_surface_t *cairo_surface, const size_t document_width);

void
anvi_pangocairo_end_frame(struct anvi_renderer *renderer, cairo_surface_t *cairo_surface);

void
anvi_pangocairo_free_renderer(struct anvi_renderer *renderer);

typedef struct {
    double cursor_top;
    double cursor_bottom;
} cursor_top_and_bottom;


cursor_top_and_bottom
get_cursor_top_and_bottom(struct anvi_renderer *renderer, const char *data, const size_t cursor_bytes);

void
anvi_pangocairo_begin_document_area(struct anvi_renderer *renderer, const double x, const double y, const double width, const double height, const double scroll_y);

void
anvi_pangocairo_end_document_area(struct anvi_renderer *renderer);

size_t
anvi_pangocairo_move_cursor_vertically(struct anvi_renderer *renderer, const char *text, size_t cursor_bytes, int direction);

void
anvi_pangocairo_end_vertical_movement(struct anvi_renderer *renderer);

#endif
