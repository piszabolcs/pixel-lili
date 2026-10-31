/* critters.h - the yard's small life (2026-10-31, the owner: "the yard is too static, it feels fake"): a sparrow
 * that flies in, lands, pecks and hops about, and flies off when Lili comes near; butterflies on spring and summer
 * days; leaves falling from the two trees in autumn, snow in winter. Nothing sways as a whole picture (2026-10-27: the
 * swaying tree was rejected): these are small things moving over the painting. Their own random numbers (world_t.crit_rng),
 * so they never change Lili's. Not saved: the yard starts empty after a power-on. */
#ifndef LILI_CRITTERS_H
#define LILI_CRITTERS_H
#include "lili_types.h"

enum { CRIT_NONE = 0, CRIT_BIRD, CRIT_BUTTERFLY };
enum { CS_FLY_IN = 0, CS_GROUND, CS_FLEE, CS_WANDER };
void critters_init(world_t *w);
void critters_tick(world_t *w, float dt);          /* called by world_tick */
#endif
