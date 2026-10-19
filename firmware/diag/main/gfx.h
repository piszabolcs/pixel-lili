/* gfx.h - a 480 x 480 RGB565 frame in PSRAM and the few primitives the diag
 * screens need (fills, rings for the corner test, a 5x9 font with the
 * Hungarian accents). display_port_flush (the pocket-tank's port) ships it. */
#ifndef DIAG_GFX_H
#define DIAG_GFX_H
#include <stdint.h>
#include <stdbool.h>
#define GFX_W 480
#define GFX_H 480
#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_BLACK  RGB(0, 0, 0)
#define C_WHITE  RGB(255, 255, 255)
#define C_GREY   RGB(96, 96, 96)
#define C_DGREY  RGB(40, 40, 40)
#define C_RED    RGB(255, 40, 40)
#define C_YELLOW RGB(255, 220, 0)
#define C_GREEN  RGB(40, 230, 80)
#define C_CYAN   RGB(0, 220, 255)
#define C_BLUE   RGB(60, 120, 255)
#define C_ORANGE RGB(255, 140, 0)
extern uint16_t *gfx_fb;
bool gfx_init(void);
void gfx_clear(uint16_t c);
void gfx_fill(int x, int y, int w, int h, uint16_t c);
void gfx_px(int x, int y, uint16_t c);
void gfx_hline(int x, int y, int w, uint16_t c);
void gfx_vline(int x, int y, int h, uint16_t c);
void gfx_rect(int x, int y, int w, int h, uint16_t c);
/* the ring |dist((x,y),(cx,cy)) - r| < thick/2, inside the clip box [x0,x1) x [y0,y1) */
void gfx_ring(float cx, float cy, float r, float thick, uint16_t c, int x0, int y0, int x1, int y1);
/* text: 5x9 cells scaled by an integer, 1 cell column of spacing; UTF-8 with
 * the Hungarian accented letters; returns the width drawn */
int  gfx_text(int x, int y, int scale, uint16_t c, const char *utf8);
int  gfx_text_w(int scale, const char *utf8);
int  gfx_textf(int x, int y, int scale, uint16_t c, const char *fmt, ...) __attribute__((format(printf, 5, 6)));
/* the font (font5x9.c): 5 columns, bit n = row n (0 = top), rows 0-1 are the accent rows */
int  font_glyph(uint32_t cp, uint16_t cols[5]);   /* 0 = no glyph (a box is drawn) */
#endif
