/* pose.c - see pose.h */
#include "pose.h"
#include "lili_world.h"
#include "asset_ids.h"

enum { PO_ANY = 0, PO_STAND, PO_SIT, PO_LIE, PO_CURL, PO_EAT, PO_DRINK, PO_SQUAT, PO_POOP, PO_TREAT };   /* curled up asleep is a pose of its own (2026-10-28, curl_up) */

/* the moves between poses: the animation, the pose it starts in and the one it ends in, and the same move backwards.
   2026-10-29 (the owner approved docs/img/pilot_2026-10-28/wave4_new.mp4): the head into the bowls, the squats for the
   pee and the poop, the bend to the treat and lying down with it are poses of their own with their own ways in and
   (but for the treat, which is eaten) out, so the plan asks for the eating, the pee or the chewing and gets the move first */
static const struct { uint16_t anim, back; uint8_t from, to; } MOVES[] = {
    { ANIM_SIT_DOWN, ANIM_STAND_UP, PO_STAND, PO_SIT },          { ANIM_STAND_UP, ANIM_SIT_DOWN, PO_SIT, PO_STAND },
    { ANIM_LIE_DOWN, ANIM_GET_UP, PO_STAND, PO_LIE },            { ANIM_GET_UP, ANIM_LIE_DOWN, PO_LIE, PO_STAND },
    { ANIM_CURL_UP, ANIM_UNCURL, PO_LIE, PO_CURL },              { ANIM_UNCURL, ANIM_CURL_UP, PO_CURL, PO_LIE },
    { ANIM_EAT_IN, ANIM_EAT_OUT, PO_STAND, PO_EAT },             { ANIM_EAT_OUT, ANIM_EAT_IN, PO_EAT, PO_STAND },
    { ANIM_DRINK_IN, ANIM_DRINK_OUT, PO_STAND, PO_DRINK },       { ANIM_DRINK_OUT, ANIM_DRINK_IN, PO_DRINK, PO_STAND },
    { ANIM_SQUAT, ANIM_UNSQUAT, PO_STAND, PO_SQUAT },            { ANIM_UNSQUAT, ANIM_SQUAT, PO_SQUAT, PO_STAND },
    { ANIM_SQUAT_POOP, ANIM_UNSQUAT_POOP, PO_STAND, PO_POOP },   { ANIM_UNSQUAT_POOP, ANIM_SQUAT_POOP, PO_POOP, PO_STAND },
    { ANIM_EATTREAT_IN, 0, PO_STAND, PO_TREAT },                 { ANIM_LIEDOWN_TREAT, 0, PO_TREAT, PO_LIE },
};
#define N_MOVES (int)(sizeof MOVES / sizeof MOVES[0])
static int move_of(uint16_t anim) {
    for (int k = 0; k < N_MOVES; k++) if (MOVES[k].anim == anim) return k;
    return -1;
}

/* the pose an animation starts (end = false) or ends in, read off its first and last frames in every direction
   (docs/img/fix_2026-10-28/poses.png): the bow and the tug count as standing, the eating, drinking, pee, poop and
   treat sets hold the pose their move ends in (the chewing lies, where lying down with the treat ends) */
static int pose_of(uint16_t anim, bool end) {
    const int m = move_of(anim);
    if (m >= 0) return end ? MOVES[m].to : MOVES[m].from;
    switch (anim) {
    case ANIM_LIE_DOWN_ROPE:                             return end ? PO_LIE : PO_STAND;
    case ANIM_SLEEP_CURL:                                return PO_CURL;
    case ANIM_IDLE_SIT_BLINK: case ANIM_PANT_SMILE: case ANIM_HEAD_TILT: case ANIM_SUN_SIT: case ANIM_LOOK_AROUND:
    case ANIM_ASK_PET:                                   return PO_SIT;
    case ANIM_CHEW_TOY: case ANIM_PET_ENJOY: case ANIM_GUILTY_PEEK: case ANIM_CHEW_TREAT: return PO_LIE;
    case ANIM_EAT_DRINK:                                 return PO_EAT;
    case ANIM_DRINK:                                     return PO_DRINK;
    case ANIM_PEE:                                       return PO_SQUAT;
    case ANIM_POOP:                                      return PO_POOP;
    case ANIM_EAT_TREAT:                                 return PO_TREAT;
    case 0:                                              return PO_ANY;
    default:                                             return PO_STAND;
    }
}
static bool is_move(uint16_t anim) { return move_of(anim) >= 0; }
static uint16_t reverse_move(uint16_t m) { const int k = move_of(m); return k >= 0 ? MOVES[k].back : 0; }
/* the move from one pose towards another: straight there when a move does it, else out of this pose first (up to
   standing, or out of the ball onto her side), from standing to the ball the lie-down first; 0 = none needed, or
   none drawn (up from the treat: a cut, as before) */
static uint16_t move_between(int from, int to) {
    if (from == PO_ANY || to == PO_ANY || from == to) return 0;
    uint16_t out = 0;
    for (int k = 0; k < N_MOVES; k++) {
        if (MOVES[k].from != from) continue;
        if (MOVES[k].to == to) return MOVES[k].anim;
        if (MOVES[k].to == PO_STAND || from == PO_CURL) out = MOVES[k].anim;
    }
    return from == PO_STAND && to == PO_CURL ? ANIM_LIE_DOWN : out;
}
/* a move that ends in the pose she is in (asked to lie down when curled up counts: lying, nothing to do) */
static bool already(const actor_t *a, uint16_t move) {
    const int now = pose_of(a->anim, true), end = pose_of(move, true);
    return is_move(move) && (now == end || (end == PO_LIE && now == PO_CURL));
}

static bool source_set(uint16_t anim, sprite_set_t *s) {
    const pak_t *p = world_pak(); const uint16_t src = ANIM_REVERSE_OF(anim) ? ANIM_REVERSE_OF(anim) : anim;
    return p && pak_sprite_set(p, src, s) && s->fps && s->n_frames;
}
int pose_frame(const sprite_set_t *set, float t, bool reversed) {
    const int n = set->n_frames; int f = (int)(t * set->fps);
    if (n <= 1 || f < 0) return 0;
    switch (set->loop) {
    case 0:  if (f >= n) f = n - 1; break;                                                   /* once: it holds its last frame */
    case 2:  f %= 2 * n - 2; if (f >= n) f = 2 * n - 2 - f; break;                           /* there and back, round and round */
    case 3:  if (f > 2 * n - 2) f = 2 * n - 2; if (f >= n) f = 2 * n - 2 - f; break;         /* there and back once: it ends where it began */
    default: f %= n; break;                                                                  /* round and round */
    }
    return reversed ? n - 1 - f : f;
}
float pose_once_len(uint16_t anim) {
    sprite_set_t s;
    if (!source_set(anim, &s)) return 1.0f;
    return (float)(s.loop == 3 ? 2 * s.n_frames - 1 : s.n_frames) / s.fps;
}

/* getting up from lying and out of the ball are the moves in backwards, half as fast again: called, she should not
   take a second over each */
static void start(actor_t *a, uint16_t anim) { a->anim = anim; a->anim_t = 0; a->rate = anim == ANIM_GET_UP || anim == ANIM_UNCURL ? 1.5f : 1.0f; }
static bool move_over(const actor_t *a) { return a->anim_t >= pose_once_len(a->anim); }
static bool gait(uint16_t anim) {
    return anim == ANIM_WALK || anim == ANIM_TROT || anim == ANIM_RUN || anim == ANIM_TIPTOE || anim == ANIM_SCURRY || anim == ANIM_CARRY_TROT;
}
/* the nearest direction the animation has (the pak draws a missing one from it) */
static int near_dir(uint16_t anim, int d) {
    sprite_set_t s;
    if (!source_set(anim, &s) || (s.dir_mask >> d & 1)) return d;
    for (int k = 1; k <= 4; k++) {
        if (s.dir_mask >> ((d + k) & 7) & 1) return (d + k) & 7;
        if (s.dir_mask >> ((d - k) & 7) & 1) return (d - k) & 7;
    }
    return d;
}
/* the next step towards a->want from the pose the animation now ends in: a move, or the wanted one itself */
static uint16_t next_of(const actor_t *a) {
    if (pose_of(a->anim, true) == pose_of(a->want, true) && is_move(a->want)) return a->want;   /* there: its last frame */
    const uint16_t m = move_between(pose_of(a->anim, true), pose_of(a->want, false));
    return m && m != a->want ? m : a->want;
}
/* it starts once she faces a direction it has: walking north-west to the rug, the lie-down (south, south-west,
   south-east) was drawn south-west at once, front instead of back (2026-10-28); the gaits follow the path */
static void step(actor_t *a) {
    const uint16_t next = next_of(a);
    if (!gait(next) && near_dir(next, a->dir) != (int)a->dir) { a->facing = 1; return; }
    const uint16_t w = a->want; const bool there = next == w && is_move(w) && pose_of(a->anim, true) == pose_of(w, true);
    start(a, next); a->want = next == w ? 0 : w; a->facing = 0;
    if (there) a->anim_t = pose_once_len(next);
}

void pose_snap(actor_t *a, uint16_t anim) { start(a, anim); a->want = 0; a->facing = 0; }
bool pose_lying(const actor_t *a) { const int p = pose_of(a->anim, true); return p == PO_LIE || p == PO_CURL; }
bool pose_curled(const actor_t *a) { return pose_of(a->anim, true) == PO_CURL; }
int pose_near_dir(uint16_t anim, int d) { return near_dir(anim, d); }
bool pose_settling(const actor_t *a) { return a->want != 0; }
bool pose_posed(const actor_t *a, uint16_t move) { return a->anim != move && already(a, move); }
bool pose_resting(const actor_t *a) { const int p = pose_of(a->anim, true); return p == PO_SIT || p == PO_LIE || p == PO_CURL; }

void pose_play(actor_t *a, uint16_t anim) {
    if (a->want) { a->want = anim == a->anim ? 0 : anim; if (!a->want) a->facing = 0; return; }   /* a move or a turn first: then on to this */
    if (a->anim == anim) return;
    if (!world_pak()) { start(a, anim); return; }                       /* no sprites: nothing to move between */
    if (is_move(anim) && pose_of(a->anim, true) == pose_of(anim, true)) { start(a, anim); a->anim_t = pose_once_len(anim); return; }   /* already there: its last frame */
    sprite_set_t s;
    if (is_move(a->anim) && !move_over(a) && source_set(a->anim, &s)) {  /* in the middle of a move */
        const uint16_t back = reverse_move(a->anim);
        if (back && back == move_between(pose_of(a->anim, true), pose_of(anim, false))) {   /* back the way it came, from the frame it got to */
            const int f = pose_frame(&s, a->anim_t, false);
            start(a, back); a->anim_t = (float)(s.n_frames - 1 - f) / s.fps; a->want = anim == back ? 0 : anim;
        } else a->want = anim;                                          /* on after it */
        return;
    }
    a->want = anim; step(a);
}

bool pose_turn(actor_t *a, dir8_t to, float dt) {
    if (a->dir == to) { a->turn_t = 0; return true; }
    for (a->turn_t += dt; a->turn_t >= POSE_TURN_S && a->dir != to; a->turn_t -= POSE_TURN_S) {
        const int d = (to - a->dir) & 7;                                /* the short way round */
        a->dir = (dir8_t)((a->dir + (d <= 4 ? 1 : 7)) & 7);
    }
    if (a->dir == to) a->turn_t = 0;
    return a->dir == to;
}

void pose_tick(actor_t *a, float dt) {
    a->anim_t += dt * (a->rate > 0 ? a->rate : 1.0f);
    if (a->want && a->facing) { if (pose_turn(a, (dir8_t)near_dir(next_of(a), a->dir), dt)) step(a); }   /* turning to it */
    else if (a->want && !is_move(a->anim)) a->want = 0;                 /* the animation was set from outside (a scene, the demo) */
    else if (a->want && move_over(a)) step(a);
}
