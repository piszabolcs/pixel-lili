/* mask.h - the glass's frame on the physical picture (spec 5.2, 7.2 step 7):
 * the rounded corners (LILI_CORNER_R_PHYS, measured in M1.7) cut to black, and a
 * LILI_VIGNETTE_PHYS wide fade to black along the whole edge so the picture melts
 * into the bezel. Applied after r8_flush_rows, on the same rows.
 *
 * The shape is symmetric in x and y, so one quadrant's table serves all four
 * (it is a frame, not a sprite: the no-mirroring rule is about Lili). */
#ifndef LILI_MASK_H
#define LILI_MASK_H
#include <stdint.h>
#include "config.h"
#include "render8.h"

#define LILI_VIGNETTE_PHYS 6
#define MASK_Q (LILI_PHYS_W / 2)                 /* 240: a quadrant's side */

typedef struct {
    uint8_t att[MASK_Q * MASK_Q];                /* top-left quadrant: 0 = black (outside the glass) .. 255 = untouched */
    uint8_t span[MASK_Q];                        /* per quadrant row: pixels from the edge that are not 255 (the rest are) */
} mask_t;

void mask_init(mask_t *m);                       /* once at boot: the table from the signed distance to the rounded box */
void mask_apply_rows(const mask_t *m, uint16_t *dst, int y0, int n);   /* the same rows r8_flush_rows produced */
uint8_t mask_att(const mask_t *m, int x, int y); /* any physical pixel, for tests */

/* debug layer on the logical canvas (spec 5.2): the corner zones where an interactive
 * element's centre must not be (closer than 24 logical px to the arc), hatched */
#define LILI_SAFE_MARGIN (24 * LILI_K)             /* 24 on the 240 design grid */
void mask_debug_draw(render8_t *r, uint8_t colour);
int  mask_point_safe(int lx, int ly);            /* 1 = a logical point may hold an interactive centre */
/* the rule in use since 2026-10-19 (docs/decisions.md): an interactive thing's whole rectangle
   is on the glass, at least `margin` logical px inside the visible edge (the rounded box is
   convex, so its four corners are enough to check) */
int  mask_rect_visible(int lx, int ly, int w, int h, int margin);

#endif
