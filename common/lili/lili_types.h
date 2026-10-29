/* lili_types.h - the data every layer of Pixel Lili shares (spec 4.2, 8.1, 11.5).
 * Platform-free; no allocation: everything lives in fixed arrays. */
#ifndef LILI_TYPES_H
#define LILI_TYPES_H
#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "events.h"

typedef enum { VIEW_ROOM = 0, VIEW_YARD = 1, VIEW_COUNT } view_t;

/* the eight sprite directions, clockwise from south (towards the viewer): never mirrored (spec 3.1 L7) */
typedef enum { DIR_S = 0, DIR_SW, DIR_W, DIR_NW, DIR_N, DIR_NE, DIR_E, DIR_SE, DIR_COUNT } dir8_t;

/* one dog. The logic never says "lili": actors[0] is her, actors[1] will be Bella (spec 11.5) */
typedef struct {
    bool    present;
    view_t  view;              /* which place she is in; both places are always simulated (spec 8.1) */
    float   x, y, z;           /* logical px: x across, y depth (the paws' screen row), z height above the floor */
    dir8_t  dir;
    uint16_t anim;             /* ANIM_* (common/asset_ids.h); 0 = none (the capsule placeholder) */
    float   anim_t;            /* seconds into the animation; the frame = anim_t * fps */
    uint16_t want;             /* pose.c: the animation after the move between poses that plays now, 0 = none */
    float   rate;              /* pose.c: how fast anim_t runs (a gait follows her speed), 0 = 1 */
    float   turn_t;            /* pose.c: time towards her next 45 degree step when she turns */
    uint8_t facing;            /* pose.c: 1 = she turns to the next animation's nearest direction before it starts */
    float   vis;               /* 1 seen .. 0 gone: she fades out deep in a bush (a map "hide_" zone) and back (2026-10-23) */
} actor_t;

/* the gaits (spec 8.4): each has a speed and an animation (nav.c) */
typedef enum { STYLE_SLEEPY = 0, STYLE_TIPTOE, STYLE_CURIOUS, STYLE_WALK, STYLE_TROT, STYLE_SCURRY, STYLE_SPRINT, STYLE_COUNT } nav_style_t;

/* how a dog moves through the world: the path it follows (nav.c) */
#define NAV_PATH_MAX 32
typedef struct {
    int16_t  path[NAV_PATH_MAX][2];   /* the waypoints, logical px, after string pulling */
    uint8_t  n, i;                    /* how many, the next one */
    uint8_t  style;                   /* nav_style_t */
    float    speed;                   /* logical px / s along the path: it speeds up, slows into sharp turns and the goal */
    uint8_t  in_hide;                 /* her paws are in a bush's hiding zone (a map "hide_" zone) */
} body_t;

/* the things in the world (spec 8.9, 8.10, 9.2): the two toys, the bowls, the pad, the spots on the floor */
enum { TOY_ROPE = 0, TOY_PLUSH, TOY_COUNT };
#define TOY_AWAY ((view_t)VIEW_COUNT) /* a toy's view when it is in the toy box: in neither view (CONFIG_LILI_PLUSH_AWAY) */
typedef struct {
    view_t  view;
    float   x, y, z, vx, vy, vz;      /* logical px, px / s; z height above the floor */
    int8_t  held_by;                  /* the actor carrying it in her mouth, -1 = loose */
    uint8_t rot;                      /* the way it lies (the rope's 4 pictures); it turns over in the air only */
    float   spin;                     /* px flown since its last turn */
} toy_t;
#define MESS_MAX 4
typedef struct {
    toy_t   toys[TOY_COUNT];
    uint8_t food, water;              /* the bowls: 0 empty .. 3 full (a refill is 3 servings, spec 8.10) */
    uint8_t pad;                      /* 0 clean, 1 used, 2 full */
    uint8_t n_mess; int16_t mess[MESS_MAX][2];   /* the spots on the room's floor (H9) */
    bool    guilty;                   /* H10: she made a spot and has not been forgiven (the last spot cleaned) */
    uint8_t mess_kind[MESS_MAX];      /* each spot: 0 a puddle, 1 a poop (2026-10-28) */
    uint8_t pad_poop;                 /* the pad holds a poop too (then it is full) */
    int8_t  treat_view;               /* a treat on the floor (the owner's long press): its view, -1 = none */
    uint8_t treat_kind;               /* the props sheet's two biscuits */
    int16_t treat_x, treat_y;
} things_t;

/* what the brain decided (spec 10.4): three actions, the gait, the face, the bubble, the urgency */
typedef struct { uint8_t a[3], style, face, bubble, urg; float margin; } decision_t;
enum { PLAN_IDLE = 0, PLAN_RUNNING, PLAN_DONE, PLAN_FAILED };
/* the plan executor's place in a decision (plan.c) */
typedef struct {
    decision_t d;
    uint8_t  status, ai, pi;          /* PLAN_*, the action (0..2), its primitive */
    uint8_t  skipped, rep;            /* the actions skipped (bits), a primitive's repeat count */
    float    t, step_t;               /* seconds in the primitive, in the action (its time limit, spec 8.7) */
    int16_t  gx, gy;                  /* the current walk's goal */
    uint8_t  hide;                    /* 0 out, 1 under the bench (the nook), 2 in a bush */
    uint8_t  prio;                    /* the running reflex's priority (1 = R1 .. 9 = R9), 255 = the brain's plan (spec 8.8) */
    uint8_t  rounds;                  /* R5: the chase rounds so far (at most 4, then she sits down tired) */
    int8_t   toy;                     /* the toy this plan is about (R4: the one thrown), -1 = the nearest */
    float    peek_t;                  /* > 0: peeking out of her hiding place for this long */
    uint8_t  poop;                    /* this business is a poop (decided as it starts, plan.c) */
} plan_t;

typedef struct {
    uint32_t seed;             /* every random number comes from here (spec 4.6: deterministic selftests) */
    uint32_t rng;
    float    clock_s;          /* seconds since init */
    view_t   view;             /* the place the owner is looking at */
    int8_t   season_force;     /* -1 = from the date, 0..3 = forced (the sim's 1-4 keys, spec 9.4) */
    int8_t   daypart_force;    /* -1 = from the clock, 0..3 = forced (the sim's D key, spec 9.3) */
    uint32_t events_seen[EV_COUNT];  /* per ev_type_t: what has reached the world, for tests and the inspector */
    actor_t  actors[MAX_ACTORS];
    body_t   bodies[MAX_ACTORS];   /* bodies[i] moves actors[i] */
    int8_t   shake_cover;          /* the yard cover (IMG_COVER k) that shakes once as she goes into it (spec 8.5), -1 = none */
    float    shake_t;              /* seconds into that shake */
    things_t things;
    plan_t   plans[MAX_ACTORS];    /* plans[i] drives bodies[i] */
    int16_t  sparkle_x, sparkle_y; float sparkle_t;   /* a care tap's sparkle (spec 8.10), seconds left */
    /* the fifth layer (spec 9, life.c): the needs 0..10, the clock, the owner */
    float    hunger, energy, happy, bored, lonely, bladder;
    float    bowel;                /* 0..10, the meals going through: at LIFE_POOP_AT a business is a poop (2026-10-28; not one of the brain's inputs) */
    float    thirst;               /* 0..10 (2026-10-29; not one of the brain's inputs, not saved): with an empty water bowl she asks */
    float    need_ask_t;           /* seconds since she last went to an empty bowl to ask (plan.c reflex_needs) */
    int64_t  epoch0;               /* local seconds (since 1970, no zone) at clock_s = 0: the RTC / the PC's clock */
    float    owner_t;              /* seconds since the owner last did something (a finger, the voice, a shake) */
    int8_t   day_force;            /* -1 = from the date, else DAY_* (the sim's special-day check) */
    uint8_t  last_kind;            /* the brain's "last activity" (F22): LAST_* in life.c */
    float    last_t;               /* seconds in that kind of activity (F23) */
    uint32_t pets, plays;          /* statistics (spec 9.7) */
    /* the sixth layer (spec 10, brain.c): what the next question will say, and when it is asked */
    uint8_t  brain_sig, owner_back;        /* F18's signal and F16's "back after an absence", until the next question */
    float    ask_t;                        /* seconds since the last question */
    uint8_t  ask_now;                      /* a trigger is waiting (spec 10.6) */
    uint8_t  mark[6];                      /* the triggers' last states: hungry, bursting, tired, view, spots, owner */
    int8_t   rule;                         /* the last decision's template (-1 = none) */
    uint8_t  bubble; float bubble_t;       /* the thought bubble over her head (spec 10.4 BB_*, D-09) and how long it stays */
    bool     bubbles_on;                   /* the setting (spec 7.8): the simulator on, the panel after the owner's try (D-09) */
    uint8_t  set_bright, set_volume;       /* the settings card (spec 7.8): brightness 0..2, volume 0..3 */
    bool     set_muted, set_search;        /* the sound off; the search hint (D-06) */
    bool     clock_ok;                     /* the clock is real (an RTC or the PC); false: the settings card asks for the time (M10.7) */
    float    went_t;                       /* seconds since she went through the door to the other place (the search hint) */
    uint8_t  went_view;                    /* the place she was in a tick ago */
    float    margin;                       /* and its top-2 margin */
    bool     finger_down;          /* the finger on the glass (follow_finger), set by the platform each frame */
    int16_t  finger_x, finger_y;
} world_t;

#endif
