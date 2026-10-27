/* ui.h - the glass interface (spec 7.8, M10.1-M10.3, M10.5-M10.6): the status bar (a frosted capsule at the top:
 * the time, the battery, Lili's three gauges, the sound, the gear) and the settings card (brightness in 3 steps, the
 * volume and mute, the thought bubbles, the search hint, the time). It is drawn over the finished picture as RGB565
 * rows (the same path as the glass mask), its frost taken from the 8-bit canvas behind it. Hungarian text in the
 * 5x9 pixel font at 2x (font5x9.c). A tap on it is the interface's, not the world's. Platform-free, no malloc. */
#ifndef LILI_UI_H
#define LILI_UI_H
#include <stdbool.h>
#include <stdint.h>
#include "lili_world.h"

#define UI_BAR_W 360
#define UI_BAR_H 64
#define UI_BAR_Y 18
#define UI_CARD_W 360
#define UI_CARD_H 330
#define UI_CARD_Y 78
typedef struct {
    float   show, idle;                    /* the bar's slide (0 up and away .. 1 down) and the seconds untouched */
    bool    want, settings;
    float   battery; bool charging, has_battery;  /* the platform's PMIC reading (no battery: the icon hides) */
    uint16_t bar[UI_BAR_W * UI_BAR_H];     /* the bar, composed each frame it shows (46 KB) */
    uint16_t card[UI_CARD_W * UI_CARD_H];  /* the settings card (232 KB): the platform places the ui_t (PSRAM) */
} ui_t;

void ui_init(ui_t *u);
void ui_show(ui_t *u);                                   /* BOOT, a wake: the bar slides down for 4 s */
void ui_tick(ui_t *u, float dt);
/* compose what shows from the world and the 8-bit canvas (after the world is drawn, before the flush) */
void ui_prepare(ui_t *u, const world_t *w, const render8_t *r);
void ui_apply_rows(const ui_t *u, uint16_t *dst, int y0, int n);    /* over the RGB565 rows, like mask_apply_rows */
/* a tap at (x, y): true = the interface took it (the world does not see it); the settings it changes land in w */
bool ui_tap(ui_t *u, world_t *w, int x, int y);
bool ui_visible(const ui_t *u);
uint8_t ui_brightness(const world_t *w);                 /* the display level for the brightness setting */

#endif
