/* brain.c - see brain.h. */
#include "brain.h"
#include "plan.h"
#include "life.h"
#include "toys.h"
#include "nav.h"
#include "maps_gen.h"
#include "lili_world.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ---- the input (spec 10.3) ---- */
static bool zone_center(view_t v, const char *name, int *x, int *y) {
    const map_t *m = &MAPS[v];
    for (int i = 0; i < m->n_zones; i++) {
        const map_zone_t *z = &m->zones[i];
        if (strcmp(z->name, name)) continue;
        if (z->kind == 0) { *x = z->a + z->c / 2; *y = z->b + z->d / 2; return true; }
        long sx = 0, sy = 0; for (int k = 0; k < z->n; k++) { sx += z->poly[2 * k]; sy += z->poly[2 * k + 1]; }
        *x = (int)(sx / z->n); *y = (int)(sy / z->n); return true;
    }
    return false;
}
/* the brain's places and the map spots they are (a token may stand for several: the bushes) */
static const struct { uint8_t v; int8_t spot; } PLACES[] = {
    { V_SPOT_CENTER, SPOT_CENTER }, { V_SPOT_FRONT, SPOT_FRONT }, { V_SPOT_NOOK, SPOT_NOOK }, { V_SPOT_BOWLS, SPOT_BOWL_FOOD },
    { V_SPOT_BOWLS, SPOT_BOWL_WATER }, { V_SPOT_PAD, SPOT_PAD }, { V_SPOT_RUG, SPOT_RUG }, { V_SPOT_DOOR, SPOT_DOOR },
    { V_SPOT_DOOR, SPOT_DOOR_YARD }, { V_SPOT_BUSH, SPOT_BUSH_1 }, { V_SPOT_BUSH, SPOT_BUSH_2 }, { V_SPOT_BUSH, SPOT_BUSH_3 },
    { V_SPOT_TREE, SPOT_TREE }, { V_SPOT_GRASS, SPOT_GRASS }, { V_SPOT_FLOWERS, SPOT_FLOWERS }, { V_SPOT_SUNBEAM, -1 },
};
static uint8_t nearest_place(const actor_t *a) {
    const map_t *m = &MAPS[a->view]; float best = 1e12f; uint8_t v = a->view == VIEW_ROOM ? V_SPOT_CENTER : V_SPOT_GRASS;
    for (unsigned i = 0; i < sizeof PLACES / sizeof PLACES[0]; i++) {
        int x, y;
        if (PLACES[i].spot < 0) { if (a->view != VIEW_ROOM || !zone_center(VIEW_ROOM, "sunbeam", &x, &y)) continue; }
        else { const map_point_t *p = &m->points[PLACES[i].spot]; if (p->z < 0) continue; x = p->x; y = p->y; }
        const float d = (x - a->x) * (x - a->x) + (y - a->y) * (y - a->y);
        if (d < best) { best = d; v = PLACES[i].v; }
    }
    return v;
}
static uint8_t toy_token(const world_t *w, int toy, int actor) {
    const toy_t *t = &w->things.toys[toy]; const actor_t *a = &w->actors[actor];
    if (t->held_by == actor) return V_ROPE_MOUTH;                     /* rope and plush share the order: mouth near far other */
    if (t->view != a->view) return V_ROPE_OTHER;
    return (t->x - a->x) * (t->x - a->x) + (t->y - a->y) * (t->y - a->y) < 90.0f * 90.0f ? V_ROPE_NEAR : V_ROPE_FAR;
}
void brain_encode(const world_t *w, int actor, brain_state_t *s) {
    const actor_t *a = &w->actors[actor]; uint8_t *f = s->f; const things_t *t = &w->things;
    f[F_LOC] = a->view == VIEW_YARD ? V_LOC_YARD : V_LOC_ROOM;
    f[F_SPOT] = nearest_place(a);
    f[F_TOD] = (uint8_t)world_daypart(w); f[F_SEASON] = (uint8_t)world_season(w); f[F_DAY] = (uint8_t)world_day(w);
    f[F_HUNGER] = (uint8_t)life_bucket(w->hunger); f[F_ENERGY] = (uint8_t)life_bucket(w->energy); f[F_HAPPY] = (uint8_t)life_bucket(w->happy);
    f[F_BORED] = (uint8_t)life_bucket(w->bored); f[F_LONELY] = (uint8_t)life_bucket(w->lonely); f[F_BLADDER] = (uint8_t)life_bucket(w->bladder);
    f[F_GUILTY] = t->guilty ? V_GUILTY_1 : V_GUILTY_0;
    f[F_PAD] = t->pad == 0 ? V_PAD_CLEAN : t->pad == 1 ? V_PAD_USED : V_PAD_FULL;
    f[F_MESS] = t->n_mess == 0 ? V_MESS_0 : t->n_mess == 1 ? V_MESS_1 : V_MESS_MANY;
    f[F_BOWLS] = t->food >= 3 ? V_BOWLS_FULL : t->food == 0 ? V_BOWLS_EMPTY : V_BOWLS_LOW;   /* the food bowl (the water is in the eating plan) */
    f[F_OWNER] = w->owner_back == OWNER_BACK_LONG ? V_OWNER_BACK_LONG : w->owner_back == OWNER_BACK_SHORT ? V_OWNER_BACK_SHORT :
                 life_owner_active(w) ? V_OWNER_HERE : V_OWNER_IDLE;
    f[F_VIEW] = w->view == a->view ? V_VIEW_SAME : V_VIEW_OTHER;
    f[F_SIG] = w->brain_sig < SCHEMA_FIELD_N[F_SIG] ? w->brain_sig : V_SIG_NONE;
    f[F_ROPE] = toy_token(w, TOY_ROPE, actor); f[F_PLUSH] = toy_token(w, TOY_PLUSH, actor);
    f[F_SUN] = world_sun_on(w) && a->view == VIEW_ROOM ? V_SUN_ON : V_SUN_OFF;
    f[F_LAST] = w->last_kind < 8 ? w->last_kind : V_LAST_NONE;
    f[F_SINCE] = w->last_t < 120 ? V_SINCE_SHORT : w->last_t < 600 ? V_SINCE_MID : V_SINCE_LONG;
    f[F_ACTOR] = V_ACTOR_LILI;
}
int brain_prompt(const brain_state_t *s, uint8_t *ids) {
    int n = 0; ids[n++] = T_BOS;
    for (int i = 0; i < F_COUNT; i++) ids[n++] = (uint8_t)(SCHEMA_FIELD_FIRST[i] + s->f[i]);
    ids[n++] = T_ARROW;
    return n;
}

/* ---- the rules (spec 10.7) ---- */
#define MAX_RULES 64
static float s_probs[MAX_RULES];
const float *brain_last_probs(void) { return s_probs; }
static float rnd01(world_t *w) { return (world_rand(w) >> 8) * (1.0f / 16777216.0f); }
static int pick_possible(world_t *w, int actor, const uint8_t *alt, int n, bool only_possible) {
    int ok[4], k = 0;
    for (int i = 0; i < n; i++) if (!only_possible || alt[i] == ACT_NONE || plan_possible(w, actor, alt[i])) ok[k++] = alt[i];
    if (!k) return -1;
    return ok[world_rand(w) % (unsigned)k];
}
int brain_rules_decide(world_t *w, int actor, const brain_state_t *s, decision_t *out, float *margin) {
    float score[MAX_RULES]; bool valid[MAX_RULES]; float best = -1e30f;
    const int n = BRAIN_N_RULES < MAX_RULES ? BRAIN_N_RULES : MAX_RULES;
    for (int i = 0; i < n; i++) {
        const brain_rule_t *r = &BRAIN_RULES[i];
        valid[i] = r->when(s->f) && pick_possible(w, actor, r->alt[0], r->n_alt[0], true) >= 0 && r->alt[0][0] != ACT_NONE;
        if (!valid[i]) continue;
        score[i] = r->score(s->f);
        /* spec 10.11 P11: after a long while of one kind of thing, another kind (more so when bored) */
        if (s->f[F_SINCE] == V_SINCE_LONG && r->kind == s->f[F_LAST]) score[i] -= s->f[F_BORED] >= 6 ? 6.0f : 3.0f;
        if (score[i] > best) best = score[i];
    }
    float sum = 0;
    for (int i = 0; i < n; i++) { s_probs[i] = valid[i] ? expf((score[i] - best) / BRAIN_TAU) : 0; sum += s_probs[i]; }
    if (sum <= 0) { if (margin) *margin = 1; return -1; }
    float p1 = 0, p2 = 0;
    for (int i = 0; i < n; i++) { s_probs[i] /= sum; if (s_probs[i] > p1) { p2 = p1; p1 = s_probs[i]; } else if (s_probs[i] > p2) p2 = s_probs[i]; }
    float u = rnd01(w); int pick = -1;
    for (int i = 0; i < n && pick < 0; i++) if (valid[i]) { u -= s_probs[i]; if (u <= 0) pick = i; }
    if (pick < 0) for (int i = n - 1; i >= 0 && pick < 0; i--) if (valid[i]) pick = i;
    const brain_rule_t *r = &BRAIN_RULES[pick];
    for (int k = 0; k < 3; k++) {
        const int a = k < 3 && r->n_alt[k] ? pick_possible(w, actor, r->alt[k], r->n_alt[k], k == 0) : ACT_NONE;
        out->a[k] = (uint8_t)(a < 0 ? ACT_NONE : a);
    }
    out->style = r->style; out->face = r->face; out->bubble = r->bubble; out->urg = r->urg; out->margin = p1 - p2;
    if (margin) *margin = p1 - p2;
    return pick;
}

/* ---- the life loop (spec 8.7, 10.6) ---- */
void brain_owner_back(world_t *w, int64_t away_s) { w->owner_back = away_s >= 15 * 60 ? OWNER_BACK_LONG : away_s >= 60 ? OWNER_BACK_SHORT : OWNER_BACK_NONE; if (w->owner_back) w->ask_now = 1; }
void brain_ask_now(world_t *w) { w->ask_now = 1; }
void brain_signal(world_t *w, const ev_t *e) {
    uint8_t s = V_SIG_NONE;
    switch (e->type) {
    case EV_WHISTLE: s = V_SIG_WHISTLE; break;   case EV_CALL_NAME: s = V_SIG_NAME; break;
    case EV_VOICE_ACTIVITY: s = V_SIG_VOICE; break; case EV_SHAKE: s = V_SIG_SHAKE; break;
    case EV_PET_START: s = V_SIG_PET; break;     case EV_TAP: s = V_SIG_TAP; break;
    case EV_FLICK: s = V_SIG_TOY_THROWN; break;  default: return;
    }
    w->brain_sig = s; w->ask_now = 1;
}

/* is it time to ask (spec 10.6)? the triggers, the 3 s / 20 s, never over a reflex */
bool brain_due(world_t *w, float dt) {
    plan_t *p = &w->plans[0]; const actor_t *a = &w->actors[0];
    w->ask_t += dt;
    const uint8_t now[6] = { w->hunger >= 5, w->bladder >= 8, w->energy <= 3, (uint8_t)a->view, w->things.n_mess, life_owner_active(w) };
    for (int i = 0; i < 6; i++) {
        if (now[i] != w->mark[i] && (i >= 3 || now[i])) w->ask_now = 1;     /* the needs on the way up only */
        w->mark[i] = now[i];
    }
    if (p->status == PLAN_RUNNING && p->prio != 255) return false;        /* a reflex is answering: the brain waits */
    const bool idle = p->status != PLAN_RUNNING;
    if (w->ask_t < BRAIN_ASK_MIN_S) return false;
    return idle || w->ask_now || w->ask_t >= BRAIN_ASK_MAX_S;
}
/* the question has been put: the signal and the "back" are said, the clock restarts */
void brain_asked(world_t *w) { w->ask_t = 0; w->ask_now = 0; w->brain_sig = V_SIG_NONE; w->owner_back = OWNER_BACK_NONE; }
/* the answer: hesitation (spec 8.7), then the plan */
void brain_apply(world_t *w, decision_t *d, float margin, int rule) {
    w->rule = (int8_t)rule; w->margin = margin;
    if (rule < 0) return;
    if (margin < BRAIN_HESITATE && d->a[0] != ACT_HEAD_TILT) { d->a[2] = d->a[1]; d->a[1] = d->a[0]; d->a[0] = ACT_HEAD_TILT; }   /* torn: a head tilt first */
    plan_start(w, 0, d);
}
void brain_env_mask(world_t *w, int actor, uint8_t *mask) {
    for (int a = 0; a < ACT_COUNT; a++) mask[a] = a != ACT_NONE && plan_possible(w, actor, a);
}

static bool s_use_llm;
void brain_use_llm(bool on) { s_use_llm = on; }

void brain_tick(world_t *w, float dt) {
    if (!brain_due(w, dt)) return;
    brain_state_t s; decision_t d; float m = 1;
    brain_encode(w, 0, &s);
    int r = -1;
    if (s_use_llm && brain_llm_ready() && brain_llm_decide(w, 0, &s, &d, &m, BRAIN_TEMP) == 0) r = BRAIN_RULE_LLM;   /* the learned brain */
    if (r < 0) r = brain_rules_decide(w, 0, &s, &d, &m);                                                            /* the rules: the teacher, the fallback */
    brain_asked(w);
    brain_apply(w, &d, m, r);
}

/* ---- a world for a brain state (the probes, the traces' perturbations) ---- */
void brain_world_from_state(world_t *w, const brain_state_t *s) {
    const uint8_t *f = s->f; actor_t *a = &w->actors[0]; things_t *t = &w->things;
    const view_t v = f[F_LOC] == V_LOC_YARD ? VIEW_YARD : VIEW_ROOM;
    a->view = v; w->view = f[F_VIEW] == V_VIEW_SAME ? v : (view_t)(1 - v);
    const map_t *m = &MAPS[v]; bool placed = false;
    for (unsigned i = 0; i < sizeof PLACES / sizeof PLACES[0] && !placed; i++) {
        if (PLACES[i].v != f[F_SPOT]) continue;
        int x, y;
        if (PLACES[i].spot < 0) { if (v != VIEW_ROOM || !zone_center(VIEW_ROOM, "sunbeam", &x, &y)) continue; }
        else { const map_point_t *p = &m->points[PLACES[i].spot]; if (p->z < 0) continue; x = p->x; y = p->y; }
        a->x = (float)x; a->y = (float)y; placed = true;
    }
    if (!placed) { const map_point_t *p = &m->points[v == VIEW_ROOM ? SPOT_CENTER : SPOT_GRASS]; a->x = p->x; a->y = p->y; }
    w->daypart_force = (int8_t)f[F_TOD]; w->season_force = (int8_t)f[F_SEASON]; w->day_force = (int8_t)f[F_DAY];
    w->hunger = f[F_HUNGER] + 0.5f; w->energy = f[F_ENERGY] + 0.5f; w->happy = f[F_HAPPY] + 0.5f;
    w->bored = f[F_BORED] + 0.5f; w->lonely = f[F_LONELY] + 0.5f; w->bladder = f[F_BLADDER] + 0.5f;
    t->guilty = f[F_GUILTY] == V_GUILTY_1;
    t->pad = f[F_PAD] == V_PAD_CLEAN ? 0 : f[F_PAD] == V_PAD_USED ? 1 : 2;
    t->n_mess = f[F_MESS] == V_MESS_0 ? 0 : f[F_MESS] == V_MESS_1 ? 1 : 2;
    for (int i = 0; i < t->n_mess; i++) { t->mess[i][0] = (int16_t)(180 + 40 * i); t->mess[i][1] = 300; }
    t->food = t->water = f[F_BOWLS] == V_BOWLS_FULL ? 3 : f[F_BOWLS] == V_BOWLS_LOW ? 1 : 0;
    w->owner_back = f[F_OWNER] == V_OWNER_BACK_LONG ? OWNER_BACK_LONG : f[F_OWNER] == V_OWNER_BACK_SHORT ? OWNER_BACK_SHORT : OWNER_BACK_NONE;
    w->owner_t = f[F_OWNER] == V_OWNER_HERE ? 0 : 1e6f;
    w->brain_sig = f[F_SIG];
    for (int k = 0; k < TOY_COUNT; k++) {
        toy_t *ty = &t->toys[k]; const uint8_t tok = f[k == TOY_ROPE ? F_ROPE : F_PLUSH];
        ty->held_by = -1; ty->z = ty->vx = ty->vy = ty->vz = 0;
        if (tok == V_ROPE_MOUTH && toy_held(w, 0) < 0) { toy_pick(w, k, 0); ty->view = v; continue; }
        if (tok == V_ROPE_OTHER) { const map_point_t *p = &MAPS[1 - v].points[1 - v == VIEW_ROOM ? SPOT_CENTER : SPOT_GRASS]; ty->view = (view_t)(1 - v); ty->x = p->x; ty->y = p->y; continue; }
        ty->view = v;
        const float r = tok == V_ROPE_NEAR ? 40.0f : 180.0f;
        bool ok = false;
        for (int tries = 0; tries < 16 && !ok; tries++) {           /* a floor spot at that distance */
            const float ang = tries * 0.7f + k;
            const float x = a->x + r * cosf(ang), y = a->y + r * sinf(ang);
            if (nav_clear(v, (int)x, (int)y, 0)) { ty->x = x; ty->y = y; ok = true; }
        }
        if (!ok) { ty->x = a->x; ty->y = a->y; }
    }
    w->last_kind = f[F_LAST]; w->last_t = f[F_SINCE] == V_SINCE_SHORT ? 30.0f : f[F_SINCE] == V_SINCE_MID ? 300.0f : 1200.0f;
}
