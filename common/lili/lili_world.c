/* lili_world.c - M2.1: an empty room and yard, and Lili as a placeholder capsule
 * (spec 7.7: nothing waits for the art). Platform-free C99, no malloc. */
#include "lili_world.h"
#include <string.h>

uint32_t world_rand(world_t *w) {                      /* xorshift32: fast, tiny, and the same sequence on every platform */
    uint32_t x = w->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return w->rng = x;
}

void world_init(world_t *w, uint32_t seed) {
    memset(w, 0, sizeof *w);
    w->seed = seed;
    w->rng = seed ? seed : 0x2545F491u;                /* xorshift must never hold 0 */
    w->view = VIEW_ROOM;
    actor_t *lili = &w->actors[0];
    lili->present = true;
    lili->view = VIEW_ROOM;
    lili->x = LILI_LOG_W / 2.0f;                       /* spec 8.2 spot_center */
    lili->y = 140.0f;
    lili->dir = DIR_S;
}

void world_tick(world_t *w, float dt) {
    w->clock_s += dt;
}

void world_switch_view(world_t *w) {
    w->view = (view_t)((w->view + 1) % VIEW_COUNT);
}

/* ---- the placeholder picture (spec 7.7): the map's props as shaded boxes, Lili as a capsule ---- */
#include "palette.h"
#include "maps_gen.h"

const map_t *world_map(const world_t *w, view_t v) { (void)w; return &MAPS[v]; }

/* a prop seen from the 3/4 camera: its footprint (x, y, w, h) on the floor, raised by `height`:
   the top face is lighter, the front face darker, a 1 px outline around both (spec 7.1) */
/* the owner's rug (photo reference/photos/rug_owner_room_2026-10-19.jpg, 300 x 200 cm, lying
   lengthwise into the room). Its pattern as 12 triangles in rug coordinates (u across 0..1,
   v along 0..1, from the far end): green top-left, dark grey top-right, a big off-white band
   across the upper middle, light grey down the left, dark grey in the middle and right, dark
   grey bottom-left, green bottom-right. The vertices: the corners, two points on each edge,
   and two inside (P, Q); the 12 triangles tile the rectangle exactly. */
enum { RUG_GREEN, RUG_WHITE, RUG_LIGHT, RUG_DARK };
static const float RUG_V[12][2] = {
    { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 },                 /* A B C D: the corners */
    { 0.55f, 0 }, { 1, 0.35f }, { 1, 0.75f }, { 0.45f, 1 }, /* E F G H: on the top, right, right, bottom edges */
    { 0, 0.62f }, { 0, 0.28f },                             /* I J: on the left edge */
    { 0.5f, 0.3f }, { 0.5f, 0.68f },                        /* P Q: inside */
};
enum { VA, VB, VC, VD, VE, VF, VG, VH, VI, VJ, VP, VQ };
static const uint8_t RUG_T[12][4] = {                       /* three vertices and a colour */
    { VA, VE, VJ, RUG_GREEN }, { VE, VB, VF, RUG_DARK },  { VJ, VE, VP, RUG_WHITE }, { VE, VF, VP, RUG_WHITE },
    { VJ, VP, VI, RUG_LIGHT }, { VP, VF, VQ, RUG_DARK },  { VF, VG, VQ, RUG_DARK },  { VI, VP, VQ, RUG_LIGHT },
    { VI, VQ, VH, RUG_LIGHT }, { VI, VH, VD, RUG_DARK },  { VQ, VG, VH, RUG_LIGHT }, { VG, VC, VH, RUG_GREEN },
};
static float edge(const float *a, const float *b, float u, float v) { return (b[0] - a[0]) * (v - a[1]) - (b[1] - a[1]) * (u - a[0]); }
static int rug_colour(float u, float v) {                   /* the triangle holding (u, v): all three edge functions agree in sign */
    for (int t = 0; t < 12; t++) {
        const float *a = RUG_V[RUG_T[t][0]], *b = RUG_V[RUG_T[t][1]], *c = RUG_V[RUG_T[t][2]];
        float e0 = edge(a, b, u, v), e1 = edge(b, c, u, v), e2 = edge(c, a, u, v);
        if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) return RUG_T[t][3];
    }
    return RUG_LIGHT;
}
/* the speckle (a shaggy pile) comes from a hash of the pixel's own position, so it is the
   same in every frame: no flicker, no PRNG state */
static void draw_rug(render8_t *r, const map_prop_t *p) {
    const uint8_t col[4] = { pal_ramp(RAMP_ID_RUG_GREEN, 5), pal_ramp(RAMP_ID_RUG_GRAY, 7), pal_ramp(RAMP_ID_RUG_GRAY, 5), pal_ramp(RAMP_ID_RUG_GRAY, 2) };
    r8_fill(r, p->x, p->y, p->w, p->h, pal_ramp(RAMP_ID_RUG_GRAY, 1));          /* the bound edge */
    for (int y = 1; y < p->h - 1; y++)
        for (int x = 1; x < p->w - 1; x++) {
            uint8_t c = col[rug_colour((x + 0.5f) / p->w, (y + 0.5f) / p->h)];
            uint32_t hsh = (uint32_t)(p->x + x) * 73856093u ^ (uint32_t)(p->y + y) * 19349663u;
            if ((hsh >> 7) % 6 == 0) c = (uint8_t)(c - 1);                            /* a darker tuft: one shade down the ramp */
            int lx = p->x + x, ly = p->y + y;
            if (lx >= 0 && ly >= 0 && lx < LILI_LOG_W && ly < LILI_LOG_H) r->px[ly * LILI_LOG_W + lx] = c;
        }
}

static void draw_prop(render8_t *r, const map_prop_t *p) {
    if (p->pattern == 1) { draw_rug(r, p); return; }
    if (p->flat) {                                          /* a door, a pad: drawn on the floor/wall */
        r8_fill(r, p->x, p->y, p->w, p->h, PAL_OUTLINE);
        r8_fill(r, p->x + 1, p->y + 1, p->w - 2, p->h - 2, pal_ramp(p->ramp, 5));
        return;
    }
    int top = p->y - p->height;
    r8_fill(r, p->x - 1, top - 1, p->w + 2, p->h + p->height + 2, PAL_OUTLINE);
    r8_fill(r, p->x, top, p->w, p->h, pal_ramp(p->ramp, 6));                    /* top face */
    r8_fill(r, p->x, top + p->h, p->w, p->height, pal_ramp(p->ramp, 3));        /* front face */
}

static void draw_actor(render8_t *r, const actor_t *a) {    /* a capsule dog, its paws at (x, y) */
    int x = (int)a->x, y = (int)(a->y - a->z);
    r8_fill(r, x - 11, y - 13, 22, 14, PAL_OUTLINE);
    r8_fill(r, x - 10, y - 12, 20, 12, PAL_FUR + 5);
    r8_fill(r, x - 6, y - 7, 12, 7, PAL_FUR_W + 5);         /* the white chest (L3) */
    r8_fill(r, x - 9, y - 19, 10, 8, PAL_OUTLINE);
    r8_fill(r, x - 8, y - 18, 8, 6, PAL_FUR + 6);           /* the head */
}

void world_render(const world_t *w, render8_t *r) {
    const map_t *m = world_map(w, w->view);
    r8_clear(r, pal_ramp(m->floor_ramp, 5));
    r8_fill(r, 0, 0, LILI_LOG_W, m->wall_bottom, pal_ramp(m->wall_ramp, 4));
    r8_fill(r, 0, m->wall_bottom - 1, LILI_LOG_W, 1, pal_ramp(m->wall_ramp, 2));   /* the wall's foot */
    /* the Y-sort (spec 7.2 step 3, 8.5): flat props first, then everything by its front edge,
       so a prop in front of Lili covers her and one behind her is covered. Fixed arrays, no malloc. */
    enum { MAXD = 32 }; int16_t key[MAXD]; int8_t who[MAXD]; int n = 0;   /* who: prop index, or -1 - actor */
    for (int i = 0; i < m->n_props && n < MAXD; i++) {
        if (m->props[i].flat) { draw_prop(r, &m->props[i]); continue; }
        key[n] = m->props[i].sort_y; who[n++] = (int8_t)i;
    }
    for (int a = 0; a < MAX_ACTORS && n < MAXD; a++)
        if (w->actors[a].present && w->actors[a].view == w->view) { key[n] = (int16_t)w->actors[a].y; who[n++] = (int8_t)(-1 - a); }
    for (int i = 1; i < n; i++)                              /* insertion sort: n is a dozen, and it is stable */
        for (int j = i; j > 0 && key[j - 1] > key[j]; j--) {
            int16_t tk = key[j]; key[j] = key[j - 1]; key[j - 1] = tk;
            int8_t tw = who[j]; who[j] = who[j - 1]; who[j - 1] = tw;
        }
    for (int i = 0; i < n; i++) {
        if (who[i] >= 0) draw_prop(r, &m->props[who[i]]);
        else draw_actor(r, &w->actors[-1 - who[i]]);
    }
}

void world_debug_draw(const world_t *w, render8_t *r) {      /* M: the walkable grid and the points (spec 6.3) */
    const map_t *m = world_map(w, w->view);
    for (int gy = 0; gy < MAP_GH; gy++)
        for (int gx = 0; gx < MAP_GW; gx++)
            if (!map_walkable(m, gx * MAP_CELL + 1, gy * MAP_CELL + 1)) r->px[(gy * MAP_CELL + 2) * LILI_LOG_W + gx * MAP_CELL + 2] = PAL_DEBUG;
    for (int s = 0; s < SPOT_COUNT; s++) {
        const map_point_t *p = &m->points[s];
        if (p->z < 0) continue;
        r8_fill(r, p->x - 2, p->y - p->z, 5, 1, PAL_WHITE);
        r8_fill(r, p->x, p->y - p->z - 2, 1, 5, PAL_WHITE);
    }
}
