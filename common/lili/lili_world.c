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

/* ---- the M2.1 placeholder picture ---- */
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

static void fill_logical(uint16_t *fb, int x0, int y0, int w, int h, uint16_t c) {
    for (int y = y0; y < y0 + h; y++) {
        if (y < 0 || y >= LILI_LOG_H) continue;
        for (int x = x0; x < x0 + w; x++) {
            if (x < 0 || x >= LILI_LOG_W) continue;
            for (int sy = 0; sy < LILI_SCALE; sy++)
                for (int sx = 0; sx < LILI_SCALE; sx++)
                    fb[(y * LILI_SCALE + sy) * LILI_PHYS_W + x * LILI_SCALE + sx] = c;
        }
    }
}

void world_render_rgb565(const world_t *w, uint16_t *fb) {
    const uint16_t floor = w->view == VIEW_ROOM ? RGB565(236, 222, 200) : RGB565(120, 176, 88);
    const uint16_t wall  = w->view == VIEW_ROOM ? RGB565(200, 180, 150) : RGB565(70, 120, 60);
    for (int i = 0; i < LILI_PHYS_W * LILI_PHYS_H; i++) fb[i] = floor;
    fill_logical(fb, 0, 0, LILI_LOG_W, 64, wall);      /* the back wall (room) or the hedge (yard), spec 8.2 / 8.3 */
    const actor_t *a = &w->actors[0];
    if (a->present && a->view == w->view) {            /* a capsule dog, 20 x 12, its paws at (x, y) */
        const uint16_t fur = RGB565(226, 170, 110);
        int x = (int)a->x, y = (int)(a->y - a->z);
        fill_logical(fb, x - 10, y - 12, 20, 12, fur);
        fill_logical(fb, x - 8, y - 18, 8, 7, fur);    /* the head */
    }
}
