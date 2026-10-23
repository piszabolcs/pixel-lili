/* lili_demo.c - see lili_demo.h. Coordinates are logical px (480). No malloc: the search's arrays are the
 * caller's (demo_init), the path is a fixed array. */
#include "lili_demo.h"
#include "asset_ids.h"
#include <math.h>

enum { D_WALK, D_TROT, D_DO, D_SET };
typedef struct { uint8_t kind; int16_t x, y; uint16_t anim; int8_t dir; float t; int8_t view, season, daypart; } demo_step_t;
static const demo_step_t DEMO[] = {
    { D_SET, 240, 290, 0, 0, 0, VIEW_ROOM, 2, 1 },                                     /* the room, autumn, afternoon */
    { D_DO, 0, 0, ANIM_IDLE_STAND, DIR_SW, 2.5f, -1, -1, -1 },
    { D_WALK, 130, 272, 0, 0, 0, -1, -1, -1 },                                          /* to the rug */
    { D_DO, 0, 0, ANIM_SLEEP_CURL, DIR_SW, 4.0f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.5f, -1, -1, -1 },
    { D_WALK, 250, 330, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PANT_SMILE, DIR_S, 3.0f, -1, -1, -1 },
    /* the bowls: her eating and drinking frames carry a bowl 15.5 px left of her paws (the room's 72 px set);
       she stands so that its bottom lands on the room's bowl (the room's own steps aside meanwhile, lili_world.c prop_in_use) */
    { D_WALK, 273, 450, 0, 0, 0, -1, -1, -1 },                                          /* east of the food bowl */
    { D_DO, 0, 0, ANIM_EAT_DRINK, DIR_W, 3.5f, -1, -1, -1 },
    /* from one bowl to the other she walks off and comes back round, not sideways across the bowls (the owner, 2026-10-23) */
    { D_WALK, 320, 392, 0, 0, 0, -1, -1, -1 },                                          /* away from the food bowl */
    { D_DO, 0, 0, ANIM_IDLE_STAND, DIR_SE, 1.0f, -1, -1, -1 },
    { D_WALK, 392, 451, 0, 0, 0, -1, -1, -1 },                                          /* round to the water bowl's east side */
    { D_WALK, 351, 451, 0, 0, 0, -1, -1, -1 },                                          /* and up to it, heading west */
    { D_DO, 0, 0, ANIM_DRINK, DIR_W, 3.0f, -1, -1, -1 },
    { D_WALK, 300, 400, 0, 0, 0, -1, -1, -1 },
    { D_WALK, 250, 250, 0, 0, 0, -1, -1, -1 },                                          /* towards the door */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.2f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_BARK_HEAD_FLIP, DIR_S, 1.8f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PLAY_BOW, DIR_SW, 2.5f, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, -1, 2 },                                               /* evening: the door's light fades */
    { D_DO, 0, 0, ANIM_IDLE_STAND, DIR_SW, 3.0f, -1, -1, -1 },
    { D_WALK, 135, 262, 0, 0, 0, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, -1, 3 },                                               /* night: only the lamp's short shadows */
    { D_DO, 0, 0, ANIM_SLEEP_CURL, DIR_SW, 4.0f, -1, -1, -1 },
    { D_SET, 236, 456, 0, 0, 0, VIEW_YARD, 0, 1 },                                      /* out to the yard: spring, afternoon */
    { D_WALK, 260, 300, 0, 0, 0, -1, -1, -1 },
    { D_WALK, 404, 246, 0, 0, 0, -1, -1, -1 },                                          /* half into the right bush (spot_bush_2) */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.6f, -1, -1, -1 },                          /* peeking out of it */
    { D_WALK, 446, 236, 0, 0, 0, -1, -1, -1 },                                          /* deep in (hide_bush_2): she fades out */
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.5f, -1, -1, -1 },
    { D_WALK, 380, 300, 0, 0, 0, -1, -1, -1 },                                          /* and out again, fading in */
    { D_SET, -1, -1, 0, 0, 0, -1, 1, -1 },                                              /* summer */
    { D_WALK, 250, 250, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PLAY_BOW, DIR_SW, 2.0f, -1, -1, -1 },
    { D_WALK, 130, 210, 0, 0, 0, -1, -1, -1 },
    { D_TROT, 286, 340, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PANT_SMILE, DIR_S, 2.5f, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, 2, -1 },                                              /* autumn */
    { D_WALK, 186, 122, 0, 0, 0, -1, -1, -1 },                                          /* half behind the top bush (spot_bush_3) */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.2f, -1, -1, -1 },
    { D_WALK, 250, 330, 0, 0, 0, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, 3, -1 },                                              /* winter */
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.5f, -1, -1, -1 },
    { D_WALK, 236, 456, 0, 0, 0, -1, -1, -1 },
};
enum { DEMO_N = (int)(sizeof DEMO / sizeof DEMO[0]) };

static demo_scratch_t *s_scr;
static int s_demo_i = -1, s_demo_rounds; static float s_demo_t;

/* the way there: a breadth-first search on the map's 4 px walkable grid, keeping clear of the blocked shapes
   (the fixed things' invisible walls) by a margin, then the straight stretches merged (line of sight) */
enum { DEMO_PATH_MAX = 256 };
static int16_t s_path[DEMO_PATH_MAX][2]; static int s_path_n, s_path_i;
static bool clear_at(const map_t *m, int x, int y, int r) {
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++)
        if (!map_walkable(m, x + dx * MAP_CELL, y + dy * MAP_CELL)) return false;
    return true;
}
static bool line_clear(const map_t *m, float x0, float y0, float x1, float y1, int r) {
    float dx = x1 - x0, dy = y1 - y0; int n = (int)(sqrtf(dx * dx + dy * dy) / 2) + 1;
    for (int i = 0; i <= n; i++) if (!clear_at(m, (int)(x0 + dx * i / n), (int)(y0 + dy * i / n), r)) return false;
    return true;
}
static void demo_plan(const map_t *m, float x0, float y0, int x1, int y1, int r) {
    int16_t *prev = s_scr->prev, *queue = s_scr->queue, *cells = s_scr->cells;
    const int start = (int)(y0 / MAP_CELL) * MAP_GW + (int)(x0 / MAP_CELL);
    int goal = (y1 / MAP_CELL) * MAP_GW + x1 / MAP_CELL;
    for (int i = 0; i < MAP_GW * MAP_GH; i++) prev[i] = -1;
    int qh = 0, qt = 0, found = -1, best = -1, best_d = 1 << 30;
    prev[start] = (int16_t)start; queue[qt++] = (int16_t)start;
    while (qh < qt) {
        int c = queue[qh++], cx = c % MAP_GW, cy = c / MAP_GW;
        int d = (cx - goal % MAP_GW) * (cx - goal % MAP_GW) + (cy - goal / MAP_GW) * (cy - goal / MAP_GW);
        if (d < best_d) { best_d = d; best = c; }                   /* the goal itself may be unreachable: the nearest then */
        if (c == goal) { found = c; break; }
        for (int k = 0; k < 8; k++) {
            static const int8_t DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
            int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 0 || ny < 0 || nx >= MAP_GW || ny >= MAP_GH) continue;
            int n = ny * MAP_GW + nx;
            if (prev[n] >= 0 || !clear_at(m, nx * MAP_CELL + 2, ny * MAP_CELL + 2, r)) continue;
            prev[n] = (int16_t)c; queue[qt++] = (int16_t)n;
        }
    }
    if (found < 0) found = best;
    int nc = 0;
    for (int c = found; nc < MAP_GW * MAP_GH; c = prev[c]) { cells[nc++] = (int16_t)c; if (c == start) break; }
    float ax = x0, ay = y0; s_path_n = 0; s_path_i = 0;
    int i = nc - 1;
    while (i > 0 && s_path_n < DEMO_PATH_MAX - 1) {                     /* the farthest cell still in sight, one after the other */
        int j = 0;
        while (j < i && !line_clear(m, ax, ay, cells[j] % MAP_GW * MAP_CELL + 2, cells[j] / MAP_GW * MAP_CELL + 2, r)) j++;
        if (j == i) j = i - 1 > 0 ? i - 1 : 0;
        ax = cells[j] % MAP_GW * MAP_CELL + 2; ay = cells[j] / MAP_GW * MAP_CELL + 2;
        s_path[s_path_n][0] = (int16_t)ax; s_path[s_path_n][1] = (int16_t)ay; s_path_n++;
        i = j;
    }
    /* the margin keeps her off the walls on the way; the spot itself only has to be floor (the bowls stand
       near the floor's bottom edge, closer than the margin) */
    if (found == goal || map_walkable(m, x1, y1)) { s_path[s_path_n][0] = (int16_t)x1; s_path[s_path_n][1] = (int16_t)y1; s_path_n++; }
}

static dir8_t dir_of(float dx, float dy) {             /* screen vector -> dir8 (S = down, clockwise) */
    float a = atan2f(-dx, dy) * 57.29578f; if (a < 0) a += 360.0f;
    return (dir8_t)((int)(a / 45.0f + 0.5f) & 7);
}

void demo_init(demo_scratch_t *scratch) { s_scr = scratch; }
void demo_start(void) { if (s_scr) { s_demo_i = 0; s_demo_t = 0; s_demo_rounds = 0; } }
void demo_stop(void) { s_demo_i = -1; }
bool demo_active(void) { return s_demo_i >= 0; }
int  demo_step(void) { return s_demo_i; }
int  demo_rounds(void) { return s_demo_rounds; }

void demo_tick(world_t *w, float dt) {
    if (s_demo_i < 0) return;
    actor_t *l = &w->actors[0];
    const demo_step_t *s = &DEMO[s_demo_i];
    bool done = false;
    if (s->kind == D_SET) {
        s_path_n = s_path_i = 0;
        if (s->view >= 0) { w->view = (view_t)s->view; l->view = (view_t)s->view; }
        if (s->season >= 0) w->season_force = s->season;
        if (s->daypart >= 0) w->daypart_force = s->daypart;
        if (s->x >= 0) { l->x = s->x; l->y = s->y; }
        done = true;
    } else if (s->kind == D_DO) {
        if (s_demo_t == 0) { l->anim = s->anim; l->dir = (dir8_t)s->dir; l->anim_t = 0; }
        s_demo_t += dt; done = s_demo_t >= s->t;
    } else {
        const uint16_t anim = s->kind == D_TROT ? ANIM_TROT : ANIM_WALK;
        const bool yard = w->view == VIEW_YARD;                              /* 0.625 the size outdoors, 0.75 in the room: slower in px */
        const float speed = (s->kind == D_TROT ? 95.0f : 48.0f) * (yard ? 0.7f : 0.8f);   /* logical px per second */
        if (s_demo_t == 0) { demo_plan(world_map(w, w->view), l->x, l->y, s->x, s->y, yard ? 2 : 3); s_demo_t = 1e-6f; }
        if (l->anim != anim) { l->anim = anim; l->anim_t = 0; }
        float step = speed * dt;
        while (step > 0 && s_path_i < s_path_n) {
            float dx = s_path[s_path_i][0] - l->x, dy = s_path[s_path_i][1] - l->y, dist = sqrtf(dx * dx + dy * dy);
            if (dist > 0.5f) l->dir = dir_of(dx, dy);
            if (dist <= step) { l->x = s_path[s_path_i][0]; l->y = s_path[s_path_i][1]; step -= dist; s_path_i++; }
            else { l->x += dx / dist * step; l->y += dy / dist * step; step = 0; }
        }
        done = s_path_i >= s_path_n;
    }
    if (done) { s_demo_i = (s_demo_i + 1) % DEMO_N; s_demo_t = 0; if (!s_demo_i) s_demo_rounds++; }
}
