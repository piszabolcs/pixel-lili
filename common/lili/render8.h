/* render8.h - the 8-bit indexed canvas and its flush (spec 7.2).
 *
 * The world draws palette indices into a 240 x 240 byte buffer (one logical
 * pixel = one byte). Only on the way to the glass does an index become a
 * colour: a 256-entry RGB565 lookup table (LUT), and every logical pixel is
 * doubled into 2 x 2 physical ones. Index 0 is transparent in sprites.
 * Platform-free; the caller owns the buffers (no malloc). */
#ifndef LILI_RENDER8_H
#define LILI_RENDER8_H
#include <stdint.h>
#include "config.h"

typedef struct {
    uint8_t  px[LILI_LOG_W * LILI_LOG_H];   /* 57 600 bytes: the world, in palette indices */
    uint16_t lut[256];                      /* index -> RGB565, the one place colours live at run time */
} render8_t;

/* palette: 256 x {r, g, b} in 0..255 -> the LUT (a time of day / season swap is just this call) */
void r8_set_palette(render8_t *r, const uint8_t rgb[256][3]);

void r8_clear(render8_t *r, uint8_t c);
void r8_fill(render8_t *r, int x, int y, int w, int h, uint8_t c);              /* clipped */
void r8_blit(render8_t *r, const uint8_t *src, int w, int h, int x, int y);    /* clipped; index 0 = transparent */

/* physical rows [y0, y0 + n) of the 480 x 480 RGB565 image into dst (n x 480 pixels):
 * the simulator asks for all 480 at once, the panel for 24-row stripes */
void r8_flush_rows(const render8_t *r, uint16_t *dst, int y0, int n);

#endif
