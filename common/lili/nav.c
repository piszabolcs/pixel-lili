/* nav.c - see nav.h. Coordinates are logical px (480), the grid is MAP_CELL px cells. */
#include "nav.h"
#include "maps_gen.h"
#include "asset_ids.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static nav_scratch_t *s_nav;

/* spec 8.4's speeds, design px / s (the 240 grid); the gait's animation */
static const struct { float speed; uint16_t anim; } GAIT[STYLE_COUNT] = {
    [STYLE_SLEEPY] = { 18, ANIM_WALK }, [STYLE_TIPTOE] = { 20, ANIM_TIPTOE }, [STYLE_CURIOUS] = { 26, ANIM_WALK },
    [STYLE_WALK] = { 32, ANIM_WALK }, [STYLE_TROT] = { 48, ANIM_TROT }, [STYLE_SCURRY] = { 64, ANIM_SCURRY },
    [STYLE_SPRINT] = { 90, ANIM_RUN },
};
/* the yard is painted smaller (its sprites are 0.625 of the masters, the room's 0.75): she covers less ground
   per step there, so the same gait is slower in px */
#define NAV_YARD_SCALE (0.625f / 0.75f)

float nav_speed(nav_style_t st, view_t v) {
    if ((unsigned)st >= STYLE_COUNT) st = STYLE_WALK;
    return GAIT[st].speed * LILI_K * LILI_NAV_SPEED_SCALE * (v == VIEW_YARD ? NAV_YARD_SCALE : 1.0f);
}
uint16_t nav_anim(nav_style_t st) { return GAIT[(unsigned)st < STYLE_COUNT ? st : STYLE_WALK].anim; }
int nav_margin(view_t v) { return v == VIEW_YARD ? 2 : 3; }
bool nav_ready(void) { return s_nav != NULL; }

/* the clearance: for every cell, how many cells to the nearest blocked one (the chessboard distance; outside
   the map counts as blocked), two passes over the grid. nav_clear(v, x, y, m) is then one look-up. */
static void measure(view_t v) {
    const map_t *m = &MAPS[v]; uint8_t *d = s_nav->clear[v];
    #define D(x, y) ((x) < 0 || (y) < 0 || (x) >= MAP_GW || (y) >= MAP_GH ? 0 : d[(y) * MAP_GW + (x)])
    for (int y = 0; y < MAP_GH; y++) for (int x = 0; x < MAP_GW; x++) {
        int c = y * MAP_GW + x;
        if (!map_walkable(m, x * MAP_CELL + MAP_CELL / 2, y * MAP_CELL + MAP_CELL / 2)) { d[c] = 0; continue; }
        const int nb[4] = { D(x - 1, y), D(x, y - 1), D(x - 1, y - 1), D(x + 1, y - 1) };
        int k = nb[0];
        for (int i = 1; i < 4; i++) if (nb[i] < k) k = nb[i];
        d[c] = (uint8_t)(k + 1 > 255 ? 255 : k + 1);
    }
    for (int y = MAP_GH - 1; y >= 0; y--) for (int x = MAP_GW - 1; x >= 0; x--) {
        int c = y * MAP_GW + x; if (!d[c]) continue;
        int k = d[c];
        const int nb[4] = { D(x + 1, y), D(x, y + 1), D(x + 1, y + 1), D(x - 1, y + 1) };
        for (int i = 0; i < 4; i++) if (nb[i] + 1 < k) k = nb[i] + 1;
        d[c] = (uint8_t)k;
    }
    #undef D
}
void nav_init(nav_scratch_t *s) { s_nav = s; if (s) for (int v = 0; v < VIEW_COUNT; v++) measure((view_t)v); }

static int cell_of(int x, int y) {
    if (x < 0 || y < 0 || x >= LILI_LOG_W || y >= LILI_LOG_H) return -1;
    return (y / MAP_CELL) * MAP_GW + x / MAP_CELL;
}
bool nav_clear(view_t v, int x, int y, int margin) {
    int c = cell_of(x, y);
    if (c < 0) return false;
    if (!s_nav) return map_walkable(&MAPS[v], x, y);
    return s_nav->clear[v][c] > margin;
}
bool nav_line_clear(view_t v, float x0, float y0, float x1, float y1, int margin) {
    float dx = x1 - x0, dy = y1 - y0; int n = (int)(sqrtf(dx * dx + dy * dy) / 2) + 1;
    for (int i = 0; i <= n; i++) if (!nav_clear(v, (int)(x0 + dx * i / n), (int)(y0 + dy * i / n), margin)) return false;
    return true;
}

/* the open set: a binary heap of (f << 14 | cell), lazily (a cell may sit in it more than once; the stale copy
   is skipped when it comes out). 14 bits hold the 14 400 cells, f stays far below 2^18 on a 120 x 120 grid. */
static int s_hn;
static void heap_push(uint32_t k) {
    uint32_t *h = s_nav->heap; if (s_hn >= NAV_HEAP) return;     /* full (never on these maps): the search just sees less */
    int i = s_hn++;
    while (i > 0 && h[(i - 1) / 2] > k) { h[i] = h[(i - 1) / 2]; i = (i - 1) / 2; }
    h[i] = k;
}
static uint32_t heap_pop(void) {
    uint32_t *h = s_nav->heap, top = h[0], last = h[--s_hn]; int i = 0;
    for (;;) {
        int c = 2 * i + 1; if (c >= s_hn) break;
        if (c + 1 < s_hn && h[c + 1] < h[c]) c++;
        if (h[c] >= last) break;
        h[i] = h[c]; i = c;
    }
    h[i] = last; return top;
}
static uint32_t octile(int c, int gx, int gy) {        /* straight 10, diagonal 14 */
    int dx = abs(c % MAP_GW - gx), dy = abs(c / MAP_GW - gy);
    return (uint32_t)(10 * (dx + dy) - 6 * (dx < dy ? dx : dy));
}

bool nav_plan(view_t v, float x0, float y0, int x1, int y1, body_t *b) {
    b->n = b->i = 0;
    if (!s_nav) return false;
    const uint8_t *cl = s_nav->clear[v];
    const int margin = nav_margin(v);
    int start = cell_of((int)x0, (int)y0), goal = cell_of(x1, y1);
    if (start < 0 || goal < 0) return false;
    const int gx = goal % MAP_GW, gy = goal / MAP_GW;
    memset(s_nav->state, 0, sizeof s_nav->state);
    s_hn = 0;
    s_nav->g[start] = 0; s_nav->prev[start] = (int16_t)start; s_nav->state[start] = 1;
    heap_push(octile(start, gx, gy) << 14 | (uint32_t)start);
    int found = -1, best = start; uint32_t best_h = octile(start, gx, gy);
    static const int8_t DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    while (s_hn > 0) {
        const int c = (int)(heap_pop() & 0x3FFF);
        if (s_nav->state[c] == 2) continue;                          /* a stale copy */
        s_nav->state[c] = 2;
        const uint32_t h = octile(c, gx, gy);
        if (h < best_h) { best_h = h; best = c; }                    /* the goal may be unreachable: the nearest then */
        if (c == goal) { found = c; break; }
        const int cx = c % MAP_GW, cy = c / MAP_GW;
        for (int k = 0; k < 8; k++) {
            const int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 0 || ny < 0 || nx >= MAP_GW || ny >= MAP_GH) continue;
            const int n = ny * MAP_GW + nx;
            if (s_nav->state[n] == 2 || !cl[n]) continue;
            /* keep the margin; inside it (she starts or ends near a wall: a bowl) only towards more room or along it */
            if (cl[n] <= margin && n != goal && !(cl[c] <= margin && cl[n] >= cl[c])) continue;
            if (k >= 4 && (!cl[cy * MAP_GW + nx] || !cl[ny * MAP_GW + cx])) continue;   /* no squeezing past a corner */
            uint32_t g = s_nav->g[c] + (k < 4 ? 10u : 14u); if (g > 0xFFFF) g = 0xFFFF;
            if (s_nav->state[n] == 1 && g >= s_nav->g[n]) continue;
            s_nav->g[n] = (uint16_t)g; s_nav->prev[n] = (int16_t)c; s_nav->state[n] = 1;
            heap_push((g + octile(n, gx, gy)) << 14 | (uint32_t)n);
        }
    }
    const int end = found >= 0 ? found : best;
    int nc = 0;                                                       /* the cells, end first */
    for (int c = end; nc < NAV_CELLS; c = s_nav->prev[c]) { s_nav->cells[nc++] = (int16_t)c; if (c == start) break; }
    /* string pulling: from where she is, the farthest cell still in sight, and on from there */
    float ax = x0, ay = y0; int i = nc - 1;
    #define CX(c) ((c) % MAP_GW * MAP_CELL + MAP_CELL / 2)
    #define CY(c) ((c) / MAP_GW * MAP_CELL + MAP_CELL / 2)
    while (i > 0 && b->n < NAV_PATH_MAX - 1) {
        int j = 0;
        while (j < i && !nav_line_clear(v, ax, ay, CX(s_nav->cells[j]), CY(s_nav->cells[j]), margin)) j++;
        if (j == i) j = i - 1;
        ax = CX(s_nav->cells[j]); ay = CY(s_nav->cells[j]);
        b->path[b->n][0] = (int16_t)ax; b->path[b->n][1] = (int16_t)ay; b->n++;
        i = j;
    }
    #undef CX
    #undef CY
    /* the goal point itself (not its cell's centre), when it is floor and in a straight line from the path's end */
    if ((found >= 0 || nav_clear(v, x1, y1, 0)) && nav_line_clear(v, ax, ay, (float)x1, (float)y1, 0)) {
        if (b->n == NAV_PATH_MAX) b->n--;
        b->path[b->n][0] = (int16_t)x1; b->path[b->n][1] = (int16_t)y1; b->n++;
        if (b->n >= 2 && b->path[b->n - 2][0] / MAP_CELL == x1 / MAP_CELL && b->path[b->n - 2][1] / MAP_CELL == y1 / MAP_CELL) {
            b->path[b->n - 2][0] = (int16_t)x1; b->path[b->n - 2][1] = (int16_t)y1; b->n--;   /* the goal's own cell centre: drop it */
        }
        return found >= 0;
    }
    return false;
}

void nav_stop(body_t *b) { b->n = b->i = 0; b->speed = 0; }

dir8_t nav_dir(float dx, float dy) {                   /* S = down the screen, clockwise */
    float a = atan2f(-dx, dy) * 57.29578f; if (a < 0) a += 360.0f;
    return (dir8_t)((int)(a / 45.0f + 0.5f) & 7);
}
dir8_t nav_dir_hyst(dir8_t cur, float dx, float dy) {
    float a = atan2f(-dx, dy) * 57.29578f; if (a < 0) a += 360.0f;
    float off = fabsf(a - cur * 45.0f); if (off > 180.0f) off = 360.0f - off;
    return off <= 22.5f + 10.0f ? cur : nav_dir(dx, dy);  /* 10 degrees past the sector's edge before she turns */
}

bool nav_follow(actor_t *a, body_t *b, float dt) {
    if (b->i >= b->n) { b->speed = 0; return true; }
    const float vmax = nav_speed((nav_style_t)b->style, a->view), acc = vmax * 4.0f;
    const uint16_t anim = nav_anim((nav_style_t)b->style);
    if (a->anim != anim) { a->anim = anim; a->anim_t = 0; }
    float step;
    {   /* the speed she aims for: full, less into a sharp turn at the next waypoint, and down to a walk into the goal */
        const float dx = b->path[b->i][0] - a->x, dy = b->path[b->i][1] - a->y, dist = sqrtf(dx * dx + dy * dy);
        float want = vmax;
        if (b->i == b->n - 1) { float s = dist * 3.0f; if (s < want) want = s; if (want < 12.0f) want = 12.0f; }
        else {
            const float ex = b->path[b->i + 1][0] - b->path[b->i][0], ey = b->path[b->i + 1][1] - b->path[b->i][1], el = sqrtf(ex * ex + ey * ey);
            if (dist > 0.5f && el > 0.5f && (dx * ex + dy * ey) / (dist * el) < 0.5f) {   /* a turn of more than 60 degrees ahead */
                float s = dist * 3.0f; if (s < vmax * 0.4f) s = vmax * 0.4f; if (s < want) want = s;
            }
        }
        if (b->speed < want) { b->speed += acc * dt; if (b->speed > want) b->speed = want; }
        else { b->speed -= acc * dt; if (b->speed < want) b->speed = want; }
        step = b->speed * dt;
    }
    while (step > 0 && b->i < b->n) {                   /* along the polyline exactly: the steering never cuts a corner into a wall */
        const float dx = b->path[b->i][0] - a->x, dy = b->path[b->i][1] - a->y, dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.5f) a->dir = nav_dir_hyst(a->dir, dx, dy);
        if (dist <= step) { a->x = b->path[b->i][0]; a->y = b->path[b->i][1]; step -= dist; b->i++; }
        else { a->x += dx / dist * step; a->y += dy / dist * step; step = 0; }
    }
    if (b->i >= b->n) { b->n = b->i = 0; b->speed = 0; return true; }
    return false;
}
