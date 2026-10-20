/* lili_types.h - the data every layer of Pixel Lili shares (spec 4.2, 8.1, 11.5).
 * Platform-free; no allocation: everything lives in fixed arrays. */
#ifndef LILI_TYPES_H
#define LILI_TYPES_H
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef enum { VIEW_ROOM = 0, VIEW_YARD = 1, VIEW_COUNT } view_t;

/* the eight sprite directions, clockwise from south (towards the viewer): never mirrored (spec 3.1 L7) */
typedef enum { DIR_S = 0, DIR_SW, DIR_W, DIR_NW, DIR_N, DIR_NE, DIR_E, DIR_SE, DIR_COUNT } dir8_t;

/* one dog. The logic never says "lili": actors[0] is her, actors[1] will be Bella (spec 11.5) */
typedef struct {
    bool    present;
    view_t  view;              /* which place she is in; both places are always simulated (spec 8.1) */
    float   x, y, z;           /* logical px: x across, y depth (the paws' screen row), z height above the floor */
    dir8_t  dir;
} actor_t;

typedef struct {
    uint32_t seed;             /* every random number comes from here (spec 4.6: deterministic selftests) */
    uint32_t rng;
    float    clock_s;          /* seconds since init */
    view_t   view;             /* the place the owner is looking at */
    int8_t   season_force;     /* -1 = from the date, 0..3 = forced (the sim's 1-4 keys, spec 9.4) */
    int8_t   daypart_force;    /* -1 = from the clock, 0..3 = forced (the sim's D key, spec 9.3) */
    uint32_t events_seen[17];  /* per ev_type_t (17 = EV_COUNT): what has reached the world, for tests and the inspector */
    actor_t  actors[MAX_ACTORS];
} world_t;

#endif
