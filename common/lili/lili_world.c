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

/* ---- the placeholder picture (spec 7.7), now in palette indices ---- */
#include "palette.h"

void world_render(const world_t *w, render8_t *r) {
    const int room = w->view == VIEW_ROOM;
    r8_clear(r, (uint8_t)((room ? PAL_FLOOR : PAL_GRASS) + 5));
    r8_fill(r, 0, 0, LILI_LOG_W, 32, (uint8_t)((room ? PAL_WALL : PAL_HEDGE) + 4));   /* the back wall / the hedge, spec 8.2 / 8.3 */
    const actor_t *a = &w->actors[0];
    if (a->present && a->view == w->view) {            /* a capsule dog, its paws at (x, y) */
        int x = (int)a->x, y = (int)(a->y - a->z);
        r8_fill(r, x - 11, y - 13, 22, 14, PAL_OUTLINE);
        r8_fill(r, x - 10, y - 12, 20, 12, PAL_FUR + 5);
        r8_fill(r, x - 6, y - 7, 12, 7, PAL_FUR_W + 5);  /* the white chest (L3) */
        r8_fill(r, x - 9, y - 19, 10, 8, PAL_OUTLINE);
        r8_fill(r, x - 8, y - 18, 8, 6, PAL_FUR + 6);    /* the head */
    }
}
