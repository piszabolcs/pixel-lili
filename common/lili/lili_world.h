/* lili_world.h - the world's life cycle (M2.1 skeleton). The simulator and the
 * firmware each own a world_t and call these; neither knows the other exists. */
#ifndef LILI_WORLD_H
#define LILI_WORLD_H
#include "lili_types.h"

void     world_init(world_t *w, uint32_t seed);
void     world_tick(world_t *w, float dt);             /* the body: physics, plans, needs (later layers) */
void     world_switch_view(world_t *w);                /* KEY3 / Tab / an edge swipe */
uint32_t world_rand(world_t *w);                       /* the one PRNG (xorshift32), seeded */

/* M2.1 placeholder output, straight to an RGB565 frame of LILI_PHYS_W x LILI_PHYS_H;
 * M2.2 replaces it with the 8-bit logical buffer and its flush */
void     world_render_rgb565(const world_t *w, uint16_t *fb);

#endif
