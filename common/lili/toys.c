/* toys.c - see toys.h. Spec 8.9's numbers are on the 240 design grid: x LILI_K here. */
#include "toys.h"
#include "nav.h"
#include "maps_gen.h"
#include <math.h>

#define TOY_GRAVITY    (300.0f * LILI_K)
#define TOY_BOUNCE     0.4f
#define TOY_THROW_K    0.35f
#define TOY_THROW_MAX  (220.0f * LILI_K)
#define TOY_THROW_VZ   (60.0f * LILI_K)
static const float FRICTION[TOY_COUNT] = { 2.8f, 3.5f };      /* per second on the floor: rope, plush */

void toys_init(world_t *w) {
    static const int START[TOY_COUNT] = { SPOT_ROPE_START, SPOT_PLUSH_START };
    for (int i = 0; i < TOY_COUNT; i++) {
        toy_t *t = &w->things.toys[i];
        const map_point_t *p = &MAPS[VIEW_ROOM].points[START[i]];
        t->view = VIEW_ROOM; t->x = p->x; t->y = p->y; t->z = 0; t->vx = t->vy = t->vz = 0; t->held_by = -1;
    }
}

/* her mouth from the paws, by facing (the room's 0.75 set; smaller outdoors): the PixelLab frames have no mouth
   anchor yet (spec 7.4's metadata), so a hand-measured point per direction */
void toy_mouth(const actor_t *a, float *dx, float *h) {
    static const int8_t DX[DIR_COUNT] = { 0, -16, -24, -14, 0, 14, 24, 16 }, H[DIR_COUNT] = { 30, 32, 34, 40, 42, 40, 34, 32 };
    const float k = a->view == VIEW_YARD ? 0.625f / 0.75f : 1.0f;
    *dx = DX[a->dir] * k; *h = H[a->dir] * k;
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
    if (!nav_clear(t->view, (int)t->x, (int)t->y, 0)) { t->x = a->x; t->y = a->y; }   /* not into a wall */
}
void toy_throw(world_t *w, int toy, float fvx, float fvy) {
    toy_t *t = &w->things.toys[toy];
    float vx = fvx * TOY_THROW_K, vy = fvy * TOY_THROW_K, v = sqrtf(vx * vx + vy * vy);
    if (v > TOY_THROW_MAX) { vx *= TOY_THROW_MAX / v; vy *= TOY_THROW_MAX / v; }
    t->held_by = -1; t->vx = vx; t->vy = vy; t->vz = TOY_THROW_VZ;
}

void toys_tick(world_t *w, float dt) {
    for (int i = 0; i < TOY_COUNT; i++) {
        toy_t *t = &w->things.toys[i];
        if (t->held_by >= 0) {                                  /* in her mouth: at her head, sorted just in front of her */
            const actor_t *a = &w->actors[t->held_by]; float dx, h; toy_mouth(a, &dx, &h);
            t->view = a->view; t->x = a->x + dx; t->y = a->y + 1; t->z = h + a->z;
            continue;
        }
        if (t->z <= 0 && t->vz <= 0 && fabsf(t->vx) < 1 && fabsf(t->vy) < 1) { t->z = 0; t->vx = t->vy = t->vz = 0; continue; }
        /* the flight: gravity, a bounce at the floor; on the floor the slide slows down */
        t->vz -= TOY_GRAVITY * dt; t->z += t->vz * dt;
        if (t->z <= 0) { t->z = 0; t->vz = t->vz < -20.0f ? -t->vz * TOY_BOUNCE : 0; }
        if (t->z == 0) { const float f = expf(-FRICTION[i] * dt); t->vx *= f; t->vy *= f; }
        /* the walls: the walkable grid's edge reflects it, axis by axis */
        float nx = t->x + t->vx * dt, ny = t->y + t->vy * dt;
        if (!nav_clear(t->view, (int)nx, (int)t->y, 0)) { t->vx = -t->vx * 0.6f; nx = t->x; }
        if (!nav_clear(t->view, (int)nx, (int)ny, 0)) { t->vy = -t->vy * 0.6f; ny = t->y; }
        t->x = nx; t->y = ny;
    }
}
