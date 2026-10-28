/* lili_world.h - the world's life cycle. The simulator and the firmware each own
 * a world_t and a render8_t and call these; neither knows the other exists. */
#ifndef LILI_WORLD_H
#define LILI_WORLD_H
#include "lili_types.h"
#include "render8.h"
#include "map.h"
#include "events.h"

#include "pak.h"
void     world_init(world_t *w, uint32_t seed);
/* the asset pack the world draws from (NULL or an unopened pak = the placeholder shapes, spec 7.7);
   the world keeps the pointer, the platform keeps the bytes */
void     world_set_pak(const pak_t *p);
const pak_t *world_pak(void);
void     world_tick(world_t *w, float dt);             /* the body: physics, plans, needs (later layers) */
void     world_switch_view(world_t *w);                /* KEY3 / Tab / an edge swipe */
void     world_finger(world_t *w, bool down, int x, int y);   /* the finger on the glass, each frame (follow_finger) */
bool     world_care_tap(world_t *w, int x, int y);    /* a tap on a bowl, the pad or a spot (spec 8.10); true = it was one */
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
int      world_season(const world_t *w);               /* 0 spring .. 3 winter: forced, else LILI_SEASON_DEFAULT (the RTC's date from M5) */
int      world_daypart(const world_t *w);              /* 0 morning, 1 afternoon, 2 evening, 3 night: forced, else from the clock */
bool     world_painted(void);                          /* the pak holds the owner's painted backgrounds (else the placeholder shapes) */
/* the sprite frame an actor shows now (offset: ANIM_YARD_OFFSET for the yard's size); a reversed animation
   (ANIM_REVERSE_OF) plays its source's frames backwards. false = no such set in the pak */
bool     world_actor_frame(const actor_t *a, uint16_t offset, sprite_set_t *set, sprite_frame_t *fr);
/* what the finger needs to know about the shown view (gesture.c): her box on the screen, whether she bows,
   the door's tap area */
#include "gesture.h"
void     world_gesture_ctx(const world_t *w, gesture_ctx_t *c);
void     world_put_treat(world_t *w, int x, int y);            /* the owner's long press: a treat on the floor there (2026-10-28) */

#endif
