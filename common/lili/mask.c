/* mask.c - see mask.h. The geometry is one formula: the signed distance from a
 * pixel centre to a rounded square (negative inside). Inside by d pixels means
 * the vignette lets through min(1, d / LILI_VIGNETTE_PHYS) of the colour. */
#include "mask.h"
#include <math.h>
#include <string.h>

/* signed distance, in pixels, from (px, py) to a square of half-side h with corner radius r, centred at (cx, cy) */
static float sd_round_square(float px, float py, float cx, float cy, float h, float r) {
    float qx = fabsf(px - cx) - (h - r), qy = fabsf(py - cy) - (h - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = (qx > qy ? qx : qy); if (in > 0) in = 0;
    return sqrtf(ox * ox + oy * oy) + in - r;
}

void mask_init(mask_t *m) {
    const float c = LILI_PHYS_W / 2.0f, h = LILI_PHYS_W / 2.0f;
    for (int y = 0; y < MASK_Q; y++) {
        int last = -1;
        for (int x = 0; x < MASK_Q; x++) {
            float d = -sd_round_square(x + 0.5f, y + 0.5f, c, c, h, (float)LILI_CORNER_R_PHYS);   /* inside: positive */
            float a = d / LILI_VIGNETTE_PHYS;
            if (a < 0) a = 0;
            if (a > 1) a = 1;
            uint8_t v = (uint8_t)(a * 255.0f + 0.5f);
            m->att[y * MASK_Q + x] = v;
            if (v != 255) last = x;
        }
        m->span[y] = (uint8_t)(last + 1);
    }
}

uint8_t mask_att(const mask_t *m, int x, int y) {
    int qx = x < MASK_Q ? x : LILI_PHYS_W - 1 - x, qy = y < MASK_Q ? y : LILI_PHYS_H - 1 - y;
    return m->att[qy * MASK_Q + qx];
}

static inline uint16_t dim565(uint16_t c, uint8_t a) {      /* each channel x a / 255 (a / 256 is close enough and a shift) */
    if (a == 0) return 0;
    uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31, k = a + 1u;
    return (uint16_t)(((r * k >> 8) << 11) | ((g * k >> 8) << 5) | (b * k >> 8));
}

void mask_apply_rows(const mask_t *m, uint16_t *dst, int y0, int n) {
    for (int py = y0; py < y0 + n; py++) {
        uint16_t *row = dst + (size_t)(py - y0) * LILI_PHYS_W;
        int qy = py < MASK_Q ? py : LILI_PHYS_H - 1 - py;
        const uint8_t *att = &m->att[qy * MASK_Q];
        int s = m->span[qy];                                   /* only the frame's band of this row is touched */
        for (int x = 0; x < s; x++) {
            uint8_t a = att[x];
            if (a != 255) { row[x] = dim565(row[x], a); row[LILI_PHYS_W - 1 - x] = dim565(row[LILI_PHYS_W - 1 - x], a); }
        }
    }
}

int mask_point_safe(int lx, int ly) {
    /* the logical corner circle; a centre must be LILI_SAFE_MARGIN inside it, in the corner squares */
    const float R = (float)LILI_CORNER_R, c = LILI_LOG_W / 2.0f;
    float d = -sd_round_square(lx + 0.5f, ly + 0.5f, c, c, c, R);
    int in_corner = (lx < R + LILI_SAFE_MARGIN || lx >= LILI_LOG_W - R - LILI_SAFE_MARGIN) &&
                    (ly < R + LILI_SAFE_MARGIN || ly >= LILI_LOG_H - R - LILI_SAFE_MARGIN);
    return !in_corner || d >= LILI_SAFE_MARGIN;
}

void mask_debug_draw(render8_t *r, uint8_t colour) {
    for (int y = 0; y < LILI_LOG_H; y++)
        for (int x = 0; x < LILI_LOG_W; x++)
            if (((x + y) & 3) == 0 && !mask_point_safe(x, y)) r->px[y * LILI_LOG_W + x] = colour;   /* hatched */
}

int mask_rect_visible(int lx, int ly, int w, int h, int margin) {
    const float R = (float)LILI_CORNER_R, c = LILI_LOG_W / 2.0f;
    const int xs[2] = { lx, lx + w }, ys[2] = { ly, ly + h };       /* the rectangle's outer corners, in pixel edges */
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            if (-sd_round_square((float)xs[i], (float)ys[j], c, c, c, R) < (float)margin) return 0;
    return 1;
}
