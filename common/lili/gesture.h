/* gesture.h - the finger on the glass, turned into the world's events (spec 5.3, M4.3).
 * The platform feeds one sample per frame (down or not, where, when): the panel from its touch chip, the
 * simulator from the mouse on the picture. gesture.c knows nothing of either, nor of the world: what it needs
 * to know about the scene (where Lili is, whether she is bowing, where the toys and the door are) comes in a
 * small context each frame. Out come the events of events.h into the queue:
 *   TAP          < 250 ms, moved < 16 px                      EV_TAP (x, y); on the door (+ 12 px, not on her): EV_SWITCH_VIEW
 *   LONG_PRESS   > 600 ms, moved < 16 px                      EV_LONG (x, y), once, while still down
 *   RUB_PET      over Lili's hitbox + 28 px, >= 3 turns in 1.2 s, > 60 px in all   EV_PET_START / TICK / END
 *   FLICK        starts on a toy (within 56 px of it, not on her; within 26 px always), > 450 px / s at the release
 *                (the last 80 ms)                             EV_FLICK (vx, vy, toy)
 *   DRAG_TOY     starts on a toy, slower                      EV_DRAG (x, y, toy) while it moves, EV_DROP at the end
 *   APPROACH     finger down, nearing her, < 300 px / s, she is bowing              EV_APPROACH (distance)
 *   EDGE_SWIPE   from within 24 px of the top / bottom edge, > 120 px vertically    EV_SWITCH_VIEW
 * Logical px (= physical on the 480 panel), milliseconds. Platform-free, no malloc. */
#ifndef LILI_GESTURE_H
#define LILI_GESTURE_H
#include <stdbool.h>
#include <stdint.h>
#include "events.h"

#define GESTURE_TOYS_MAX 2
#define GESTURE_TOY_R    56                                /* how far from a toy the finger still takes it (the M layer draws it) */
typedef struct { int16_t x0, y0, x1, y1; } g_rect_t;      /* inclusive-exclusive; x1 <= x0 = none */
typedef struct {
    g_rect_t lili;                                         /* her sprite's box on the screen (no box: she is not in this view) */
    bool     bowing;                                       /* she is in a play bow (APPROACH only counts then) */
    int      n_toys; int16_t toy_x[GESTURE_TOYS_MAX], toy_y[GESTURE_TOYS_MAX];   /* the toys in this view, on the screen */
    int8_t   toy_id[GESTURE_TOYS_MAX];                                         /* which toy each is (TOY_*): the yard may hold only the plush */
    g_rect_t door;                                         /* a tap here switches the view (2026-10-21) */
} gesture_ctx_t;

#define GESTURE_HIST 16
typedef struct {
    bool     down;
    uint32_t t0, last_t;                                   /* when the finger landed, the last sample */
    int16_t  x0, y0, x, y;                                 /* where it landed, where it is */
    int32_t  path;                                         /* how far it has travelled, px */
    int8_t   toy;                                          /* the toy it landed on, -1 = none */
    bool     moved, long_sent, swipe_sent, dragging, petting, edge;
    int8_t   sx, sy;                                       /* the last movement's sign per axis (the rub's turns) */
    uint8_t  n_turn; uint32_t turn_t[8];                   /* the turns' times (a ring of the last 8) */
    int32_t  rub_path;                                     /* px travelled over her hitbox this press */
    uint32_t pet_tick_t, approach_t;
    int32_t  approach_d;                                   /* the last distance to her (APPROACH: it must shrink) */
    uint8_t  nh, hi; struct { int16_t x, y; uint32_t t; } h[GESTURE_HIST];   /* the last samples (the flick's speed) */
} gesture_t;

void gesture_init(gesture_t *g);
void gesture_feed(gesture_t *g, const gesture_ctx_t *c, bool down, int x, int y, uint32_t t_ms, ev_queue_t *q);

#endif
