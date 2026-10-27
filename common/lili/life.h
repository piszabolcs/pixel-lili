/* life.h - Lili's fifth layer (spec 9, M5): the needs and their hourly drift, the calendar (dayparts, seasons,
 * the special days), the owner's presence, and the time away (the offline simulation and the scene she is found
 * in). The world's clock is local time in seconds (the RTC on the panel, the PC's clock in the simulator): no
 * time zones and no libc dates here, the civil date is computed (so both platforms agree). Platform-free. */
#ifndef LILI_LIFE_H
#define LILI_LIFE_H
#include "lili_types.h"

/* ---- the calendar (spec 9.3-9.5) ---- */
typedef struct { int year, month, day, hour, minute, second, yday; } lili_date_t;
enum { DAY_NORMAL = 0, DAY_ADOPT, DAY_BDAY, DAY_MEMORIAL };
void    life_date(int64_t local_s, lili_date_t *d);          /* local seconds since 1970-01-01 00:00 -> the civil date */
int64_t life_local(int year, int month, int day, int hour, int minute, int second);
int     life_daypart_of(int hour);                           /* 0 morning 07-12, 1 afternoon 13-17, 2 evening 18-21, 3 night 22-06 */
int     life_season_of(int month);                           /* 0 spring Mar-May .. 3 winter Dec-Feb */
int     life_day_of(int month, int day);                     /* DAY_*: Jan 15 (D-04), Jun 27, Sep 12 */
int64_t world_now(const world_t *w);                         /* the world's local time now */
bool    life_is_dst(int64_t local_s);                        /* EU summer time (the panel's clock follows it: body_216.c) */
int     world_day(const world_t *w);                         /* DAY_* today (forced in the sim, else the date) */

/* ---- the needs (spec 9.1) ---- */
void life_init(world_t *w, int64_t local_now);               /* the needs' starting values, the clock */
void life_tick(world_t *w, float dt);                        /* the hourly drift, by what she is doing and whether the owner is there */
void life_owner(world_t *w);                                 /* the owner did something (a finger, the voice, a shake): present */
bool life_owner_active(const world_t *w);                    /* within the last minute */
enum { LIFE_PET = 1000, LIFE_GREET, LIFE_FORGIVEN, LIFE_CALL, LIFE_FETCH };   /* the reflexes' effects (plan.c maps them) */
void life_after_action(world_t *w, int actor, int act);      /* an action (ACT_*) or a reflex (LIFE_*) ran to its end: what it satisfied */
float life_mischief_p(const world_t *w, int day);            /* H9's chance (spec 9.2), 0 on September 12th */
int  life_bucket(float v);                                   /* 0..10 -> the brain's 0..9 bucket */

/* ---- the time away (spec 9.6) ---- */
enum { SCENE_GREET = 0, SCENE_GUILTY, SCENE_CAUGHT, SCENE_OUTSIDE, SCENE_ASLEEP };
typedef struct { int pees_pad, pees_floor, sleeps, rug_sneaks, meals; } offline_log_t;
/* dt_s away (cut to 30 days), in 10-minute steps at the offline rates, with the statistical events; returns the
   wake-up scene and sets her up in it (place, pose, plan). Deterministic: the world's own seeded PRNG */
int  life_offline(world_t *w, int64_t dt_s, offline_log_t *log);
const char *life_scene_name(int scene);

#endif
