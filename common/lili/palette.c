/* palette.c - the M2.2 placeholder palette (see palette.h). The base colours are
 * eyeballed from the photos; M3 measures them (k-means on the fur, belly, nose). */
#include "palette.h"
#include <string.h>

static void ramp(uint8_t out[256][3], int first, int r, int g, int b) {
    for (int i = 0; i < PAL_RAMP; i++) {
        float k = 0.35f + 0.80f * i / (PAL_RAMP - 1);        /* dark shade -> base -> a little lighter */
        int c[3] = { (int)(r * k), (int)(g * k), (int)(b * k) };
        for (int j = 0; j < 3; j++) out[first + i][j] = (uint8_t)(c[j] > 255 ? 255 : c[j]);
    }
}

void pal_placeholder(uint8_t out[256][3]) {
    memset(out, 0, 256 * 3);
    const uint8_t fixed[][3] = { { 0, 0, 0 }, { 0x3B, 0x24, 0x18 }, { 12, 10, 12 }, { 255, 250, 242 }, { 255, 0, 255 } };
    memcpy(out, fixed, sizeof fixed);
    ramp(out, PAL_FLOOR, 236, 222, 200);
    ramp(out, PAL_WALL,  205, 186, 158);
    ramp(out, PAL_GRASS, 118, 178, 86);
    ramp(out, PAL_HEDGE, 70, 128, 62);
    ramp(out, PAL_FUR,   226, 170, 110);
    ramp(out, PAL_FUR_W, 250, 242, 230);
    ramp(out, PAL_WOOD,    176, 120, 72);      /* the corner bench, the door */
    ramp(out, PAL_BLANKET, 226, 40, 120);      /* D-14: the photo shows magenta-pink, not red */
    ramp(out, PAL_METAL,   180, 186, 196);
    ramp(out, PAL_WATER,   96, 160, 230);
    ramp(out, PAL_PAD,     235, 240, 250);     /* the white pad (its thin blue edge: M3) */
    ramp(out, PAL_HOUSE,   214, 196, 170);
    ramp(out, PAL_FLOWER,  240, 120, 150);
    ramp(out, PAL_LEAF,    86, 150, 70);
}
