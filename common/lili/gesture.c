/* gesture.c - see gesture.h. The thresholds are spec 5.3's starting values, by name. */
#include "gesture.h"
#include "config.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* the owner (2026-10-28): the screen is small (2.16", 12 px a millimetre) and a finger is not; the targets were a few
   millimetres wide. Now they reach out like a phone's keys: the toy is the finger's anywhere within G_TOY_R (9 mm across)
   unless the finger is on her (then she gets the pet), within G_TOY_CORE always; the nearest toy wins; her pet box,
   the door, a still tap and a flick are all a little more forgiving */
enum {
    G_TAP_MS = 250, G_LONG_MS = 600, G_STILL_PX = 16,
    G_PET_PAD = 28, G_PET_TURNS = 3, G_PET_WINDOW_MS = 1200, G_PET_PATH = 60, G_PET_TICK_MS = 250, G_TURN_PX = 4,
    G_TOY_CORE = 26, G_TOY_R = GESTURE_TOY_R, G_FLICK_PXS = 450, G_FLICK_MS = 80,
    G_APPROACH_PXS = 300, G_APPROACH_MS = 200,
    G_EDGE_PX = 24, G_SWIPE_PX = 120, G_DOOR_PAD = 12,
};

void gesture_init(gesture_t *g) { memset(g, 0, sizeof *g); g->toy = -1; }

static bool in_rect(const g_rect_t *r, int x, int y, int pad) {
    return r->x1 > r->x0 && x >= r->x0 - pad && x < r->x1 + pad && y >= r->y0 - pad && y < r->y1 + pad;
}
static int toy_at(const gesture_ctx_t *c, int x, int y) {
    int best = -1, bd = G_TOY_R * G_TOY_R + 1;
    for (int i = 0; i < c->n_toys && i < GESTURE_TOYS_MAX; i++) {
        const int dx = x - c->toy_x[i], dy = y - c->toy_y[i], d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    if (best < 0 || bd <= G_TOY_CORE * G_TOY_CORE) return best;
    return in_rect(&c->lili, x, y, 0) ? -1 : best;
}
/* the finger's velocity over the last G_FLICK_MS: from the newest sample back to the oldest one still in that window */
static void velocity(const gesture_t *g, float *vx, float *vy) {
    *vx = *vy = 0;
    if (g->nh < 2) return;
    int newest = (g->hi + GESTURE_HIST - 1) % GESTURE_HIST, old = newest;
    for (int k = 1; k < g->nh; k++) {
        int i = (newest + GESTURE_HIST - k) % GESTURE_HIST;
        if (g->h[newest].t - g->h[i].t > G_FLICK_MS) break;
        old = i;
    }
    uint32_t dt = g->h[newest].t - g->h[old].t;
    if (!dt) return;
    *vx = (g->h[newest].x - g->h[old].x) * 1000.0f / dt; *vy = (g->h[newest].y - g->h[old].y) * 1000.0f / dt;
}
static int dist_to(const g_rect_t *r, int x, int y) {
    float cx = (r->x0 + r->x1) / 2.0f, cy = (r->y0 + r->y1) / 2.0f;
    return (int)sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
}

void gesture_feed(gesture_t *g, const gesture_ctx_t *c, bool down, int x, int y, uint32_t t, ev_queue_t *q) {
    if (down && !g->down) {                                        /* the finger lands */
        gesture_init(g);
        g->down = true; g->t0 = g->last_t = t; g->x0 = g->x = (int16_t)x; g->y0 = g->y = (int16_t)y;
        g->toy = (int8_t)toy_at(c, x, y);
        g->edge = y < G_EDGE_PX || y >= LILI_LOG_H - G_EDGE_PX;
        g->approach_d = c->lili.x1 > c->lili.x0 ? dist_to(&c->lili, x, y) : 1 << 20;
        g->h[0].x = (int16_t)x; g->h[0].y = (int16_t)y; g->h[0].t = t; g->nh = 1; g->hi = 1;
        return;
    }
    if (!down && !g->down) return;
    if (!down) {                                                   /* the finger lifts */
        const uint32_t held = g->last_t - g->t0;
        if (g->petting) ev_push(q, EV_PET_END, g->x, g->y);
        if (g->toy >= 0) {
            float vx, vy; velocity(g, &vx, &vy);
            if (sqrtf(vx * vx + vy * vy) > G_FLICK_PXS) ev_push3(q, EV_FLICK, (int)vx, (int)vy, g->toy);
            else if (g->dragging) ev_push3(q, EV_DROP, g->x, g->y, g->toy);
        }
        if (!g->moved && !g->long_sent && !g->swipe_sent && held < G_TAP_MS && g->toy < 0)
            ev_push(q, in_rect(&c->door, g->x0, g->y0, G_DOOR_PAD) && !in_rect(&c->lili, g->x0, g->y0, 0) ? EV_SWITCH_VIEW : EV_TAP, g->x0, g->y0);
        g->down = false;
        return;
    }
    /* the finger is down and (maybe) moving */
    const int dx = x - g->x, dy = y - g->y;
    g->path += abs(dx) + abs(dy);
    if (abs(x - g->x0) >= G_STILL_PX || abs(y - g->y0) >= G_STILL_PX) g->moved = true;
    g->h[g->hi].x = (int16_t)x; g->h[g->hi].y = (int16_t)y; g->h[g->hi].t = t;
    g->hi = (uint8_t)((g->hi + 1) % GESTURE_HIST); if (g->nh < GESTURE_HIST) g->nh++;
    if (!g->moved && !g->long_sent && t - g->t0 > G_LONG_MS && g->toy < 0) { ev_push(q, EV_LONG, g->x0, g->y0); g->long_sent = true; }
    if (g->edge && !g->swipe_sent && abs(y - g->y0) > G_SWIPE_PX) { ev_push(q, EV_SWITCH_VIEW, x, y); g->swipe_sent = true; }
    if (g->toy >= 0) {                                             /* a toy under the finger: it follows */
        if (g->moved && (dx || dy)) { ev_push3(q, EV_DRAG, x, y, g->toy); g->dragging = true; }
    } else if (!g->edge) {
        /* the rub: the finger over her (+16 px) turning back and forth */
        const bool over = in_rect(&c->lili, x, y, G_PET_PAD);
        if (over) {
            g->rub_path += abs(dx) + abs(dy);
            const int8_t sx = dx > G_TURN_PX ? 1 : dx < -G_TURN_PX ? -1 : 0, sy = dy > G_TURN_PX ? 1 : dy < -G_TURN_PX ? -1 : 0;
            if ((sx && g->sx && sx != g->sx) || (sy && g->sy && sy != g->sy)) g->turn_t[g->n_turn++ % 8] = t;
            if (sx) g->sx = sx;
            if (sy) g->sy = sy;
            int recent = 0;
            for (int i = 0; i < 8 && i < g->n_turn; i++) recent += t - g->turn_t[i] <= G_PET_WINDOW_MS;
            if (!g->petting && recent >= G_PET_TURNS && g->rub_path > G_PET_PATH) { g->petting = true; g->pet_tick_t = t; ev_push(q, EV_PET_START, x, y); }
            else if (g->petting && t - g->pet_tick_t >= G_PET_TICK_MS) { g->pet_tick_t = t; ev_push(q, EV_PET_TICK, x, y); }
        } else if (g->petting) { g->petting = false; ev_push(q, EV_PET_END, x, y); }
        /* the approach: nearing her slowly while she bows */
        if (c->bowing && c->lili.x1 > c->lili.x0 && t - g->approach_t >= G_APPROACH_MS) {
            const int d = dist_to(&c->lili, x, y);
            float vx, vy; velocity(g, &vx, &vy);
            if (d < g->approach_d && sqrtf(vx * vx + vy * vy) < G_APPROACH_PXS) { ev_push(q, EV_APPROACH, d, 0); g->approach_t = t; }
            g->approach_d = d;
        }
    }
    g->x = (int16_t)x; g->y = (int16_t)y; g->last_t = t;
}
