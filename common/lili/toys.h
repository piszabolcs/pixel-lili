/* toys.h - the rope and the plush (spec 8.9, M4.6): thrown by a flick, they fly, thud and slide to a stop on
 * the floor, the walkable grid's edge is a wall, and they never leave their view except in her mouth. In her
 * mouth they follow her head. Logical px (480), seconds. Platform-free, no malloc. */
#ifndef LILI_TOYS_H
#define LILI_TOYS_H
#include "lili_types.h"

void toys_init(world_t *w);                                     /* the starting spots (the room's rope_start, plush_start) */
void toys_away(world_t *w);                                     /* the plush into the toy box (CONFIG_LILI_PLUSH_AWAY): in neither view */
uint8_t toy_rot_held(dir8_t d);                                 /* the rope's picture across her mouth as she faces d */
void toys_tick(world_t *w, float dt);                           /* the flight and the slide; a carried toy follows her mouth */
void toy_throw(world_t *w, int toy, float fvx, float fvy);      /* a flick's finger velocity (px / s) -> the toy's (spec 8.9) */
void toy_pick(world_t *w, int toy, int actor);                  /* into her mouth */
void toy_drop(world_t *w, int toy);                             /* out of it, onto the floor in front of her */
int  toy_held(const world_t *w, int actor);                     /* the toy in her mouth, -1 = none */
int  toy_near(const world_t *w, int actor, float r);            /* the nearest loose toy in her view within r px, -1 = none */
void toy_mouth(const actor_t *a, float *dx, float *h);          /* her mouth: dx across from the paws, h above them */

#endif
