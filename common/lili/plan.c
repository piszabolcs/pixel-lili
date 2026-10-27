/* plan.c - see plan.h. The action table is the body's half of the brain's vocabulary: every action of
 * model/schema_lili.json is here, with its precondition and its primitives. Animations the PixelLab round has not
 * made yet stand in with the nearest ones (huff: a head tilt; the pickup: a sniff; the bench peek: a look round). */
#include "plan.h"
#include "nav.h"
#include "toys.h"
#include "maps_gen.h"
#include "asset_ids.h"
#include "pak.h"
#include "lili_world.h"
#include "gesture.h"
#include "life.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { P_END = 0, P_GOTO, P_ANIM, P_ONCE, P_WAIT, P_DIR, P_PICK, P_DROP, P_TOSS, P_VIEW, P_HIDE, P_UNHIDE, P_PEEK, P_FOLLOW, P_STYLE, P_FX, P_LOOK };
/* walk targets: a map spot (its SPOT_* id), or one of these */
enum { TG_FRONT = 40, TG_RANDOM, TG_RANDOM_FAR, TG_TOY, TG_ROPE, TG_PLUSH, TG_DOOR, TG_SUNBEAM, TG_BUSH_HIDE, TG_BUSH_EDGE,
       TG_BOWL_FOOD, TG_BOWL_WATER, TG_FLOOR_AWAY, TG_INSIDE, TG_TAP };
enum { FX_EAT = 1, FX_DRINK, FX_PAD, FX_MESS, FX_BARK, FX_HUFF, FX_FORGIVE };
enum {                                        /* preconditions (spec 8.7, 10.5's context mask) */
    N_ROOM = 1 << 0, N_YARD = 1 << 1, N_TOY_VIEW = 1 << 2, N_MOUTH = 1 << 3, N_FREE_MOUTH = 1 << 4, N_FOOD = 1 << 5,
    N_WATER = 1 << 6, N_SUN = 1 << 7, N_NO_MEMORIAL = 1 << 8, N_HIDDEN = 1 << 9, N_AUTUMN = 1 << 10, N_WARM = 1 << 11,
    N_WINTER = 1 << 12, N_ROPE = 1 << 13, N_PLUSH = 1 << 14, N_ON_RUG = 1 << 15, N_TOY_ANY = 1 << 16,
};
typedef struct { uint8_t op, arg; uint16_t anim; float t; } prim_t;
#define GO(tg)      { P_GOTO, tg, 0, 0 }
#define AN(a, t)    { P_ANIM, 0, a, t }
#define ON(a)       { P_ONCE, 0, a, 0 }
#define WT(t)       { P_WAIT, 0, 0, t }
#define DR(d)       { P_DIR, d, 0, 0 }
#define FX(e)       { P_FX, e, 0, 0 }
#define ST(s)       { P_STYLE, s, 0, 0 }
#define PICK(toy)   { P_PICK, toy, 0, 0 }
#define DROP        { P_DROP, 0, 0, 0 }
#define TOSS        { P_TOSS, 0, 0, 0 }
#define VIEW        { P_VIEW, 0, 0, 0 }
#define HIDE(p)     { P_HIDE, p, 0, 0 }
#define UNHIDE      { P_UNHIDE, 0, 0, 0 }
#define PEEK(t)     { P_PEEK, 0, 0, t }
#define FOLLOW(t)   { P_FOLLOW, 0, 0, t }
#define ANYTOY 255
#define LOOK(tg)    { P_LOOK, tg, 0, 0 }
#define PRIMS 12
typedef struct { uint32_t need; prim_t p[PRIMS]; } act_t;
/* the reflexes' own sequences, after the brain's actions in the same table (spec 8.8) */
enum { RX_FORGIVEN = ACT_COUNT, RX_CALL, RX_CALL_OTHER, RX_PET, RX_PET_END, RX_FETCH, RX_TEASE, RX_TIRED, RX_GREET, RX_GREET_GUILTY,
       RX_SHAKE, RX_VOICE, RX_TAP_LOOK, RX_TAP_GO, RX_JOY, RX_END };

static const act_t ACTS[RX_END] = {
    /* moving about */
    [ACT_GOTO_FRONT]     = { 0,                     { GO(TG_FRONT), DR(DIR_S), AN(ANIM_IDLE_SIT_BLINK, 2.0f) } },
    [ACT_FOLLOW_FINGER]  = { 0,                     { FOLLOW(6.0f), AN(ANIM_PANT_SMILE, 1.5f) } },
    [ACT_WANDER]         = { 0,                     { GO(TG_RANDOM), WT(0.8f), GO(TG_RANDOM), AN(ANIM_IDLE_STAND, 1.0f) } },
    [ACT_SNIFF_AROUND]   = { 0,                     { GO(TG_RANDOM), AN(ANIM_SNIFF, 2.5f), GO(TG_RANDOM), AN(ANIM_SNIFF, 2.0f) } },
    [ACT_GO_DOOR]        = { 0,                     { GO(TG_DOOR), ON(ANIM_LOOK_AROUND) } },
    [ACT_GO_NOOK]        = { 0     ,                { GO(SPOT_NOOK), DR(DIR_W), AN(ANIM_IDLE_SIT_BLINK, 2.0f) } },
    [ACT_GO_BOWLS]       = { 0     ,                { GO(TG_BOWL_FOOD), DR(DIR_W), AN(ANIM_IDLE_STAND, 1.5f) } },
    [ACT_GO_PAD]         = { 0     ,                { GO(SPOT_PAD), AN(ANIM_SNIFF, 1.5f) } },
    [ACT_GO_RUG]         = { 0     ,                { GO(SPOT_RUG), AN(ANIM_IDLE_SIT_BLINK, 2.0f) } },
    [ACT_GO_SUNBEAM]     = { N_SUN         ,        { GO(TG_SUNBEAM), DR(DIR_S), AN(ANIM_SUN_SIT, 3.0f) } },
    [ACT_GO_BUSH]        = { 0     ,                { GO(TG_BUSH_EDGE), AN(ANIM_SNIFF, 2.0f) } },
    [ACT_GO_TREE]        = { 0     ,                { GO(SPOT_TREE), AN(ANIM_SNIFF, 2.0f) } },
    [ACT_GO_TOY]         = { N_TOY_VIEW,            { GO(TG_TOY), AN(ANIM_SNIFF, 1.0f) } },
    /* through the door (spec 8.4: she goes out, the camera stays) */
    [ACT_ENTER_YARD]     = { N_ROOM,                { GO(TG_DOOR), VIEW, GO(TG_INSIDE), ON(ANIM_LOOK_AROUND) } },
    [ACT_ENTER_ROOM]     = { N_YARD,                { GO(TG_DOOR), VIEW, GO(TG_INSIDE), AN(ANIM_IDLE_STAND, 1.0f) } },
    /* play */
    [ACT_PICKUP_ROPE]    = { N_ROPE | N_FREE_MOUTH, { GO(TG_ROPE), AN(ANIM_SNIFF, 0.6f), PICK(TOY_ROPE) } },
    [ACT_PICKUP_PLUSH]   = { N_PLUSH | N_FREE_MOUTH,{ GO(TG_PLUSH), AN(ANIM_SNIFF, 0.6f), PICK(TOY_PLUSH) } },
    [ACT_BRING_TOY]      = { N_MOUTH,               { ST(STYLE_TROT), GO(TG_FRONT), DROP, DR(DIR_S), AN(ANIM_PLAY_BOW, 2.0f) } },
    [ACT_DROP_TOY_FRONT] = { N_MOUTH,               { GO(TG_FRONT), DROP, DR(DIR_S), AN(ANIM_IDLE_SIT_BLINK, 1.5f) } },
    [ACT_CHEW_TOY]       = { N_TOY_ANY,             { GO(TG_TOY), PICK(ANYTOY), ON(ANIM_LIE_DOWN), AN(ANIM_CHEW_TOY, 5.0f) } },
    [ACT_PLAY_BOW]       = { 0,                     { AN(ANIM_PLAY_BOW, 3.0f) } },
    [ACT_ZOOMIES]        = { 0,                     { ST(STYLE_SPRINT), GO(TG_RANDOM_FAR), GO(TG_RANDOM_FAR), GO(TG_RANDOM_FAR), AN(ANIM_PANT_SMILE, 2.0f) } },
    [ACT_RUN_WITH_TOY]   = { N_MOUTH,               { ST(STYLE_TROT), GO(TG_RANDOM_FAR), GO(TG_RANDOM_FAR), AN(ANIM_PLAY_BOW, 1.5f) } },
    [ACT_TOSS_TOY]       = { N_MOUTH,               { TOSS, ON(ANIM_POUNCE), GO(TG_TOY) } },
    /* the body and its talk */
    [ACT_SIT]            = { 0,                     { ON(ANIM_SIT_DOWN), AN(ANIM_IDLE_SIT_BLINK, 3.0f) } },
    [ACT_LIE_DOWN]       = { 0,                     { ON(ANIM_LIE_DOWN), WT(3.0f) } },
    [ACT_BARK]           = { 0,                     { FX(FX_BARK), ON(ANIM_BARK_HEAD_FLIP) } },
    [ACT_HUFF]           = { 0,                     { FX(FX_HUFF), ON(ANIM_HEAD_TILT) } },                 /* A13 not made yet */
    [ACT_HEAD_TILT]      = { 0,                     { ON(ANIM_HEAD_TILT) } },
    [ACT_WAG]            = { 0,                     { AN(ANIM_IDLE_STAND, 2.0f) } },                       /* her idle wags */
    [ACT_BEG]            = { 0,                     { DR(DIR_S), AN(ANIM_ASK_PET, 3.0f) } },
    [ACT_ASK_PET]        = { 0,                     { GO(TG_FRONT), DR(DIR_S), AN(ANIM_ASK_PET, 4.0f) } },
    [ACT_PANT_IDLE]      = { 0,                     { AN(ANIM_PANT_SMILE, 3.0f) } },
    [ACT_STRETCH]        = { 0,                     { ON(ANIM_PLAY_BOW), AN(ANIM_IDLE_STAND, 1.0f) } },
    /* needs */
    [ACT_EAT]            = { N_FOOD         ,       { GO(TG_BOWL_FOOD), DR(DIR_W), AN(ANIM_EAT_DRINK, 4.0f), FX(FX_EAT) } },
    [ACT_DRINK]          = { N_WATER         ,      { GO(TG_BOWL_WATER), DR(DIR_W), AN(ANIM_DRINK, 3.0f), FX(FX_DRINK) } },
    [ACT_USE_PAD]        = { 0     ,                { GO(SPOT_PAD), ON(ANIM_SQUAT), AN(ANIM_PEE, 3.0f), FX(FX_PAD) } },
    [ACT_MISCHIEF_FLOOR] = { N_NO_MEMORIAL         ,{ GO(TG_FLOOR_AWAY), ON(ANIM_SQUAT), AN(ANIM_PEE, 2.0f), FX(FX_MESS), ON(ANIM_LOOK_AROUND) } },
    [ACT_SLEEP_NOOK]     = { 0     ,                { GO(SPOT_NOOK), ON(ANIM_LIE_DOWN), AN(ANIM_SLEEP_CURL, 10.0f) } },
    [ACT_SLEEP_RUG]      = { 0     ,                { GO(SPOT_RUG), ON(ANIM_LIE_DOWN), AN(ANIM_SLEEP_CURL, 10.0f) } },
    [ACT_NAP_SUNBEAM]    = { N_SUN         ,        { GO(TG_SUNBEAM), DR(DIR_S), AN(ANIM_SUN_SIT, 3.0f), AN(ANIM_SLEEP_CURL, 6.0f) } },
    /* sneaking and hiding */
    [ACT_LOOK_AROUND]    = { 0,                     { ON(ANIM_LOOK_AROUND) } },
    [ACT_SNEAK_RUG]      = { 0     ,                { ON(ANIM_LOOK_AROUND), ST(STYLE_TIPTOE), GO(SPOT_RUG), ON(ANIM_LIE_DOWN), AN(ANIM_SLEEP_CURL, 6.0f) } },   /* H7 on the rug */
    [ACT_LEAVE_RUG]      = { N_ROOM | N_ON_RUG,     { ON(ANIM_STARTLE), ST(STYLE_SCURRY), GO(SPOT_CENTER), AN(ANIM_IDLE_STAND, 1.0f) } },
    [ACT_HIDE_BENCH]     = { 0     ,                { ST(STYLE_SCURRY), GO(SPOT_NOOK), DR(DIR_W), HIDE(1), WT(3.0f) } },
    [ACT_PEEK_BENCH]     = { N_ROOM | N_HIDDEN,     { PEEK(3.0f), ON(ANIM_LOOK_AROUND) } },                /* A21 not made yet */
    [ACT_HIDE_BUSH]      = { N_YARD,                { GO(TG_BUSH_HIDE), HIDE(2), AN(ANIM_IDLE_SIT_BLINK, 3.0f) } },
    [ACT_PEEK_BUSH]      = { N_YARD,                { GO(TG_BUSH_EDGE), DR(DIR_S), ON(ANIM_LOOK_AROUND) } },
    /* the seasons outdoors */
    [ACT_CHASE_LEAF]     = { N_YARD | N_AUTUMN,     { ST(STYLE_TROT), GO(TG_RANDOM), ON(ANIM_POUNCE), GO(TG_RANDOM), ON(ANIM_POUNCE) } },
    [ACT_CHASE_BUTTERFLY]= { N_YARD | N_WARM,       { ST(STYLE_TROT), GO(TG_RANDOM), ON(ANIM_POUNCE), GO(TG_RANDOM_FAR), ON(ANIM_POUNCE) } },
    [ACT_PLAY_SNOW]      = { N_YARD | N_WINTER,     { ON(ANIM_PLAY_BOW), ST(STYLE_SPRINT), GO(TG_RANDOM_FAR), AN(ANIM_SNIFF, 2.0f) } },
    [ACT_SNIFF_FLOWERS]  = { N_WARM         ,       { GO(SPOT_FLOWERS), AN(ANIM_SNIFF, 3.0f) } },
    [ACT_NONE]           = { 0,                     { { P_END, 0, 0, 0 } } },
    /* the reflexes (spec 8.8): R1 forgiven, R2 called, R3 petted, R4 a toy thrown, R5 chased, R6 greeting, R7 shaken, R8 a voice, R9 a tap */
    [RX_FORGIVEN]        = { 0, { UNHIDE, FX(FX_FORGIVE), ST(STYLE_TROT), GO(TG_FRONT), DR(DIR_S), AN(ANIM_PANT_SMILE, 2.5f) } },
    [RX_CALL]            = { 0, { UNHIDE, ON(ANIM_HEAD_TILT), ST(STYLE_TROT), GO(TG_FRONT), DR(DIR_S), AN(ANIM_IDLE_SIT_BLINK, 1.5f) } },
    [RX_CALL_OTHER]      = { 0, { UNHIDE, ST(STYLE_TROT), GO(TG_DOOR), VIEW, GO(TG_FRONT), DR(DIR_S), AN(ANIM_IDLE_SIT_BLINK, 1.5f) } },
    [RX_PET]             = { 0, { AN(ANIM_PET_ENJOY, 60.0f) } },
    [RX_PET_END]         = { 0, { AN(ANIM_PET_ENJOY, 2.0f) } },
    [RX_FETCH]           = { 0, { LOOK(TG_TOY), AN(ANIM_STARTLE, 0.08f), ST(STYLE_SPRINT), GO(TG_TOY), ON(ANIM_POUNCE), PICK(ANYTOY),
                                  ST(STYLE_TROT), GO(TG_FRONT), DROP, DR(DIR_S), AN(ANIM_PLAY_BOW, 1.5f) } },
    [RX_TEASE]           = { 0, { PICK(ANYTOY), ST(STYLE_SPRINT), GO(TG_RANDOM_FAR), LOOK(TG_FRONT), AN(ANIM_PLAY_BOW, 2.5f) } },
    [RX_TIRED]           = { 0, { DROP, ON(ANIM_SIT_DOWN), AN(ANIM_PANT_SMILE, 3.0f) } },
    [RX_GREET]           = { 0, { UNHIDE, ST(STYLE_TROT), GO(TG_TOY), PICK(ANYTOY), GO(TG_FRONT), DR(DIR_S), ON(ANIM_LIE_DOWN), AN(ANIM_CHEW_TOY, 6.0f) } },
    [RX_GREET_GUILTY]    = { 0, { GO(SPOT_NOOK), DR(DIR_W), HIDE(1), PEEK(4.0f), ON(ANIM_LOOK_AROUND) } },
    [RX_SHAKE]           = { 0, { ON(ANIM_STARTLE), FX(FX_BARK), ON(ANIM_BARK_HEAD_FLIP), WT(1.0f) } },
    [RX_VOICE]           = { 0, { ON(ANIM_HEAD_TILT) } },
    [RX_TAP_LOOK]        = { 0, { LOOK(TG_TAP), WT(0.8f) } },
    [RX_TAP_GO]          = { 0, { LOOK(TG_TAP), WT(0.4f), GO(TG_TAP), ON(ANIM_SNIFF) } },
    [RX_JOY]             = { 0, { DR(DIR_S), AN(ANIM_IDLE_STAND, 1.2f) } },                  /* a tap on her: a wag (spec 8.10) */
};

/* the schema's styles (st_walk st_trot st_sprint st_tiptoe st_scurry st_sleepy st_curious) -> the gaits; a high
   urgency hurries a walk into a trot, a low one slows it to a dawdle */
nav_style_t plan_style(int st, int urg) {
    static const uint8_t G[ST_COUNT] = { STYLE_WALK, STYLE_TROT, STYLE_SPRINT, STYLE_TIPTOE, STYLE_SCURRY, STYLE_SLEEPY, STYLE_CURIOUS };
    nav_style_t g = (nav_style_t)(st >= 0 && st < ST_COUNT ? G[st] : STYLE_WALK);
    if (urg >= 7 && (g == STYLE_WALK || g == STYLE_CURIOUS)) g = STYLE_TROT;
    if (urg && urg <= 2 && g == STYLE_WALK) g = STYLE_CURIOUS;
    return g;
}

bool world_sun_on(const world_t *w) { const int d = world_daypart(w); return d == 0 || d == 1; }

static bool on_rug(const actor_t *a) {
    const map_t *m = &MAPS[VIEW_ROOM];
    for (int i = 0; i < m->n_zones; i++) {
        const map_zone_t *z = &m->zones[i];
        if (z->kind == 0 && !strcmp(z->name, "rug")) return a->view == VIEW_ROOM && a->x >= z->a && a->x < z->a + z->c && a->y >= z->b && a->y < z->b + z->d;
    }
    return false;
}
static bool toy_loose_in_view(const world_t *w, int toy, view_t v) { const toy_t *t = &w->things.toys[toy]; return t->held_by < 0 && t->view == v; }

bool plan_possible(const world_t *w, int actor, int act) {
    if (act >= ACT_COUNT && act < RX_END) return true;              /* a reflex: its trigger was its condition */
    if (act < 0 || act >= ACT_COUNT || act == ACT_NONE) return false;
    const actor_t *a = &w->actors[actor];
    const uint32_t n = ACTS[act].need; const int season = world_season(w);
    if ((n & N_ROOM) && a->view != VIEW_ROOM) return false;
    if ((n & N_YARD) && a->view != VIEW_YARD) return false;
    if ((n & N_TOY_VIEW) && !toy_loose_in_view(w, TOY_ROPE, a->view) && !toy_loose_in_view(w, TOY_PLUSH, a->view)) return false;
    if ((n & N_MOUTH) && toy_held(w, actor) < 0) return false;
    if ((n & N_FREE_MOUTH) && toy_held(w, actor) >= 0) return false;
    if ((n & N_TOY_ANY) && toy_held(w, actor) < 0 && !toy_loose_in_view(w, TOY_ROPE, a->view) && !toy_loose_in_view(w, TOY_PLUSH, a->view)) return false;
    if ((n & N_ROPE) && !toy_loose_in_view(w, TOY_ROPE, a->view)) return false;
    if ((n & N_PLUSH) && !toy_loose_in_view(w, TOY_PLUSH, a->view)) return false;
    if ((n & N_FOOD) && !w->things.food) return false;
    if ((n & N_WATER) && !w->things.water) return false;
    if ((n & N_SUN) && !world_sun_on(w)) return false;
    if ((n & N_HIDDEN) && w->plans[actor].hide != 1) return false;
    if ((n & N_AUTUMN) && season != 2) return false;
    if ((n & N_WARM) && season > 1) return false;
    if ((n & N_WINTER) && season != 3) return false;
    if ((n & N_ON_RUG) && !on_rug(a)) return false;
    if ((n & N_NO_MEMORIAL) && world_day(w) == DAY_MEMORIAL) return false;   /* H15: never on September 12th (spec 10.5) */
    return true;
}

/* ---- the walk targets ---- */
static bool random_floor(world_t *w, view_t v, float fx, float fy, float min_d, int16_t *x, int16_t *y) {
    for (int k = 0; k < 60; k++) {
        int px = 24 + (int)(world_rand(w) % (LILI_LOG_W - 48)), py = 24 + (int)(world_rand(w) % (LILI_LOG_H - 48));
        if (!nav_clear(v, px, py, nav_margin(v))) continue;
        if ((px - fx) * (px - fx) + (py - fy) * (py - fy) < min_d * min_d) continue;
        *x = (int16_t)px; *y = (int16_t)py; return true;
    }
    return false;
}
static bool zone_center(view_t v, const char *prefix, int pick, int16_t *x, int16_t *y) {
    const map_t *m = &MAPS[v]; int n = 0;
    for (int i = 0; i < m->n_zones; i++) if (!strncmp(m->zones[i].name, prefix, strlen(prefix))) n++;
    if (!n) return false;
    pick %= n;
    for (int i = 0; i < m->n_zones; i++) {
        const map_zone_t *z = &m->zones[i];
        if (strncmp(z->name, prefix, strlen(prefix)) || pick--) continue;
        if (z->kind == 0) { *x = (int16_t)(z->a + z->c / 2); *y = (int16_t)(z->b + z->d / 2); return true; }
        long sx = 0, sy = 0; for (int k = 0; k < z->n; k++) { sx += z->poly[2 * k]; sy += z->poly[2 * k + 1]; }
        *x = (int16_t)(sx / z->n); *y = (int16_t)(sy / z->n); return true;
    }
    return false;
}
static bool target(world_t *w, int actor, int tg, int16_t *x, int16_t *y) {
    actor_t *a = &w->actors[actor]; const map_t *m = &MAPS[a->view];
    if (tg < SPOT_COUNT) { const map_point_t *p = &m->points[tg]; if (p->z < 0) return false; *x = p->x; *y = p->y; return true; }
    const map_point_t *door = &m->points[a->view == VIEW_ROOM ? SPOT_DOOR : SPOT_DOOR_YARD];
    switch (tg) {
    case TG_FRONT:      if (a->view == VIEW_ROOM) return target(w, actor, SPOT_FRONT, x, y);
                        *x = LILI_LOG_W / 2; *y = LILI_LOG_H * 4 / 5; return true;          /* the yard: the grass above the house door */
    case TG_RANDOM:     return random_floor(w, a->view, a->x, a->y, 40, x, y);
    case TG_RANDOM_FAR: return random_floor(w, a->view, a->x, a->y, 150, x, y) || random_floor(w, a->view, a->x, a->y, 60, x, y);
    case TG_TOY: case TG_ROPE: case TG_PLUSH: {
        int t = tg == TG_ROPE ? TOY_ROPE : tg == TG_PLUSH ? TOY_PLUSH : toy_held(w, actor) >= 0 ? -1 : w->plans[actor].toy >= 0 ? w->plans[actor].toy : toy_near(w, actor, 1e6f);
        if (tg == TG_TOY && toy_held(w, actor) >= 0) { *x = (int16_t)a->x; *y = (int16_t)a->y; return true; }   /* already in her mouth */
        if (t < 0 || w->things.toys[t].view != a->view) return false;
        *x = (int16_t)w->things.toys[t].x; *y = (int16_t)(w->things.toys[t].y + 2); return true;
    }
    case TG_DOOR:       *x = door->x; *y = door->y; return door->z >= 0;
    case TG_INSIDE:     *x = door->x; *y = (int16_t)(door->y + (a->view == VIEW_ROOM ? 60 : -70)); return true;
    case TG_SUNBEAM:    return zone_center(VIEW_ROOM, "sunbeam", 0, x, y);
    case TG_BUSH_HIDE:  return zone_center(VIEW_YARD, "hide", (int)(world_rand(w) % 4), x, y);
    case TG_BUSH_EDGE:  return target(w, actor, world_rand(w) & 1 ? SPOT_BUSH_2 : SPOT_BUSH_3, x, y);
    case TG_BOWL_FOOD:  *x = 273; *y = 450; return true;      /* east of the food bowl: her frames carry their own bowl (lili_demo.c) */
    case TG_BOWL_WATER: *x = 351; *y = 451; return true;
    case TG_TAP:        *x = w->finger_x; *y = w->finger_y; return true;   /* off the floor: the walk ends at the nearest floor */
    case TG_FLOOR_AWAY: {                                     /* H9: next to the pad, on the bare floor, not on it */
        const map_point_t *pad = &MAPS[VIEW_ROOM].points[SPOT_PAD];
        for (int k = 0; k < 40; k++) {
            int16_t px, py;
            if (!random_floor(w, VIEW_ROOM, pad->x, pad->y, 70, &px, &py)) break;
            if ((px - pad->x) * (px - pad->x) + (py - pad->y) * (py - pad->y) < 170 * 170) { *x = px; *y = py; return true; }
        }
        return random_floor(w, VIEW_ROOM, pad->x, pad->y, 70, x, y);
    }
    }
    return false;
}

/* which view a walk target is in: the spots of one map only, the room's bowls / sunbeam / floor, the yard's bushes */
static view_t target_view(const world_t *w, int actor, int tg) {
    const view_t here = w->actors[actor].view, there = (view_t)(1 - here);
    if (tg < SPOT_COUNT) return MAPS[here].points[tg].z < 0 && MAPS[there].points[tg].z >= 0 ? there : here;
    if (tg == TG_BOWL_FOOD || tg == TG_BOWL_WATER || tg == TG_SUNBEAM || tg == TG_FLOOR_AWAY) return VIEW_ROOM;
    if (tg == TG_BUSH_HIDE || tg == TG_BUSH_EDGE) return VIEW_YARD;
    return here;
}
/* out of one door, in at the other (spec 8.4: the camera stays where it is) */
static void through_door(world_t *w, int actor) {
    actor_t *a = &w->actors[actor]; plan_t *p = &w->plans[actor];
    const view_t to = (view_t)(1 - a->view);
    const map_point_t *d = &MAPS[to].points[to == VIEW_ROOM ? SPOT_DOOR : SPOT_DOOR_YARD];
    a->view = to; a->x = d->x; a->y = d->y; a->vis = 0; p->hide = 0;
    int t = toy_held(w, actor); if (t >= 0) w->things.toys[t].view = to;
}

static float once_len(uint16_t anim) {                        /* a one-shot animation's length in seconds */
    sprite_set_t s; const pak_t *p = world_pak();
    const uint16_t src = ANIM_REVERSE_OF(anim) ? ANIM_REVERSE_OF(anim) : anim;
    if (p && pak_sprite_set(p, src, &s) && s.fps) return (float)s.n_frames / s.fps;
    return 1.0f;
}
static void set_anim(actor_t *a, uint16_t anim) { if (a->anim != anim) { a->anim = anim; a->anim_t = 0; } }

static void effect(world_t *w, int actor, int fx) {
    things_t *t = &w->things; const actor_t *a = &w->actors[actor];
    switch (fx) {
    case FX_EAT:   if (t->food) t->food--; break;
    case FX_DRINK: if (t->water) t->water--; break;
    case FX_PAD:   if (t->pad < 2) t->pad++; break;
    case FX_MESS:  if (t->n_mess < MESS_MAX) { t->mess[t->n_mess][0] = (int16_t)a->x; t->mess[t->n_mess][1] = (int16_t)a->y; t->n_mess++; }
                   t->guilty = true; break;
    case FX_FORGIVE: t->guilty = false; break;
    default: break;                                           /* FX_BARK, FX_HUFF: the sound bank (M9) */
    }
}

void plan_start(world_t *w, int actor, const decision_t *d) {
    plan_t *p = &w->plans[actor];
    const uint8_t hide = p->hide, rounds = p->rounds;
    memset(p, 0, sizeof *p);
    p->d = *d; p->status = PLAN_RUNNING; p->hide = hide; p->prio = 255; p->rounds = rounds; p->toy = -1;
    nav_stop(&w->bodies[actor]);
    w->bodies[actor].style = (uint8_t)plan_style(d->style, d->urg);
    if (d->bubble != BB_NONE && d->bubble < BB_COUNT) { w->bubble = d->bubble; w->bubble_t = 2.5f; }   /* what she thinks, for a moment */
}
void plan_one(world_t *w, int actor, int act, int style) {
    decision_t d = { { (uint8_t)act, ACT_NONE, ACT_NONE }, (uint8_t)style, FC_NEUTRAL, BB_NONE, 5, 1.0f };
    plan_start(w, actor, &d);
}

/* the next action that can run now; false = none left */
static bool next_action(world_t *w, int actor, plan_t *p, int from) {
    for (int i = from; i < 3; i++) {
        if (p->d.a[i] == ACT_NONE) continue;
        if (plan_possible(w, actor, p->d.a[i])) { p->ai = (uint8_t)i; p->pi = 0; p->t = p->step_t = 0; p->rep = 0;
                                                  w->bodies[actor].style = (uint8_t)plan_style(p->d.style, p->d.urg); return true; }
        p->skipped |= (uint8_t)(1 << i);
    }
    return false;
}

static int life_of(int a) {                                   /* a reflex's effect on the needs, else the action itself */
    switch (a) {
    case RX_PET_END: return LIFE_PET;
    case RX_GREET: return LIFE_GREET;
    case RX_FORGIVEN: return LIFE_FORGIVEN;
    case RX_CALL: case RX_CALL_OTHER: return LIFE_CALL;
    case RX_FETCH: case RX_TEASE: return LIFE_FETCH;
    default: return a < ACT_COUNT ? a : -1;
    }
}
static int finish(world_t *w, int actor, plan_t *p) {
    actor_t *a = &w->actors[actor];
    nav_stop(&w->bodies[actor]);
    const int ran = 3 - __builtin_popcount(p->skipped) - (p->d.a[0] == ACT_NONE) - (p->d.a[1] == ACT_NONE) - (p->d.a[2] == ACT_NONE);
    p->status = ran > 0 ? PLAN_DONE : PLAN_FAILED;
    /* idle until the next decision (spec 8.7: never frozen): resting poses hold, the rest stands and wags */
    if (a->anim != ANIM_SLEEP_CURL && a->anim != ANIM_IDLE_SIT_BLINK && a->anim != ANIM_CHEW_TOY && a->anim != ANIM_SUN_SIT && a->anim != ANIM_LIE_DOWN)
        set_anim(a, ANIM_IDLE_STAND);
    return p->status;
}

int plan_tick(world_t *w, int actor, float dt) {
    plan_t *p = &w->plans[actor];
    if (p->peek_t > 0) { p->peek_t -= dt; if (p->peek_t < 0) p->peek_t = 0; }
    if (p->status != PLAN_RUNNING) return p->status;
    actor_t *a = &w->actors[actor]; body_t *b = &w->bodies[actor];
    if (p->step_t == 0 && p->pi == 0 && p->t == 0 && !next_action(w, actor, p, p->ai)) return finish(w, actor, p);
    p->t += dt; p->step_t += dt;
    if (p->step_t > PLAN_STEP_LIMIT_S) goto next_act;              /* the action's time limit */
    for (;;) {
        const prim_t *q = &ACTS[p->d.a[p->ai]].p[p->pi];
        bool done = false;
        switch (q->op) {
        case P_END: goto next_act;
        case P_GOTO:
            if (p->rep >= 10 || target_view(w, actor, q->arg) != a->view) {   /* the place is in the other view: the door first */
                if (p->rep < 10) {
                    const map_point_t *d = &MAPS[a->view].points[a->view == VIEW_ROOM ? SPOT_DOOR : SPOT_DOOR_YARD];
                    if (p->hide == 1) p->hide = 0;
                    nav_plan(a->view, a->x, a->y, d->x, d->y, b); p->rep = 10;
                }
                if (p->rep == 10) { if (nav_follow(a, b, dt)) { p->rep = 11; p->hide = 3; p->t = 0; } break; }
                if (p->t >= 0.5f) { through_door(w, actor); p->rep = 0; }
                break;
            }
            if (p->rep == 0) {                                     /* start the walk */
                int16_t gx, gy;
                if (!target(w, actor, q->arg, &gx, &gy)) { done = true; break; }
                if (p->hide == 1) p->hide = 0;                     /* out from under the bench */
                p->gx = gx; p->gy = gy; nav_plan(a->view, a->x, a->y, gx, gy, b); p->rep = 1;
            }
            if ((q->arg == TG_TOY || q->arg == TG_ROPE || q->arg == TG_PLUSH) && toy_held(w, actor) < 0) {   /* a toy still rolling: after it */
                int16_t gx, gy;
                if (target(w, actor, q->arg, &gx, &gy) && abs(gx - p->gx) + abs(gy - p->gy) > 20) { p->gx = gx; p->gy = gy; nav_plan(a->view, a->x, a->y, gx, gy, b); }
            }
            done = nav_follow(a, b, dt);
            if (toy_held(w, actor) == TOY_ROPE && (a->anim == ANIM_TROT || a->anim == ANIM_WALK)) set_anim(a, ANIM_CARRY_TROT);   /* the rope is in those frames */
            break;
        case P_ANIM: set_anim(a, q->anim); done = p->t >= q->t; break;
        case P_ONCE:                                               /* already in that pose: no getting up to lie down again */
            if ((q->anim == ANIM_LIE_DOWN && (a->anim == ANIM_SLEEP_CURL || a->anim == ANIM_CHEW_TOY)) || (q->anim == ANIM_SIT_DOWN && a->anim == ANIM_IDLE_SIT_BLINK)) { done = true; break; }
            set_anim(a, q->anim); done = p->t >= once_len(q->anim); break;
        case P_WAIT: done = p->t >= q->t; break;
        case P_DIR:  a->dir = (dir8_t)q->arg; done = true; break;
        case P_LOOK: { int16_t lx, ly; if (target(w, actor, q->arg, &lx, &ly) && (lx != (int)a->x || ly != (int)a->y)) a->dir = nav_dir(lx - a->x, ly - a->y); done = true; break; }
        case P_PICK: {
            int t = q->arg == ANYTOY ? (toy_held(w, actor) >= 0 ? -1 : p->toy >= 0 ? p->toy : toy_near(w, actor, 48)) : q->arg;
            if (t >= 0 && toy_held(w, actor) < 0) {
                const toy_t *ty = &w->things.toys[t];
                if (ty->held_by < 0 && ty->view == a->view && fabsf(ty->x - a->x) < 48 && fabsf(ty->y - a->y) < 48) toy_pick(w, t, actor);
            }
            done = true; break;
        }
        case P_DROP: { int t = toy_held(w, actor); if (t >= 0) toy_drop(w, t); done = true; break; }
        case P_TOSS: {
            int t = toy_held(w, actor);
            if (t >= 0) {
                static const int8_t VX[DIR_COUNT] = { 0, -7, -10, -7, 0, 7, 10, 7 }, VY[DIR_COUNT] = { 10, 7, 0, -7, -10, -7, 0, 7 };
                toy_drop(w, t); toy_throw(w, t, VX[a->dir] * 90.0f, VY[a->dir] * 90.0f);
            }
            done = true; break;
        }
        case P_VIEW:                                               /* out of the door; 0.5 s later at the other one */
            if (p->rep == 0) { p->hide = 3; p->rep = 1; }
            if (p->t >= 0.5f) { through_door(w, actor); done = true; }
            break;
        case P_HIDE:   p->hide = q->arg; done = true; break;
        case P_UNHIDE: p->hide = 0; done = true; break;
        case P_PEEK:   p->peek_t = q->t; done = true; break;
        case P_FOLLOW:                                             /* the finger, while it is down; then she waits for it */
            if (a->view != w->view) { done = true; break; }        /* she cannot see the glass from the other place */
            if (w->finger_down && (p->rep == 0 || abs(w->finger_x - p->gx) + abs(w->finger_y - p->gy) > 24 || b->n == 0)) {
                p->gx = w->finger_x; p->gy = w->finger_y; p->rep = 1; nav_plan(a->view, a->x, a->y, p->gx, p->gy, b);
            }
            nav_follow(a, b, dt);
            if (!b->n && a->anim != ANIM_IDLE_SIT_BLINK && !w->finger_down) set_anim(a, ANIM_IDLE_STAND);
            done = p->t >= q->t;
            break;
        case P_STYLE: b->style = q->arg; done = true; break;
        case P_FX:    effect(w, actor, q->arg); done = true; break;
        }
        if (!done) return PLAN_RUNNING;
        p->pi++; p->t = 0; p->rep = 0;
        if (p->pi >= PRIMS) goto next_act;
        if (ACTS[p->d.a[p->ai]].p[p->pi].op == P_END) goto next_act;
        if (q->op == P_GOTO || q->op == P_ANIM || q->op == P_ONCE || q->op == P_WAIT || q->op == P_FOLLOW || q->op == P_VIEW) return PLAN_RUNNING;   /* one timed primitive a tick */
    }
next_act:
    nav_stop(b);
    life_after_action(w, actor, life_of(p->d.a[p->ai]));          /* what it satisfied (spec 9.1) */
    if (!next_action(w, actor, p, p->ai + 1)) return finish(w, actor, p);
    return PLAN_RUNNING;
}

int plan_describe(const world_t *w, int actor, char *buf, int n) {
    const plan_t *p = &w->plans[actor];
    static const char *const S[] = { "nincs", "fut", "kesz", "elbukott" };
    if (p->status == PLAN_IDLE) return snprintf(buf, (size_t)n, "terv: nincs");
    static const char *const RX[RX_END - ACT_COUNT] = { "R1 forgiven", "R2 call", "R2 call (door)", "R3 pet", "R3 pet end", "R4 fetch",
        "R5 tease", "R5 tired", "R6 greet", "R6 greet (guilty)", "R7 shake", "R8 voice", "R9 tap", "R9 tap (go)", "tap on her" };
    const int a = p->d.a[p->ai];
    return snprintf(buf, (size_t)n, "terv: %s  %d/3 %s  lepes %d  %.0f s", S[p->status], p->ai + 1, a < ACT_COUNT ? ACTION_NAME(a) : RX[a - ACT_COUNT], p->pi, p->step_t);
}

/* ---- the reflexes (spec 8.8) ---- */
bool plan_asleep(const world_t *w, int actor) { return w->actors[actor].anim == ANIM_SLEEP_CURL; }
static bool deep_asleep(const world_t *w, int actor) { return plan_asleep(w, actor) && world_daypart(w) == 3 && w->actors[actor].anim_t > 60.0f; }

/* start a reflex's sequence if its priority (1 = R1, the highest .. 9) beats a running reflex's */
static bool fire(world_t *w, int actor, int rx, int prio, int bubble) {
    plan_t *p = &w->plans[actor];
    if (p->status == PLAN_RUNNING && p->prio < prio) return false;
    decision_t d = { { (uint8_t)rx, ACT_NONE, ACT_NONE }, ST_TROT, FC_NEUTRAL, (uint8_t)bubble, 7, 1.0f };   /* plan_start shows the bubble */
    if (rx != RX_TEASE && rx != RX_TIRED) p->rounds = 0;
    plan_start(w, actor, &d);
    p->prio = (uint8_t)prio;
    return true;
}
static bool on_her(const world_t *w, int x, int y, int pad) {
    gesture_ctx_t c; world_gesture_ctx(w, &c);
    return c.lili.x1 > c.lili.x0 && x >= c.lili.x0 - pad && x < c.lili.x1 + pad && y >= c.lili.y0 - pad && y < c.lili.y1 + pad;
}

bool reflex_forgiven(world_t *w) {                             /* R1: the last spot cleaned while she is guilty */
    if (!w->things.guilty) return false;
    return fire(w, 0, RX_FORGIVEN, 1, BB_HEART);
}
bool reflex_greet(world_t *w) {                                /* R6: the owner is back (a wake after > 15 min) */
    return fire(w, 0, w->things.guilty ? RX_GREET_GUILTY : RX_GREET, 6, w->things.guilty ? BB_SWEAT : BB_HEART);
}

bool reflex_event(world_t *w, const ev_t *e) {
    const int me = 0; actor_t *a = &w->actors[me]; plan_t *p = &w->plans[me];
    const bool asleep = plan_asleep(w, me), here = a->view == w->view, guilty = w->things.guilty;
    switch (e->type) {
    case EV_WHISTLE: case EV_CALL_NAME:                        /* R2: she comes to the owner, from anywhere */
        if (deep_asleep(w, me) || guilty) return false;
        return fire(w, me, here ? RX_CALL : RX_CALL_OTHER, 2, BB_EXCLAIM);
    case EV_PET_START: case EV_PET_TICK:                       /* R3: she stops and enjoys it */
        if (!here || p->hide || a->vis < 0.5f) return false;
        if (p->status == PLAN_RUNNING && p->prio == 3 && p->d.a[0] == RX_PET) { p->t = 0; return true; }   /* still being petted */
        return fire(w, me, RX_PET, 3, BB_HEART);
    case EV_PET_END:
        if (p->status == PLAN_RUNNING && p->prio == 3) return fire(w, me, RX_PET_END, 3, BB_HEART);
        return false;
    case EV_FLICK:                                             /* R4: off after the toy, and back with it */
        if (asleep || !here) return false;
        if (!fire(w, me, RX_FETCH, 4, BB_EXCLAIM)) return false;
        p->toy = (int8_t)(e->c >= 0 && e->c < TOY_COUNT ? e->c : -1);
        return true;
    case EV_APPROACH:                                          /* R5: she bows with a toy by her: grab it and dash off */
        if (asleep || !here || e->a >= 70 * LILI_K || a->anim != ANIM_PLAY_BOW) return false;
        if (toy_held(w, me) < 0 && toy_near(w, me, 40) < 0) return false;
        if (p->rounds >= 4) return fire(w, me, RX_TIRED, 5, BB_SWEAT);
        p->rounds++;
        { const uint8_t r = p->rounds; bool ok = fire(w, me, RX_TEASE, 5, BB_NOTE); p->rounds = r; return ok; }
    case EV_SHAKE:                                             /* R7: startled, one bark */
        return fire(w, me, RX_SHAKE, 7, BB_EXCLAIM);
    case EV_VOICE_ACTIVITY:                                    /* R8: the head up and tilted */
        if (asleep) return false;
        return fire(w, me, RX_VOICE, 8, BB_QUESTION);
    case EV_TAP:                                               /* R9: a tap on the free floor: a look, half the time a walk there */
        if (here && !asleep && on_her(w, e->a, e->b, 8)) return fire(w, me, RX_JOY, 9, BB_HEART);
        if (asleep || guilty || !here) return false;
        w->finger_x = e->a; w->finger_y = e->b;
        return fire(w, me, (world_rand(w) & 1) ? RX_TAP_GO : RX_TAP_LOOK, 9, BB_NONE);
    default: return false;
    }
}
