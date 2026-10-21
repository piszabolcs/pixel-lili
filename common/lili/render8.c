/* render8.c - see render8.h. The flush is the hot loop of every frame: on the
 * panel it runs for all 230 400 physical pixels 25-30 times a second. */
#include "render8.h"
#include <string.h>

void r8_set_palette(render8_t *r, const uint8_t rgb[256][3]) {
    for (int i = 0; i < 256; i++)
        r->lut[i] = (uint16_t)(((rgb[i][0] & 0xF8) << 8) | ((rgb[i][1] & 0xFC) << 3) | (rgb[i][2] >> 3));
}

void r8_clear(render8_t *r, uint8_t c) { memset(r->px, c, sizeof r->px); }

void r8_fill(render8_t *r, int x, int y, int w, int h, uint8_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > LILI_LOG_W ? LILI_LOG_W : x + w, y1 = y + h > LILI_LOG_H ? LILI_LOG_H : y + h;
    for (int yy = y0; yy < y1; yy++) if (x1 > x0) memset(&r->px[yy * LILI_LOG_W + x0], c, (size_t)(x1 - x0));
}

void r8_blit(render8_t *r, const uint8_t *src, int w, int h, int x, int y) {
    for (int sy = 0; sy < h; sy++) {
        int yy = y + sy; if (yy < 0 || yy >= LILI_LOG_H) continue;
        for (int sx = 0; sx < w; sx++) {
            int xx = x + sx; if (xx < 0 || xx >= LILI_LOG_W) continue;
            uint8_t c = src[sy * w + sx];
            if (c) r->px[yy * LILI_LOG_W + xx] = c;
        }
    }
}

/* One logical row becomes one physical row: each index is looked up once and
 * written as a 32-bit word holding the same RGB565 colour twice (the 2x across).
 * The second physical row of the pair is a copy of the first (the 2x down). */
void r8_flush_rows(const render8_t *r, uint16_t *dst, int y0, int n) {
#if LILI_SCALE == 1
    for (int py = y0; py < y0 + n; py++) {                 /* native: one LUT look-up per pixel, two pixels a store */
        const uint8_t *src = &r->px[py * LILI_LOG_W];
        uint32_t *out = (uint32_t *)(dst + (size_t)(py - y0) * LILI_PHYS_W);
        for (int x = 0; x < LILI_LOG_W; x += 2)
            out[x >> 1] = (uint32_t)r->lut[src[x]] | ((uint32_t)r->lut[src[x + 1]] << 16);
    }
#else
    for (int py = y0; py < y0 + n; py++) {
        uint16_t *row = dst + (size_t)(py - y0) * LILI_PHYS_W;
        if (py > y0 && (py & 1)) {                         /* the odd row of a pair whose even row is in this call */
            memcpy(row, row - LILI_PHYS_W, LILI_PHYS_W * sizeof(uint16_t));
            continue;
        }
        const uint8_t *src = &r->px[(py >> 1) * LILI_LOG_W];
        uint32_t *out = (uint32_t *)row;
        for (int x = 0; x < LILI_LOG_W; x++) {
            uint32_t c = r->lut[src[x]];
            out[x] = c | (c << 16);
        }
    }
#endif
}
