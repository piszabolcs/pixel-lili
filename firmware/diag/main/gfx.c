#include "gfx.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

uint16_t *gfx_fb;
bool gfx_init(void) {
    gfx_fb = heap_caps_aligned_alloc(64, GFX_W * GFX_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!gfx_fb) { ESP_LOGE("gfx", "no PSRAM for the frame"); return false; }
    gfx_clear(C_BLACK);
    return true;
}
void gfx_clear(uint16_t c) { gfx_fill(0, 0, GFX_W, GFX_H, c); }
void gfx_fill(int x, int y, int w, int h, uint16_t c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GFX_W) w = GFX_W - x;
    if (y + h > GFX_H) h = GFX_H - y;
    if (w <= 0 || h <= 0) return;
    uint32_t cc = ((uint32_t)c << 16) | c;
    for (int j = 0; j < h; j++) {
        uint16_t *row = gfx_fb + (y + j) * GFX_W + x;
        int i = 0;
        if (((uintptr_t)row & 2) && w) { row[0] = c; i = 1; }
        for (; i + 1 < w; i += 2) *(uint32_t *)(row + i) = cc;
        if (i < w) row[i] = c;
    }
}
void gfx_px(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < GFX_W && y < GFX_H) gfx_fb[y * GFX_W + x] = c; }
void gfx_hline(int x, int y, int w, uint16_t c) { gfx_fill(x, y, w, 1, c); }
void gfx_vline(int x, int y, int h, uint16_t c) { gfx_fill(x, y, 1, h, c); }
void gfx_rect(int x, int y, int w, int h, uint16_t c) {
    gfx_hline(x, y, w, c); gfx_hline(x, y + h - 1, w, c); gfx_vline(x, y, h, c); gfx_vline(x + w - 1, y, h, c);
}
void gfx_ring(float cx, float cy, float r, float thick, uint16_t c, int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > GFX_W) x1 = GFX_W;
    if (y1 > GFX_H) y1 = GFX_H;
    float lo = r - thick * 0.5f, hi = r + thick * 0.5f;
    for (int y = y0; y < y1; y++) {
        float dy = (y + 0.5f) - cy;
        for (int x = x0; x < x1; x++) {
            float dx = (x + 0.5f) - cx, d = sqrtf(dx * dx + dy * dy);
            if (d >= lo && d < hi) gfx_fb[y * GFX_W + x] = c;
        }
    }
}

static uint32_t utf8_next(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    uint32_t cp; int n;
    if (p[0] < 0x80) { cp = p[0]; n = 1; }
    else if ((p[0] & 0xE0) == 0xC0) { cp = p[0] & 0x1F; n = 2; }
    else if ((p[0] & 0xF0) == 0xE0) { cp = p[0] & 0x0F; n = 3; }
    else { cp = p[0] & 0x07; n = 4; }
    for (int i = 1; i < n; i++) { if (!p[i]) { n = i; break; } cp = (cp << 6) | (p[i] & 0x3F); }
    *s += n;
    return cp;
}
int gfx_text_w(int scale, const char *s) {
    int n = 0;
    while (*s) { utf8_next(&s); n++; }
    return n ? (n * 6 - 1) * scale : 0;
}
int gfx_text(int x, int y, int scale, uint16_t c, const char *s) {
    int x0 = x;
    while (*s) {
        uint32_t cp = utf8_next(&s);
        uint16_t cols[5];
        if (!font_glyph(cp, cols)) { for (int i = 0; i < 5; i++) cols[i] = (i == 0 || i == 4) ? 0x1FC : 0x104; }
        for (int i = 0; i < 5; i++)
            for (int rbit = 0; rbit < 9; rbit++)
                if (cols[i] & (1 << rbit)) gfx_fill(x + i * scale, y + rbit * scale, scale, scale, c);
        x += 6 * scale;
    }
    return x - x0 - scale;
}
int gfx_textf(int x, int y, int scale, uint16_t c, const char *fmt, ...) {
    char buf[160]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    return gfx_text(x, y, scale, c, buf);
}
