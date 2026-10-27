/* lili_demo.h - the demo tour (2026-10-23): a fixed route through the painted room and yard so the owner can
 * see the animations, the light and the shadows moving. The simulator (G, --demo, --demo-record) and the
 * panel run the same script. It is a director, not the world's behaviour (that comes in M5): walk / trot to a
 * point along a searched path, play an animation for a while, or set the view, season and daypart. */
#ifndef LILI_DEMO_H
#define LILI_DEMO_H
#include <stdbool.h>
#include <stdint.h>
#include "lili_world.h"
#include "map.h"

void demo_start(void);                         /* nothing without nav_init (the walks need the path search) */
void demo_stop(void);
bool demo_active(void);
int  demo_step(void);                          /* the script's current step, -1 = stopped */
int  demo_rounds(void);                        /* how many times the tour came round to its start */
void demo_tick(world_t *w, float dt);          /* before world_tick; nothing while stopped */

#endif
