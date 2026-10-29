/* plan.h - the plan executor (spec 8.7, M4.4): the brain's three actions, each a short list of primitives
 * (go to a place, play an animation for a while or once, wait, pick up / drop a toy, change view, hide, peek,
 * follow the finger, a gait, an effect on the world). An action whose precondition fails is skipped; if all
 * three are, the plan fails at once (a new question for the brain). Each action has a time limit (20 s).
 * Platform-free, no malloc. */
#ifndef LILI_PLAN_H
#define LILI_PLAN_H
#include "lili_types.h"
#include "schema_gen.h"

#define PLAN_STEP_LIMIT_S 20.0f

void        plan_start(world_t *w, int actor, const decision_t *d);
int         plan_tick(world_t *w, int actor, float dt);          /* PLAN_RUNNING / PLAN_DONE / PLAN_FAILED (PLAN_IDLE: none) */
bool        plan_possible(const world_t *w, int actor, int act);  /* the action's precondition, now */
void        plan_one(world_t *w, int actor, int act, int style); /* a decision of one action (the inspector, the tests) */
nav_style_t plan_style(int st, int urg);                          /* the schema's style (ST_*) and the urgency -> the gait */
bool        world_sun_on(const world_t *w);                       /* the sunbeam is on the room's floor (spec 8.2: afternoon) */
int         plan_describe(const world_t *w, int actor, char *buf, int n);
/* the reflexes (spec 8.8, M4.5): an event answered at once, past the brain, if its priority allows (a running
   reflex of a higher priority is not interrupted); true = a reflex took it. R6's greeting comes from the
   platform (a wake after more than 15 minutes away), R1's release from the last spot cleaned (M4.7) */
bool        reflex_event(world_t *w, const ev_t *e);
bool        reflex_greet(world_t *w);
bool        reflex_forgiven(world_t *w);
bool        reflex_needs(world_t *w);                             /* hungry / thirsty at an empty bowl: she asks (2026-10-29) */
bool        plan_asleep(const world_t *w, int actor);

#endif
