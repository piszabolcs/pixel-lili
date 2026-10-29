/* ui.c - see ui.h. Physical px = logical px on the 480 panel (LILI_SCALE 1). */
#include "ui.h"
#include "font5x9.h"
#include "life.h"
#include <string.h>
#include <stdio.h>

#define SLIDE_S   0.2f
#define STAY_S    4.0f
#define BAR_X     ((LILI_PHYS_W - UI_BAR_W) / 2)
#define CARD_X    ((LILI_PHYS_W - UI_CARD_W) / 2)
#define TINT_K    150         /* of 256: the dark glass over the blurred picture (spec 7.8 had 22 % white; the owner, 2026-10-29: */
#define TINT_R    24          /* the white text was hard to read on it) */
#define TINT_G    18
#define TINT_B    16

static uint16_t rgb(int r, int g, int b) { r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
                                            return (uint16_t)((r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3); }
static void unpack(uint16_t c, int *r, int *g, int *b) { *r = (c >> 8) & 0xF8; *g = (c >> 3) & 0xFC; *b = (c << 3) & 0xF8; }
#define WHITE rgb(255, 255, 255)
#define INK   rgb(40, 28, 22)
#define WARM  rgb(255, 170, 90)

void ui_init(ui_t *u) { memset(u, 0, sizeof *u); u->battery = 1; }
void ui_show(ui_t *u) { u->want = true; u->idle = 0; }
/* BOOT (the owner, 2026-10-29): a press brings the bar and it stays; the next press takes it, the card with it */
void ui_toggle(ui_t *u) {
    if (u->want || u->settings) { u->want = false; u->pinned = false; u->settings = false; }
    else { u->want = true; u->pinned = true; u->idle = 0; }
}
bool ui_visible(const ui_t *u) { return u->show > 0.01f || u->settings; }
uint8_t ui_brightness(const world_t *w) { static const uint8_t L[3] = { 70, 150, 255 }; return L[w->set_bright % 3]; }

void ui_tick(ui_t *u, float dt) {
    if (u->settings) { u->want = true; u->idle = 0; }
    if (u->want && !u->pinned) { u->idle += dt; if (u->idle > STAY_S) u->want = false; }
    const float target = u->want ? 1.0f : 0.0f, step = dt / SLIDE_S;
    u->show = u->show < target ? (u->show + step > target ? target : u->show + step) : (u->show - step < target ? target : u->show - step);
}
static int bar_y(const ui_t *u) {                      /* ease-out slide from above the glass */
    const float s = 1 - (1 - u->show) * (1 - u->show);
    return (int)(UI_BAR_Y - (1 - s) * (UI_BAR_H + UI_BAR_Y + 4));
}
/* the rounded rectangle's span on row y (w x h, radius rad): false = outside */
static bool span(int y, int w, int h, int rad, int *x0, int *x1) {
    if (y < 0 || y >= h) return false;
    int inset = 0;
    const int dy = y < rad ? rad - y : y >= h - rad ? y - (h - rad - 1) : 0;
    if (dy) { inset = rad; for (int k = 0; k <= rad; k++) if ((rad - k) * (rad - k) + dy * dy <= rad * rad) { inset = k; break; } }   /* the first column inside the corner's circle */
    *x0 = inset; *x1 = w - inset; return *x1 > *x0;
}
/* the frost: the 8-bit picture behind (w x h at x0, y0) blurred (8 px cells, bilinear), whitened, inside the rounded shape */
static void frost(uint16_t *dst, int w, int h, int rad, const render8_t *r, int x0, int y0) {
    enum { C = 8, GW = UI_CARD_W / C + 2, GH = UI_CARD_H / C + 2 };
    static uint16_t cr[GH][GW], cg[GH][GW], cb[GH][GW];
    const int gw = w / C + 2, gh = h / C + 2;
    for (int gy = 0; gy < gh; gy++) for (int gx = 0; gx < gw; gx++) {
        int sr = 0, sg = 0, sb = 0, n = 0;
        for (int yy = 0; yy < C; yy += 2) for (int xx = 0; xx < C; xx += 2) {
            const int px = x0 + (gx - 1) * C + xx, py = y0 + (gy - 1) * C + yy;
            if (px < 0 || py < 0 || px >= LILI_LOG_W || py >= LILI_LOG_H) continue;
            int rr, g, b; unpack(r->lut[r->px[py * LILI_LOG_W + px]], &rr, &g, &b); sr += rr; sg += g; sb += b; n++;
        }
        cr[gy][gx] = (uint16_t)(n ? sr / n : 0); cg[gy][gx] = (uint16_t)(n ? sg / n : 0); cb[gy][gx] = (uint16_t)(n ? sb / n : 0);
    }
    for (int y = 0; y < h; y++) {
        int a, b; if (!span(y, w, h, rad, &a, &b)) { for (int x = 0; x < w; x++) dst[y * w + x] = 0; continue; }
        const int fy = (y + C / 2) * 256 / C, iy = fy >> 8, ty = fy & 255;           /* fixed point: 8 fraction bits */
        for (int x = 0; x < w; x++) {
            if (x < a || x >= b) { dst[y * w + x] = 0; continue; }
            const int fx = (x + C / 2) * 256 / C, ix = fx >> 8, tx = fx & 255;
            const int w00 = (256 - tx) * (256 - ty), w10 = tx * (256 - ty), w01 = (256 - tx) * ty, w11 = tx * ty;
            #define BL(c) (((c)[iy][ix] * w00 + (c)[iy][ix + 1] * w10 + (c)[iy + 1][ix] * w01 + (c)[iy + 1][ix + 1] * w11) >> 16)
            int R = BL(cr), G = BL(cg), B = BL(cb);
            #undef BL
            R = (R * (256 - TINT_K) + TINT_R * TINT_K) >> 8; G = (G * (256 - TINT_K) + TINT_G * TINT_K) >> 8; B = (B * (256 - TINT_K) + TINT_B * TINT_K) >> 8;
            const bool edge = x == a || x == b - 1 || y == 0 || y == h - 1;
            if (edge) { R = (R * 166 + 255 * 90) >> 8; G = (G * 166 + 255 * 90) >> 8; B = (B * 166 + 255 * 90) >> 8; }   /* a 35 % white rim */
            else if (y <= 2 && x > a + 4 && x < b - 5) { R = (R * 3 + 255) / 4; G = (G * 3 + 255) / 4; B = (B * 3 + 255) / 4; }   /* the light on the top edge */
            dst[y * w + x] = rgb(R, G, B) | 1;                                                       /* never 0: 0 = outside */
        }
    }
}
/* ---- drawing into an RGB565 image ---- */
static void fill(uint16_t *d, int dw, int dh, int x, int y, int w, int h, uint16_t c) {
    for (int yy = y; yy < y + h; yy++) for (int xx = x; xx < x + w; xx++)
        if (xx >= 0 && yy >= 0 && xx < dw && yy < dh && d[yy * dw + xx]) d[yy * dw + xx] = c | 1;
}
static uint32_t utf8_next(const char **s) {
    const uint8_t *p = (const uint8_t *)*s; uint32_t c = *p++;
    if (c >= 0xC0 && c < 0xE0 && *p) c = (c & 0x1F) << 6 | (*p++ & 0x3F);
    else if (c >= 0xE0 && c < 0xF0 && p[0] && p[1]) { c = (c & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F); p += 2; }
    *s = (const char *)p; return c;
}
static int text_w(const char *s, int sc) { int n = 0; while (*s) { utf8_next(&s); n++; } return n * 6 * sc - sc; }
static void text(uint16_t *d, int dw, int dh, int x, int y, int sc, uint16_t c, const char *s) {
    while (*s) {
        uint16_t cols[5];
        if (font_glyph(utf8_next(&s), cols))
            for (int i = 0; i < 5; i++) for (int row = 0; row < 9; row++) if (cols[i] >> row & 1) {
                fill(d, dw, dh, x + i * sc + 1, y + row * sc + 1, sc, sc, rgb(30, 20, 16));   /* a soft shadow under the glyph */
                fill(d, dw, dh, x + i * sc, y + row * sc, sc, sc, c);
            }
        x += 6 * sc;
    }
}
static void gauge(uint16_t *d, int x, int y, float v, uint16_t c) {      /* a 34 x 8 bar, v 0..1 */
    fill(d, UI_BAR_W, UI_BAR_H, x - 1, y - 1, 36, 10, INK);
    fill(d, UI_BAR_W, UI_BAR_H, x, y, 34, 8, rgb(230, 225, 215));
    fill(d, UI_BAR_W, UI_BAR_H, x, y, (int)(34 * (v < 0 ? 0 : v > 1 ? 1 : v) + 0.5f), 8, c);
}
static void icon9(uint16_t *d, int dw, int dh, int x, int y, const uint16_t rows[9], uint16_t c) {
    for (int r = 0; r < 9; r++) for (int k = 0; k < 9; k++) if (rows[r] >> (8 - k) & 1) fill(d, dw, dh, x + 2 * k, y + 2 * r, 2, 2, c);
}
static const uint16_t IC_FOOD[9] = { 0, 0x038, 0x07C, 0, 0x1FF, 0x0FE, 0x0FE, 0x07C, 0 }, IC_BOLT[9] = { 0x018, 0x030, 0x060, 0x0FC, 0x018, 0x030, 0x060, 0x040, 0 },
    IC_HEART[9] = { 0, 0x0C6, 0x1EF, 0x1FF, 0x1FF, 0x0FE, 0x07C, 0x038, 0x010 }, IC_SPK[9] = { 0x004, 0x00C, 0x0DC, 0x0DC, 0x0DC, 0x0DC, 0x0DC, 0x00C, 0x004 },
    IC_GEAR[9] = { 0x038, 0x1BB, 0x1FF, 0x0C6, 0x1C7, 0x0C6, 0x1FF, 0x1BB, 0x038 }, IC_X[9] = { 0, 0x101, 0x082, 0x044, 0x028, 0x044, 0x082, 0x101, 0 };

static void compose_bar(ui_t *u, const world_t *w, const render8_t *r) {
    uint16_t *d = u->bar;
    frost(d, UI_BAR_W, UI_BAR_H, UI_BAR_H / 2, r, BAR_X, bar_y(u));
    lili_date_t dt; life_date(world_now(w), &dt); char t[8]; snprintf(t, sizeof t, "%02d:%02d", dt.hour, dt.minute);
    text(d, UI_BAR_W, UI_BAR_H, 16, 23, 2, WHITE, t);                                   /* the time at the left (2026-10-29) */
    /* Lili's three gauges in the middle, each icon over its bar with a gap (2026-10-29: they ran into each other) */
    static const int GX[3] = { UI_BAR_W / 2 - 48, UI_BAR_W / 2, UI_BAR_W / 2 + 48 };
    const uint16_t *IC[3] = { IC_FOOD, IC_BOLT, IC_HEART };
    const float V[3] = { 1 - w->hunger / 10, w->energy / 10, w->happy / 10 };                /* fed, energy, mood */
    const uint16_t C[3] = { WARM, rgb(250, 220, 90), rgb(240, 110, 120) };
    for (int i = 0; i < 3; i++) { icon9(d, UI_BAR_W, UI_BAR_H, GX[i] - 9, 13, IC[i], WHITE); gauge(d, GX[i] - 17, 39, V[i], C[i]); }
    if (u->has_battery) {                                  /* the battery: its outline, the charge, the % under it (a bolt when charging) */
        fill(d, UI_BAR_W, UI_BAR_H, 254, 15, 30, 16, INK); fill(d, UI_BAR_W, UI_BAR_H, 284, 20, 3, 6, INK);
        fill(d, UI_BAR_W, UI_BAR_H, 256, 17, 26, 12, rgb(230, 225, 215));
        fill(d, UI_BAR_W, UI_BAR_H, 256, 17, (int)(26 * u->battery + 0.5f), 12, u->battery < 0.15f ? rgb(230, 60, 50) : rgb(110, 200, 110));
        char pc[8]; snprintf(pc, sizeof pc, "%d%%", (int)(u->battery * 100 + 0.5f)); text(d, UI_BAR_W, UI_BAR_H, 270 - text_w(pc, 1) / 2, 37, 1, WHITE, pc);
        if (u->charging) icon9(d, UI_BAR_W, UI_BAR_H, 260, 14, IC_BOLT, rgb(255, 240, 120));
    }
    icon9(d, UI_BAR_W, UI_BAR_H, 296, 23, IC_SPK, WHITE);
    if (w->set_muted) icon9(d, UI_BAR_W, UI_BAR_H, 308, 23, IC_X, rgb(240, 90, 80));
    icon9(d, UI_BAR_W, UI_BAR_H, 330, 23, IC_GEAR, WHITE);
}
static const char *ROWS[] = { "Fényerő", "Hang", "Gondolatbuborék", "Keresési segítség", "Idő", "Bezárás" };
enum { ROW_Y0 = 56, ROW_H = 44, N_ROWS = 6 };
static void compose_card(ui_t *u, const world_t *w, const render8_t *r) {
    uint16_t *d = u->card; const int W = UI_CARD_W, H = UI_CARD_H;
    frost(d, W, H, 24, r, CARD_X, UI_CARD_Y);
    text(d, W, H, (W - text_w("Beállítások", 2)) / 2, 16, 2, WHITE, "Beállítások");
    for (int i = 0; i < N_ROWS; i++) {
        const int y = ROW_Y0 + i * ROW_H;
        fill(d, W, H, 20, y - 6, W - 40, 1, rgb(255, 255, 255));
        if (i == N_ROWS - 1) { text(d, W, H, (W - text_w(ROWS[i], 2)) / 2, y + 8, 2, WHITE, ROWS[i]); continue; }
        text(d, W, H, 20, y + 8, 2, WHITE, ROWS[i]);
        char v[16] = "";
        if (i == 0) { for (int k = 0; k < 3; k++) fill(d, W, H, W - 76 + k * 18, y + 8, 14, 18, k <= w->set_bright ? WHITE : rgb(120, 110, 100)); continue; }
        if (i == 1) {
            if (w->set_muted) snprintf(v, sizeof v, "némítva");
            else { for (int k = 0; k < 3; k++) fill(d, W, H, W - 76 + k * 18, y + 8 + (2 - k) * 4, 14, 18 - (2 - k) * 4, k < w->set_volume ? WHITE : rgb(120, 110, 100)); continue; }
        }
        if (i == 2) snprintf(v, sizeof v, "%s", w->bubbles_on ? "be" : "ki");
        if (i == 3) snprintf(v, sizeof v, "%s", w->set_search ? "be" : "ki");
        if (i == 4) { lili_date_t dt; life_date(world_now(w), &dt); if (w->clock_ok) snprintf(v, sizeof v, "%02d:%02d", dt.hour, dt.minute); else snprintf(v, sizeof v, "állítsd be!"); }
        text(d, W, H, W - 20 - text_w(v, 2), y + 8, 2, i == 4 ? rgb(230, 225, 215) : WHITE, v);
    }
}
/* composed again when it moves (the slide), when the card opens or changes, and every 4th frame for what passes
   behind the frost and the clock: the frost is the costly part (spec 7.8: above 25 fps while it shows) */
void ui_prepare(ui_t *u, const world_t *w, const render8_t *r) {
    static int last_y = -999, frame; static bool last_settings; static uint8_t last_set[5];
    const uint8_t set[5] = { w->set_bright, w->set_volume, w->set_muted, w->set_search, w->bubbles_on };
    const bool moved = bar_y(u) != last_y, changed = u->settings != last_settings || memcmp(set, last_set, sizeof set);
    frame++;
    if (u->show > 0.01f && (moved || changed || frame % 4 == 0)) { compose_bar(u, w, r); last_y = bar_y(u); }
    if (u->settings && (changed || frame % 4 == 2)) compose_card(u, w, r);
    last_settings = u->settings; memcpy(last_set, set, sizeof set);
}
void ui_apply_rows(const ui_t *u, uint16_t *dst, int y0, int n) {
    if (u->show > 0.01f) {
        const int by = bar_y(u);
        for (int y = y0 > by ? y0 : by; y < y0 + n && y < by + UI_BAR_H; y++) {
            if (y < 0) continue;
            const uint16_t *s = &u->bar[(y - by) * UI_BAR_W]; uint16_t *o = &dst[(y - y0) * LILI_PHYS_W + BAR_X];
            for (int x = 0; x < UI_BAR_W; x++) if (s[x]) o[x] = s[x];
        }
    }
    if (u->settings)
        for (int y = y0 > UI_CARD_Y ? y0 : UI_CARD_Y; y < y0 + n && y < UI_CARD_Y + UI_CARD_H; y++) {
            const uint16_t *s = &u->card[(y - UI_CARD_Y) * UI_CARD_W]; uint16_t *o = &dst[(y - y0) * LILI_PHYS_W + CARD_X];
            for (int x = 0; x < UI_CARD_W; x++) if (s[x]) o[x] = s[x];
        }
}
bool ui_tap(ui_t *u, world_t *w, int x, int y) {
    if (u->settings) {
        u->idle = 0;
        if (x < CARD_X || x >= CARD_X + UI_CARD_W || y < UI_CARD_Y || y >= UI_CARD_Y + UI_CARD_H) { u->settings = false; return true; }
        const int row = (y - UI_CARD_Y - ROW_Y0 + 6) / ROW_H;
        switch (row) {
        case 0: w->set_bright = (uint8_t)((w->set_bright + 1) % 3); break;
        case 1: if (w->set_muted) { w->set_muted = false; w->set_volume = 1; } else if (w->set_volume >= 3) w->set_muted = true; else w->set_volume++; break;
        case 2: w->bubbles_on = !w->bubbles_on; break;
        case 3: w->set_search = !w->set_search; break;
        case 5: u->settings = false; break;
        default: break;                                 /* the time: the serial console's settime (or Wi-Fi, later) */
        }
        return true;
    }
    if (u->show > 0.5f) {
        const int by = bar_y(u);
        if (y >= by - 16 && y < by + UI_BAR_H + 16 && x >= BAR_X - 8 && x < BAR_X + UI_BAR_W + 8) {   /* a finger's slack around the bar (2026-10-28) */
            u->idle = 0;
            const int bx = x - BAR_X;
            if (bx >= 290 && bx < 324) w->set_muted = !w->set_muted;              /* the speaker and the gear, with the bar's slack */
            else if (bx >= 324) u->settings = true;
            return true;
        }
    }
    return false;
}
