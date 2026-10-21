/* lili_world.h - the world's life cycle. The simulator and the firmware each own
 * a world_t and a render8_t and call these; neither knows the other exists. */
#ifndef LILI_WORLD_H
#define LILI_WORLD_H
#include "lili_types.h"
#include "render8.h"
#include "map.h"
#include "events.h"

void     world_init(world_t *w, uint32_t seed);
void     world_tick(world_t *w, float dt);             /* the body: physics, plans, needs (later layers) */
void     world_switch_view(world_t *w);                /* KEY3 / Tab / an edge swipe */
uint32_t world_rand(world_t *w);                       /* the one PRNG (xorshift32), seeded */
void     world_handle_event(world_t *w, const ev_t *e);/* the one door in: reflexes and plans hang here from M4 on */

/* the shown view into the 8-bit canvas; the caller flushes it (r8_flush_rows).
   world_render = the static layer (floor, wall, flat props: world_render_bg) + the Y-sorted
   rest (world_render_fg). The static layer only changes with world_bg_key, so a frame loop
   can keep it in a bgcache_t and copy it back instead of drawing it every frame (M2.6). */
void     world_render(const world_t *w, render8_t *r);
void     world_render_bg(const world_t *w, render8_t *r);
void     world_render_fg(const world_t *w, render8_t *r);
uint32_t world_bg_key(const world_t *w);               /* changes exactly when the static layer would look different */
typedef struct { uint8_t px[LILI_LOG_W * LILI_LOG_H]; uint32_t key; uint8_t valid; } bgcache_t;   /* 57 600 B: the caller places it */
void     world_render_cached(const world_t *w, render8_t *r, bgcache_t *c);   /* same picture as world_render */
void     world_debug_draw(const world_t *w, render8_t *r);  /* the walkable grid and the points */
const map_t *world_map(const world_t *w, view_t v);

#endif
