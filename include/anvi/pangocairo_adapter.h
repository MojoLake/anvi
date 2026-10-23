#ifndef PANGOCAIRO_ADAPTER_H
#define PANGOCAIRO_ADAPTER_H

#include <stdlib.h>

#include <pango/pangocairo.h>

void anvi_pangocairo_draw_cursor(size_t cursor_bytes, cairo_t *cr, PangoLayout *layout, size_t dx, size_t dy);

#endif
