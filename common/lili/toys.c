/* toys.c - see toys.h. Spec 8.9's numbers are on the 240 design grid: x LILI_K here. */
#include "toys.h"
#include "nav.h"
#include "maps_gen.h"
#include "asset_ids.h"
#include <math.h>

/* the owner (2026-10-28): the rope spun, flew far and bounced like a rubber ball. A knotted rope is soft and heavy:
   a short arc under a real-looking gravity (spec 8.9's 300 design px / s2 was a slow float), a thud at the landing
   (it skids on at about half its speed, at most one little hop after a hard throw), a short slide to a stop, a soft
   wall; it turns over only in the air, one picture per TOY_TURN_PX flown, and lies the way it landed.
   Then (same day): now it is a rock, it cannot be thrown; set it between the two. Measured with the flick speeds
   700 / 1200 / 2000 px / s: the rubber ball went 206 / 353 / 370 px and slid for 2.7 s, the rock 21 / 59 / 135 px;
   this goes 89 / 241 / 249 px, about 20 px high, one small hop, at rest in about a second, 2-6 turns in the air */
#define TOY_GRAVITY    (480.0f * LILI_K)
#define TOY_THROW_K    0.34f                                /* the flick's finger speed -> the toy's */
#define TOY_THROW_MAX  (210.0f * LILI_K)
#define TOY_LIFT       0.55f                                /* the arc: up at this part of the speed across, within: */
#define TOY_VZ_MIN     (58.0f * LILI_K)
#define TOY_VZ_MAX     (105.0f * LILI_K)
#define TOY_BOUNCE     0.3f                                 /* the hop's part of the landing speed, only above TOY_HOP_VZ */
#define TOY_HOP_VZ     (35.0f * LILI_K)
#define TOY_LAND_SKID  0.75f                                /* the speed across kept at a landing */
#define TOY_DRAG       (60.0f * LILI_K)                     /* on the floor: a steady braking (px / s2) ... */
#define TOY_WALL       0.45f
#define TOY_TURN_PX    (15.0f * LILI_K)
static const float FRICTION[TOY_COUNT] = { 3.8f, 4.5f };      /* ... and this per second: rope, plush */

void toys_init(world_t *w) {
    static const int START[TOY_COUNT] = { SPOT_ROPE_START, SPOT_PLUSH_START };
    for (int i = 0; i < TOY_COUNT; i++) {
        toy_t *t = &w->things.toys[i];
        const map_point_t *p = &MAPS[VIEW_ROOM].points[START[i]];
        t->view = VIEW_ROOM; t->x = p->x; t->y = p->y; t->z = 0; t->vx = t->vy = t->vz = 0; t->held_by = -1;
        t->rot = (uint8_t)(((int)t->x * 7 + (int)t->y * 13) & 3); t->spin = 0;    /* how it lies at its spot */
    }
    toys_away(w);
}
void toys_away(world_t *w) {
    if (!CONFIG_LILI_PLUSH_AWAY) return;
    toy_t *t = &w->things.toys[TOY_PLUSH];
    t->view = TOY_AWAY; t->held_by = -1; t->z = t->vx = t->vy = t->vz = 0;
}
uint8_t toy_rot_held(dir8_t d) {                              /* across her mouth as she faces: across, falling right, upright, rising right */
    static const uint8_t HELD[DIR_COUNT] = { 0, 3, 2, 1, 0, 3, 2, 1 };
    return HELD[d & 7];
}

/* her mouth from the paws, by facing (the room's 0.75 set; smaller outdoors), standing; lower in a bow, sniffing or lying.
   The PixelLab frames have no mouth anchor (spec 7.4's metadata), so measured by hand on idle_stand (2026-10-27: the
   rope sat on her chest at the old heights) */
static float mouth_k(uint16_t anim) {
    switch (anim) {
    case ANIM_SNIFF: return 0.35f;
    case ANIM_LIE_DOWN: case ANIM_SLEEP_CURL: case ANIM_CHEW_TREAT: return 0.5f;
    case ANIM_PLAY_BOW: case ANIM_POUNCE: return 0.55f;
    case ANIM_RUN: case ANIM_SCURRY: case ANIM_TIPTOE: return 0.85f;
    default: return 1.0f;
    }
}
void toy_mouth(const actor_t *a, float *dx, float *h) {
    static const int8_t DX[DIR_COUNT] = { 0, -14, -20, -14, 0, 14, 20, 14 }, H[DIR_COUNT] = { 33, 33, 34, 38, 39, 38, 34, 33 };
    const float k = a->view == VIEW_YARD ? 0.625f / 0.75f : 1.0f;
    *dx = DX[a->dir] * k; *h = H[a->dir] * k * mouth_k(a->anim);
}

int toy_held(const world_t *w, int actor) {
    for (int i = 0; i < TOY_COUNT; i++) if (w->things.toys[i].held_by == actor) return i;
    return -1;
}
int toy_near(const world_t *w, int actor, float r) {
    const actor_t *a = &w->actors[actor]; int best = -1; float bd = r * r;
    for (int i = 0; i < TOY_COUNT; i++) {
        const toy_t *t = &w->things.toys[i];
        if (t->held_by >= 0 || t->view != a->view) continue;
        float d = (t->x - a->x) * (t->x - a->x) + (t->y - a->y) * (t->y - a->y);
        if (d <= bd) { bd = d; best = i; }
    }
    return best;
}
void toy_pick(world_t *w, int toy, int actor) { toy_t *t = &w->things.toys[toy]; t->held_by = (int8_t)actor; t->vx = t->vy = t->vz = 0; }
void toy_drop(world_t *w, int toy) {
    toy_t *t = &w->things.toys[toy];
    if (t->held_by < 0) return;
    const actor_t *a = &w->actors[t->held_by]; float dx, h; toy_mouth(a, &dx, &h);
    t->held_by = -1; t->view = a->view; t->x = a->x + dx; t->y = a->y + 4; t->z = h * 0.5f; t->vx = t->vy = 0; t->vz = 0;
    t->rot = toy_rot_held(a->dir); t->spin = 0;               /* it falls the way it was in her mouth */
    if (!nav_clear(t->view, (int)t->x, (int)t->y, 0)) { t->x = a->x; t->y = a->y; }   /* not into a wall */
}
void toy_throw(world_t *w, int toy, float fvx, float fvy) {
    toy_t *t = &w->things.toys[toy];
    if (t->view >= VIEW_COUNT) return;                       /* in the toy box */
    float vx = fvx * TOY_THROW_K, vy = fvy * TOY_THROW_K, v = sqrtf(vx * vx + vy * vy);
    if (v > TOY_THROW_MAX) { vx *= TOY_THROW_MAX / v; vy *= TOY_THROW_MAX / v; v = TOY_THROW_MAX; }
    const float vz = v * TOY_LIFT;
    t->held_by = -1; t->vx = vx; t->vy = vy; t->vz = vz < TOY_VZ_MIN ? TOY_VZ_MIN : vz > TOY_VZ_MAX ? TOY_VZ_MAX : vz; t->spin = 0;
}

void toys_tick(world_t *w, float dt) {
    for (int i = 0; i < TOY_COUNT; i++) {
        toy_t *t = &w->things.toys[i];
        if (t->held_by >= 0) {                                  /* in her mouth: at her head, sorted just in front of her (behind her facing away) */
            const actor_t *a = &w->actors[t->held_by]; float dx, h; toy_mouth(a, &dx, &h);
            const bool away = a->dir == DIR_NW || a->dir == DIR_N || a->dir == DIR_NE;
            t->view = a->view; t->x = a->x + dx; t->y = a->y + (away ? -1 : 1); t->z = h + a->z + (away ? -2 : 0);
            continue;
        }
        if (t->view >= VIEW_COUNT) continue;                    /* in the toy box */
        if (t->z <= 0 && t->vz <= 0 && fabsf(t->vx) < 1 && fabsf(t->vy) < 1) { t->z = 0; t->vx = t->vy = t->vz = 0; continue; }
        /* the flight: gravity, a thud at the floor (a little hop only after a hard throw); on the floor the slide brakes */
        const bool flying = t->z > 0 || t->vz > 0;
        t->vz -= TOY_GRAVITY * dt; t->z += t->vz * dt;
        if (flying && t->z <= 0) {
            t->z = 0; t->vx *= TOY_LAND_SKID; t->vy *= TOY_LAND_SKID;
            t->vz = t->vz < -TOY_HOP_VZ ? -t->vz * TOY_BOUNCE : 0;
        }
        if (t->z <= 0) {
            t->z = 0; if (t->vz < 0) t->vz = 0;
            const float f = expf(-FRICTION[i] * dt), v = sqrtf(t->vx * t->vx + t->vy * t->vy), d = TOY_DRAG * dt;
            const float k = v > d ? (v - d) / v * f : 0;
            t->vx *= k; t->vy *= k;
        } else {                                                /* turning over as it flies */
            t->spin += sqrtf(t->vx * t->vx + t->vy * t->vy) * dt;
            if (t->spin >= TOY_TURN_PX) { t->spin -= TOY_TURN_PX; t->rot = (uint8_t)((t->rot + (t->vx >= 0 ? 1 : 3)) & 3); }
        }
        /* the walls: the walkable grid's edge sends it back, softly, axis by axis */
        float nx = t->x + t->vx * dt, ny = t->y + t->vy * dt;
        if (!nav_clear(t->view, (int)nx, (int)t->y, 0)) { t->vx = -t->vx * TOY_WALL; nx = t->x; }
        if (!nav_clear(t->view, (int)nx, (int)ny, 0)) { t->vy = -t->vy * TOY_WALL; ny = t->y; }
        t->x = nx; t->y = ny;
    }
}
