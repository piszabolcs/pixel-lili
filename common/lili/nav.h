/* nav.h - how Lili gets somewhere (spec 8.4, M4.1): the walkable grid of each map (4 x 4 logical px cells), an
 * A* search on it with 8 neighbours keeping a margin from the blocked cells, the path pulled straight (string
 * pulling: the farthest cell still in sight), then steering along it ("arrive": an acceleration limit, slowing
 * down into the goal) and the facing quantized to the 8 sprite directions with hysteresis, so she does not
 * flicker between two of them on a diagonal. The gait (style) picks the speed and the animation.
 * No malloc: the search's arrays are the caller's (nav_init), the path is the body's fixed array. */
#ifndef LILI_NAV_H
#define LILI_NAV_H
#include "lili_types.h"
#include "map.h"

enum { NAV_CELLS = MAP_GW * MAP_GH, NAV_HEAP = 2 * NAV_CELLS };
/* the work area: the clearance of both maps (cells to the nearest blocked one) and the search's arrays;
   about 200 KB, so the platform places it (PSRAM on the panel) */
typedef struct {
    uint8_t  clear[VIEW_COUNT][NAV_CELLS];
    uint16_t g[NAV_CELLS];
    int16_t  prev[NAV_CELLS], cells[NAV_CELLS];
    uint8_t  state[NAV_CELLS];
    uint32_t heap[NAV_HEAP];
} nav_scratch_t;

void     nav_init(nav_scratch_t *s);                   /* once, before any plan: measures both maps' clearance */
bool     nav_ready(void);
/* the margin (cells) a dog keeps from walls and things: the yard's sprites are smaller */
int      nav_margin(view_t v);
float    nav_speed(nav_style_t style, view_t v);       /* logical px / s: spec 8.4's design speeds x LILI_K, smaller outdoors */
uint16_t nav_anim(nav_style_t style);                  /* the gait's animation (walk, trot, run, tiptoe, scurry) */
/* a path from (x0, y0) to (x1, y1) in view v into b (b->style is kept); false = the goal is unreachable and the
   path ends at the nearest place she can reach. The goal itself only has to be floor (a bowl's spot may be
   closer to a wall than the margin). */
bool     nav_plan(view_t v, float x0, float y0, int x1, int y1, body_t *b);
/* one step along b's path: steering with the gait's speed, the facing with hysteresis, the gait's animation.
   true = arrived (or no path) */
bool     nav_follow(actor_t *a, body_t *b, float dt);
void     nav_stop(body_t *b);
bool     nav_clear(view_t v, int x, int y, int margin); /* floor here, with margin cells around it */
bool     nav_line_clear(view_t v, float x0, float y0, float x1, float y1, int margin);
dir8_t   nav_dir(float dx, float dy);                  /* a screen vector to the nearest of the 8 directions */
dir8_t   nav_dir_hyst(dir8_t cur, float dx, float dy); /* the same, but it keeps cur until the vector is clearly past it */

#endif
