/* lili_world.h - the world's life cycle. The simulator and the firmware each own
 * a world_t and a render8_t and call these; neither knows the other exists. */
#ifndef LILI_WORLD_H
#define LILI_WORLD_H
#include "lili_types.h"
#include "render8.h"

void     world_init(world_t *w, uint32_t seed);
void     world_tick(world_t *w, float dt);             /* the body: physics, plans, needs (later layers) */
void     world_switch_view(world_t *w);                /* KEY3 / Tab / an edge swipe */
uint32_t world_rand(world_t *w);                       /* the one PRNG (xorshift32), seeded */

/* the shown view into the 8-bit canvas; the caller flushes it (r8_flush_rows) */
void     world_render(const world_t *w, render8_t *r);

#endif
