/* life.c - see life.h. The rates are spec 9.1's, per hour, by name in config.h's spirit (here: LIFE_*). */
#include "life.h"
#include "pose.h"
#include "plan.h"
#include "toys.h"
#include "nav.h"
#include "maps_gen.h"
#include "asset_ids.h"
#include "schema_gen.h"
#include "lili_world.h"
#include <string.h>

/* ---- the calendar: the civil date from days since 1970 (H. Hinnant's algorithm), no libc, no zone ---- */
static int64_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400), doy = (unsigned)((153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}
int64_t life_local(int year, int month, int day, int hour, int minute, int second) {
    return days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
}
void life_date(int64_t s, lili_date_t *d) {
    int64_t z = s >= 0 ? s / 86400 : (s - 86399) / 86400, sec = s - z * 86400;
    const int64_t days = z;
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097), yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    d->day = (int)(doy - (153 * mp + 2) / 5 + 1); d->month = (int)(mp < 10 ? mp + 3 : mp - 9);
    d->year = (int)((int64_t)yoe + era * 400 + (d->month <= 2));
    d->hour = (int)(sec / 3600); d->minute = (int)(sec / 60 % 60); d->second = (int)(sec % 60);
    d->yday = (int)(days - days_from_civil(d->year, 1, 1));
}
int life_daypart_of(int h) { return h >= 7 && h < 13 ? 0 : h >= 13 && h < 18 ? 1 : h >= 18 && h < 22 ? 2 : 3; }
int life_season_of(int m) { return m >= 3 && m <= 5 ? 0 : m >= 6 && m <= 8 ? 1 : m >= 9 && m <= 11 ? 2 : 3; }
int life_day_of(int m, int d) { return m == 1 && d == 15 ? DAY_ADOPT : m == 6 && d == 27 ? DAY_BDAY : m == 9 && d == 12 ? DAY_MEMORIAL : DAY_NORMAL; }
/* EU summer time (spec 5.6: CET-1CEST,M3.5.0,M10.5.0/3): from the last Sunday of March 02:00 to the last Sunday
   of October 03:00, local time (as the clock showed it before the change) */
bool life_is_dst(int64_t s) {
    lili_date_t d; life_date(s, &d);
    if (d.month < 3 || d.month > 10) return false;
    if (d.month > 3 && d.month < 10) return true;
    const int64_t d31 = days_from_civil(d.year, d.month, 31);
    const int wd31 = (int)(((d31 + 4) % 7 + 7) % 7);           /* 0 = Sunday (1970-01-01 was a Thursday) */
    const int64_t at = (d31 - wd31) * 86400 + (d.month == 3 ? 2 : 3) * 3600;
    return d.month == 3 ? s >= at : s < at;
}
int64_t world_now(const world_t *w) { return w->epoch0 + (int64_t)w->clock_s; }
int world_day(const world_t *w) {
    if (w->day_force >= 0 && w->day_force <= DAY_MEMORIAL) return w->day_force;
    lili_date_t d; life_date(world_now(w), &d); return life_day_of(d.month, d.day);
}

/* ---- the needs (spec 9.1, per hour) ---- */
#define LIFE_HUNGER_AWAKE    0.35f
#define LIFE_HUNGER_OFF      0.30f
#define LIFE_ENERGY_PLAY    -1.5f
#define LIFE_ENERGY_IDLE    -0.4f
#define LIFE_ENERGY_SLEEP    2.5f
#define LIFE_HAPPY_PULL      0.5f      /* towards 7 (towards 5 when lonely > 7) */
#define LIFE_BORED_ALONE     1.2f
#define LIFE_BORED_OFF       0.8f
#define LIFE_LONELY_OWNER   -2.0f
#define LIFE_LONELY_ALONE    0.6f
#define LIFE_BLADDER         0.45f
#define LIFE_BOWEL           0.08f     /* a slow drift; a meal is the most of it (2026-10-28) */
#define LIFE_BOWEL_MEAL      3.5f
#define LIFE_OWNER_S         60.0f     /* the owner counts as there for a minute after anything they did */

static float clamp10(float v) { return v < 0 ? 0 : v > 10 ? 10 : v; }
static float toward(float v, float to, float step) { return v < to ? (v + step > to ? to : v + step) : (v - step < to ? to : v - step); }
int life_bucket(float v) { int b = (int)(v * 0.999f); return b < 0 ? 0 : b > 9 ? 9 : b; }

void life_init(world_t *w, int64_t now) {
    w->epoch0 = now - (int64_t)w->clock_s;
    w->hunger = 2; w->energy = 8; w->happy = 7; w->bored = 2; w->lonely = 2; w->bladder = 2; w->bowel = 2; w->things.treat_view = -1;
    w->owner_t = 1e6f; w->day_force = -1; w->last_kind = 7; w->last_t = 0;
}
void life_owner(world_t *w) { w->owner_t = 0; }
bool life_owner_active(const world_t *w) { return w->owner_t < LIFE_OWNER_S; }

static bool playing(uint16_t a) { return a == ANIM_RUN || a == ANIM_TROT || a == ANIM_PLAY_BOW || a == ANIM_POUNCE || a == ANIM_TUG_ROPE || a == ANIM_CARRY_TROT || a == ANIM_SCURRY; }

void life_tick(world_t *w, float dt) {
    const float h = dt / 3600.0f;
    const actor_t *a = &w->actors[0];
    const bool asleep = a->anim == ANIM_SLEEP_CURL, owner = life_owner_active(w);
    w->owner_t += dt; w->last_t += dt;
    w->hunger  = clamp10(w->hunger + (asleep ? LIFE_HUNGER_OFF : LIFE_HUNGER_AWAKE) * h);
    w->energy  = clamp10(w->energy + (asleep ? LIFE_ENERGY_SLEEP : playing(a->anim) ? LIFE_ENERGY_PLAY : LIFE_ENERGY_IDLE) * h);
    w->happy   = clamp10(toward(w->happy, w->lonely > 7 ? 5.0f : 7.0f, LIFE_HAPPY_PULL * h));
    if (!asleep) w->bored = clamp10(w->bored + (owner ? -LIFE_BORED_ALONE : LIFE_BORED_ALONE) * h);
    w->lonely  = clamp10(w->lonely + (owner ? LIFE_LONELY_OWNER : LIFE_LONELY_ALONE) * h);
    w->bladder = clamp10(w->bladder + LIFE_BLADDER * h);
    w->bowel   = clamp10(w->bowel + LIFE_BOWEL * h);
}

/* the brain's F22 "last activity" kinds (model/schema_lili.json: last_play .. last_none) */
enum { LAST_PLAY = 0, LAST_REST, LAST_EAT, LAST_EXPLORE, LAST_GREET, LAST_MISCHIEF, LAST_SOCIAL, LAST_NONE };
static void kind(world_t *w, int k) { if (w->last_kind != k) { w->last_kind = (uint8_t)k; w->last_t = 0; } }

void life_after_action(world_t *w, int actor, int act) {
    switch (act) {
    case ACT_EAT:            w->hunger = 0; w->bladder = clamp10(w->bladder + 2); w->bowel = clamp10(w->bowel + LIFE_BOWEL_MEAL); kind(w, LAST_EAT); break;
    case ACT_DRINK:          w->bladder = clamp10(w->bladder + 1); kind(w, LAST_EAT); break;
    case ACT_USE_PAD:        w->bladder = 0; if (w->plans[actor].poop) w->bowel = 0; kind(w, LAST_REST); break;
    case ACT_MISCHIEF_FLOOR: w->bladder = 0; if (w->plans[actor].poop) w->bowel = 0; kind(w, LAST_MISCHIEF); break;
    case ACT_PICKUP_ROPE: case ACT_PICKUP_PLUSH: case ACT_BRING_TOY: case ACT_CHEW_TOY: case ACT_PLAY_BOW: case ACT_ZOOMIES:
    case ACT_RUN_WITH_TOY: case ACT_TOSS_TOY: case ACT_CHASE_LEAF: case ACT_CHASE_BUTTERFLY: case ACT_PLAY_SNOW:
        w->bored = clamp10(w->bored - 3); w->happy = clamp10(w->happy + 0.5f); w->plays++; kind(w, LAST_PLAY); break;
    case ACT_WANDER: case ACT_SNIFF_AROUND: case ACT_GO_BUSH: case ACT_GO_TREE: case ACT_SNIFF_FLOWERS: case ACT_ENTER_YARD:
        w->bored = clamp10(w->bored - 1); kind(w, LAST_EXPLORE); break;                     /* real exploring: a little less bored */
    case ACT_GO_DOOR: case ACT_LOOK_AROUND: case ACT_ENTER_ROOM: case ACT_PEEK_BUSH: case ACT_GO_TOY:
        kind(w, LAST_EXPLORE); break;
    case ACT_SLEEP_NOOK: case ACT_SLEEP_RUG: case ACT_NAP_SUNBEAM: case ACT_SIT: case ACT_LIE_DOWN: case ACT_SNEAK_RUG:
    case ACT_GO_SUNBEAM: case ACT_GO_RUG: case ACT_GO_NOOK: case ACT_PANT_IDLE:
        kind(w, LAST_REST); break;
    case ACT_GOTO_FRONT: case ACT_FOLLOW_FINGER: case ACT_ASK_PET: case ACT_BEG: case ACT_WAG: case ACT_BARK: case ACT_HUFF: case ACT_HEAD_TILT:
        kind(w, LAST_SOCIAL); break;
    case LIFE_PET:      w->happy = clamp10(w->happy + 1); w->lonely = clamp10(w->lonely - 2); w->bored = clamp10(w->bored - 1); w->pets++; kind(w, LAST_SOCIAL); break;
    case LIFE_GREET:    w->happy = clamp10(w->happy + 2); w->lonely = clamp10(w->lonely - 3); kind(w, LAST_GREET); break;
    case LIFE_FORGIVEN: w->happy = clamp10(w->happy + 2); kind(w, LAST_SOCIAL); break;
    case LIFE_CALL:     w->lonely = clamp10(w->lonely - 1); kind(w, LAST_SOCIAL); break;
    case LIFE_FETCH:    w->bored = clamp10(w->bored - 3); w->happy = clamp10(w->happy + 1); w->plays++; kind(w, LAST_PLAY); break;
    default: break;
    }
}

/* ---- the time away (spec 9.6) ---- */
#define OFF_STEP_S   600
#define OFF_MAX_S    (30 * 86400)
const char *life_scene_name(int s) {
    static const char *const N[] = { "greet", "guilty", "caught on the rug", "outside", "asleep in the nook" };
    return s >= 0 && s <= SCENE_ASLEEP ? N[s] : "?";
}
static float rnd01(world_t *w) { return (world_rand(w) >> 8) * (1.0f / 16777216.0f); }
/* H9's chance (spec 9.2): bored, lonely and a full pad make the floor likelier; never on September 12th */
float life_mischief_p(const world_t *w, int day) {
    if (day == DAY_MEMORIAL) return 0;
    float p = 0.05f + 0.08f * (w->bored > 5 ? w->bored - 5 : 0) + 0.08f * (w->lonely > 5 ? w->lonely - 5 : 0) + (w->things.pad >= 2 ? 0.5f : 0);
    return p > 0.85f ? 0.85f : p;
}
static void place(world_t *w, view_t v, int spot) {
    actor_t *a = &w->actors[0]; const map_point_t *p = &MAPS[v].points[spot];
    a->view = v; if (p->z >= 0) { a->x = p->x; a->y = p->y; }
}

int life_offline(world_t *w, int64_t dt, offline_log_t *log) {
    offline_log_t dummy; if (!log) log = &dummy;
    memset(log, 0, sizeof *log);
    if (dt <= 0) return SCENE_GREET;
    if (dt > OFF_MAX_S) dt = OFF_MAX_S;
    const int64_t end = world_now(w) + dt, start = end - dt;
    bool asleep = false, on_rug = false;
    things_t *t = &w->things;
    for (int64_t s = start; s + OFF_STEP_S <= end; s += OFF_STEP_S) {
        lili_date_t d; life_date(s, &d);
        const float h = OFF_STEP_S / 3600.0f; const int dp = life_daypart_of(d.hour), day = life_day_of(d.month, d.day);
        /* asleep at night, or when worn out; awake when rested by day */
        const bool want_sleep = dp == 3 || w->energy < 2;
        if (want_sleep && !asleep) { asleep = true; log->sleeps++; }
        else if (!want_sleep && asleep && w->energy > 8) asleep = false;
        w->hunger  = clamp10(w->hunger + LIFE_HUNGER_OFF * h);
        w->energy  = clamp10(w->energy + (asleep ? LIFE_ENERGY_SLEEP : LIFE_ENERGY_IDLE) * h);
        w->happy   = clamp10(toward(w->happy, w->lonely > 7 ? 5.0f : 7.0f, LIFE_HAPPY_PULL * h));
        w->bored   = clamp10(w->bored + LIFE_BORED_OFF * h);
        w->lonely  = clamp10(w->lonely + LIFE_LONELY_ALONE * h);
        w->bladder = clamp10(w->bladder + LIFE_BLADDER * h);
        w->bowel   = clamp10(w->bowel + LIFE_BOWEL * h);
        if (w->hunger > 5 && t->food && !asleep) { t->food--; w->hunger = 0; w->bladder = clamp10(w->bladder + 2); w->bowel = clamp10(w->bowel + LIFE_BOWEL_MEAL); log->meals++; }
        if (w->bladder >= 8) {                                /* H8 / H9 (spec 9.2) */
            if (rnd01(w) < life_mischief_p(w, day)) {
                int n = 1 + (int)(world_rand(w) % 2);
                for (int k = 0; k < n && t->n_mess < 3; k++) {
                    int16_t x, y;                             /* next to the pad, on the bare floor */
                    const map_point_t *pad = &MAPS[VIEW_ROOM].points[SPOT_PAD];
                    for (int tries = 0; tries < 30; tries++) {
                        x = (int16_t)(pad->x + 60 + (int)(world_rand(w) % 140)); y = (int16_t)(pad->y - 90 + (int)(world_rand(w) % 100));
                        if (nav_clear(VIEW_ROOM, x, y, 0)) {
                            t->mess_kind[t->n_mess] = k == 0 && w->bowel >= LIFE_POOP_AT;   /* the first spot a poop when the meals have gone through */
                            t->mess[t->n_mess][0] = x; t->mess[t->n_mess][1] = y; t->n_mess++; break;
                        }
                    }
                }
                t->guilty = true; log->pees_floor++;
            } else { if (t->pad < 2) t->pad++; if (w->bowel >= LIFE_POOP_AT) { t->pad_poop = 1; t->pad = 2; } log->pees_pad++; }
            if (w->bowel >= LIFE_POOP_AT) w->bowel = 0;
            w->bladder = 0;
        }
        if (!on_rug && w->lonely >= 7 && w->bored >= 6 && rnd01(w) < 0.08f) { on_rug = true; log->rug_sneaks++; }   /* H7, the rug */
    }
    w->clock_s += (float)dt;                                  /* the world's clock moves on by the time away */
    /* the scene she is found in (spec 9.6 step 4) */
    lili_date_t d; life_date(end, &d);
    const int dp = life_daypart_of(d.hour), season = life_season_of(d.month);
    actor_t *a = &w->actors[0]; plan_t *p = &w->plans[0];
    p->status = PLAN_IDLE; p->hide = 0; p->peek_t = 0;
    for (int i = 0; i < TOY_COUNT; i++) if (t->toys[i].held_by >= 0) toy_drop(w, i);
    if (t->n_mess) {                                          /* the room looks empty: she peeks from under the bench */
        place(w, VIEW_ROOM, SPOT_NOOK); a->dir = DIR_S; pose_snap(a, ANIM_GUILTY_PEEK); p->hide = 1; p->peek_t = 4; t->guilty = true;
        return SCENE_GUILTY;
    }
    if (on_rug) {                                             /* asleep where she should not be: up with a start */
        place(w, VIEW_ROOM, SPOT_RUG); pose_snap(a, ANIM_SLEEP_CURL);
        plan_one(w, 0, ACT_LEAVE_RUG, ST_SCURRY);
        return SCENE_CAUGHT;
    }
    if (dp <= 2 && !(season == 3 && dp == 3) && w->energy > 5 && rnd01(w) < 0.4f) {   /* out in the yard, sniffing or in a bush */
        if (world_rand(w) & 1) { place(w, VIEW_YARD, SPOT_GRASS); pose_snap(a, ANIM_SNIFF); }
        else { place(w, VIEW_YARD, world_rand(w) & 1 ? SPOT_BUSH_2 : SPOT_BUSH_3); plan_one(w, 0, ACT_HIDE_BUSH, ST_WALK); }
        return SCENE_OUTSIDE;
    }
    if (dp == 3) { place(w, VIEW_ROOM, SPOT_NOOK); pose_snap(a, ANIM_SLEEP_CURL); a->dir = DIR_S; return SCENE_ASLEEP; }
    place(w, VIEW_ROOM, SPOT_CENTER); pose_snap(a, ANIM_IDLE_STAND);
    reflex_greet(w);
    return SCENE_GREET;
}
