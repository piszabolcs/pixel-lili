/* critters.c - see critters.h */
#include "critters.h"
#include "lili_world.h"
#include "nav.h"
#include "plan.h"
#include <math.h>
#include <string.h>

#define BIRD_SPEED   110.0f     /* px / s in the air */
#define BIRD_FLEE_R  80.0f      /* Lili this near: off it goes */
#define FLY_Z        110.0f     /* how high it comes in */

static uint32_t rnd(world_t *w) { uint32_t s = w->crit_rng; s ^= s << 13; s ^= s >> 17; s ^= s << 5; return w->crit_rng = s ? s : 0x2545F491u; }
static float frand(world_t *w, float a, float b) { return a + (b - a) * (float)(rnd(w) % 10000) / 10000.0f; }

void critters_init(world_t *w) {
    memset(w->crit, 0, sizeof w->crit); memset(w->flake, 0, sizeof w->flake);
    w->crit_rng = w->seed * 2654435761u + 77; w->crit_next = 20.0f;
}
static bool lili_here(const world_t *w) { const actor_t *a = &w->actors[0]; return a->present && a->view == VIEW_YARD && a->vis > 0.5f; }
static float lili_d(const world_t *w, float x, float y) { const actor_t *a = &w->actors[0]; return hypotf(a->x - x, a->y - y); }

static void spawn_bird(world_t *w, critter_t *c) {
    for (int k = 0; k < 30; k++) {                                          /* somewhere on the open grass */
        const int x = 60 + (int)(rnd(w) % 360), y = 190 + (int)(rnd(w) % 230);
        if (!nav_clear(VIEW_YARD, x, y, 2) || (lili_here(w) && lili_d(w, (float)x, (float)y) < BIRD_FLEE_R * 1.5f)) continue;
        memset(c, 0, sizeof *c);
        c->kind = CRIT_BIRD; c->state = CS_FLY_IN; c->tx = (float)x; c->ty = (float)y;
        c->x = rnd(w) & 1 ? -20.0f : LILI_LOG_W + 20.0f; c->y = c->ty - 40; c->z = FLY_Z;
        c->dir = (int8_t)(c->x < 0 ? DIR_E : DIR_W);
        return;
    }
}
static void spawn_butterfly(world_t *w, critter_t *c) {
    memset(c, 0, sizeof *c);
    c->kind = CRIT_BUTTERFLY; c->state = CS_WANDER; c->x = frand(w, 40, 440); c->y = -10; c->z = 40;
    c->tx = frand(w, 40, 440); c->ty = frand(w, 140, 400); c->t = frand(w, 40, 80); c->anim_t = frand(w, 0, 2);
}

static void bird_tick(world_t *w, critter_t *c, float dt) {
    c->anim_t += dt;
    if (c->state == CS_FLY_IN) {
        const float dx = c->tx - c->x, dy = c->ty - c->y, d = hypotf(dx, dy);
        if (d < 3) {
            c->state = CS_GROUND; c->z = 0; c->t = frand(w, 8, 16); c->hop = frand(w, 1, 2.5f); c->anim_t = 0;
            if (lili_here(w) && rnd(w) % 100 < 60) { w->crit_x = (int16_t)c->x; w->crit_y = (int16_t)c->y; reflex_critter(w, CRIT_BIRD); }
            return;
        }
        const float s = BIRD_SPEED * dt / d; c->x += dx * s; c->y += dy * s;
        c->z = FLY_Z * fminf(1.0f, d / 160.0f);                             /* down the last stretch */
        return;
    }
    if (c->state == CS_GROUND) {
        c->t -= dt; c->hop -= dt;
        if (c->hop <= 0) {                                                  /* a hop or two aside */
            const float nx = c->x + frand(w, -14, 14), ny = c->y + frand(w, -6, 6);
            if (nav_clear(VIEW_YARD, (int)nx, (int)ny, 1)) { c->dir = (int8_t)(nx > c->x ? DIR_E : DIR_W); c->x = nx; c->y = ny; }
            c->hop = frand(w, 1, 2.5f);
        }
        if (c->t <= 0 || (lili_here(w) && lili_d(w, c->x, c->y) < BIRD_FLEE_R)) {
            c->state = CS_FLEE; c->anim_t = 0;
            const actor_t *a = &w->actors[0];
            c->vx = (lili_here(w) ? (c->x >= a->x ? 1.0f : -1.0f) : (rnd(w) & 1 ? 1.0f : -1.0f)) * BIRD_SPEED * 1.3f;
            c->vy = -30; c->dir = (int8_t)(c->vx > 0 ? DIR_E : DIR_W);
        }
        return;
    }
    c->x += c->vx * dt; c->y += c->vy * dt; c->z += 70.0f * dt;             /* CS_FLEE: up and away */
    if (c->x < -40 || c->x > LILI_LOG_W + 40 || c->z > 260) c->kind = CRIT_NONE;
}
static void butterfly_tick(world_t *w, critter_t *c, float dt) {
    c->anim_t += dt; c->t -= dt;
    const float dx = c->tx - c->x, dy = c->ty - c->y, d = hypotf(dx, dy);
    if (d < 6 || (lili_here(w) && lili_d(w, c->tx, c->ty) < 40)) {       /* a new flower to make for */
        c->tx = frand(w, 40, 440); c->ty = c->t > 0 ? frand(w, 140, 400) : -60;
        if (c->t > 0 && lili_here(w) && lili_d(w, c->x, c->y) < 120 && rnd(w) % 100 < 15) {   /* she noticed it */
            w->crit_x = (int16_t)c->x; w->crit_y = (int16_t)c->y; reflex_critter(w, CRIT_BUTTERFLY);
        }
    }
    if (d > 0.1f) { const float s = 34.0f * dt / d; c->x += dx * s + 10.0f * dt * sinf(c->anim_t * 5.0f); c->y += dy * s; }
    c->z = 34.0f + 14.0f * sinf(c->anim_t * 2.3f);                          /* bobbing */
    if (c->y < -40) c->kind = CRIT_NONE;
}

/* leaves from the two trees' crowns (autumn), snow over the whole yard (winter) */
static void flakes_tick(world_t *w, float dt, int season) {
    const bool leaves = season == 2, snow = season == 3;
    for (int i = 0; i < CRIT_FLAKES; i++) {
        flake_t *f = &w->flake[i];
        if (!f->on) {
            if (!(leaves || snow) || rnd(w) % 1000 > (snow ? 40u : 6u)) continue;
            f->on = (uint8_t)(leaves ? 1 : 2); f->t = frand(w, 0, 6.3f);
            if (leaves) { const bool left = rnd(w) & 1; f->x = left ? frand(w, 10, 130) : frand(w, 350, 460); f->y = left ? frand(w, 120, 260) : frand(w, 20, 150); }
            else { f->x = frand(w, 0, LILI_LOG_W); f->y = -4; }
            f->ground = f->y + (leaves ? frand(w, 60, 200) : frand(w, 120, 480));
            f->vy = leaves ? frand(w, 16, 26) : frand(w, 18, 32); f->rest = 0;
            continue;
        }
        if (f->rest > 0) { if ((f->rest -= dt) <= 0) f->on = 0; continue; }    /* lying a moment, then gone */
        f->t += dt;
        f->x += (f->on == 1 ? 18.0f : 8.0f) * sinf(f->t * (f->on == 1 ? 2.2f : 1.3f)) * dt;
        f->y += f->vy * dt;
        if (f->y >= f->ground) f->rest = f->on == 1 ? 5.0f : 1.0f;
        if (f->y > LILI_LOG_H + 4) f->on = 0;
    }
}

void critters_tick(world_t *w, float dt) {
    const int season = world_season(w), part = world_daypart(w);
    int birds = 0, flies = 0;
    for (int i = 0; i < CRIT_MAX; i++) {
        critter_t *c = &w->crit[i];
        if (c->kind == CRIT_BIRD) { bird_tick(w, c, dt); birds++; }
        else if (c->kind == CRIT_BUTTERFLY) { butterfly_tick(w, c, dt); flies++; }
    }
    if (w->view == VIEW_YARD) flakes_tick(w, dt, season);
    if ((w->crit_next -= dt) > 0 || part == 3) return;
    w->crit_next = frand(w, 25, 70);
    for (int i = 0; i < CRIT_MAX; i++) if (!w->crit[i].kind) {
        if (!birds && rnd(w) % 100 < 70) spawn_bird(w, &w->crit[i]);
        else if (flies < 2 && season <= 1) spawn_butterfly(w, &w->crit[i]);
        break;
    }
}
