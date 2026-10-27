/* body_216.h - the panel's senses and energy states for the Lili world (see body_216.c) */
#ifndef LILI_BODY_216_H
#define LILI_BODY_216_H
#include <stdbool.h>
#include "lili_world.h"
enum { BODY_ACTIVE = 0, BODY_DIM, BODY_OFF };
void body_init(void);                                     /* after display_port_init (its I2C bus) */
bool body_poll(world_t *w, ev_queue_t *q, float dt);      /* once a frame: false = the screen is off (skip the drawing) */
void body_owner(world_t *w);                              /* the owner did something: ACTIVE */
int  body_state(void);
#endif
