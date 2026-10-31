/* save.c - see save.h. Little-endian field by field (no struct copies: the sim and the ESP32 pad differently).
 * Layout: "LSAV", u16 version, u16 payload length, the payload, u32 CRC-32 of everything before it.
 * Payload v1 (append only, never reorder): i64 saved_at, i64 first_start, 6 x f32 needs, u8 guilty, u8 n_mess,
 * 4 x (i16, i16) spots, u8 pad, u8 food, u8 water, u8 view, f32 x, f32 y, u8 dir, u16 anim,
 * 2 x (u8 view, f32 x, f32 y, i8 held_by) toys, u32 pets, u32 plays, u8 last_kind,
 * then (appended) u8 brightness, u8 volume, u8 muted, u8 search hint, u8 bubbles, ..., and (2026-10-31) u8 n, n x u32
 * the statistics (STAT_*). */
#include "save.h"
#include "life.h"
#include "toys.h"
#include "pose.h"
#include <string.h>

static uint32_t crc32(const uint8_t *p, int n) {
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1))); }
    return ~c;
}
typedef struct { uint8_t *b; int n, cap; } wr_t;
static void put(wr_t *w, const void *v, int k) { if (w->n + k <= w->cap) memcpy(w->b + w->n, v, (size_t)k); w->n += k; }
static void u8(wr_t *w, uint8_t v) { put(w, &v, 1); }
static void u16(wr_t *w, uint16_t v) { uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; put(w, b, 2); }
static void u32(wr_t *w, uint32_t v) { uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) }; put(w, b, 4); }
static void i64(wr_t *w, int64_t v) { u32(w, (uint32_t)v); u32(w, (uint32_t)((uint64_t)v >> 32)); }
static void f32(wr_t *w, float v) { uint32_t u; memcpy(&u, &v, 4); u32(w, u); }

int save_write(const world_t *w, int64_t first_start, uint8_t *buf, int cap) {
    wr_t o = { buf, 0, cap };
    put(&o, "LSAV", 4); u16(&o, SAVE_VERSION); u16(&o, 0);           /* the length, filled in below */
    const int start = o.n;
    i64(&o, world_now(w)); i64(&o, first_start);
    f32(&o, w->hunger); f32(&o, w->energy); f32(&o, w->happy); f32(&o, w->bored); f32(&o, w->lonely); f32(&o, w->bladder);
    u8(&o, w->things.guilty); u8(&o, w->things.n_mess);
    for (int i = 0; i < MESS_MAX; i++) { u16(&o, (uint16_t)w->things.mess[i][0]); u16(&o, (uint16_t)w->things.mess[i][1]); }
    u8(&o, w->things.pad); u8(&o, w->things.food); u8(&o, w->things.water);
    const actor_t *a = &w->actors[0];
    u8(&o, (uint8_t)a->view); f32(&o, a->x); f32(&o, a->y); u8(&o, (uint8_t)a->dir); u16(&o, a->anim);
    for (int i = 0; i < TOY_COUNT; i++) { const toy_t *t = &w->things.toys[i]; u8(&o, (uint8_t)t->view); f32(&o, t->x); f32(&o, t->y); u8(&o, (uint8_t)t->held_by); }
    u32(&o, w->pets); u32(&o, w->plays); u8(&o, w->last_kind);
    u8(&o, w->set_bright); u8(&o, w->set_volume); u8(&o, w->set_muted); u8(&o, w->set_search); u8(&o, w->bubbles_on);   /* settings (appended 2026-10-27) */
    f32(&o, w->bowel); for (int i = 0; i < MESS_MAX; i++) u8(&o, w->things.mess_kind[i]);                              /* the poop, the treat (appended 2026-10-28) */
    u8(&o, w->things.pad_poop); u8(&o, (uint8_t)w->things.treat_view); u8(&o, w->things.treat_kind);
    u16(&o, (uint16_t)w->things.treat_x); u16(&o, (uint16_t)w->things.treat_y);
    u8(&o, STAT_COUNT); for (int i = 0; i < STAT_COUNT; i++) u32(&o, w->stat[i]);                                     /* the statistics page (2026-10-31) */
    const int len = o.n - start;
    if (o.n + 4 > cap) return 0;
    buf[6] = (uint8_t)len; buf[7] = (uint8_t)(len >> 8);
    u32(&o, crc32(buf, o.n));
    return o.n;
}

typedef struct { const uint8_t *b; int n, end; } rd_t;
static bool has(const rd_t *r, int k) { return r->n + k <= r->end; }
static uint8_t g8(rd_t *r, uint8_t d) { return has(r, 1) ? r->b[r->n++] : d; }
static uint16_t g16(rd_t *r, uint16_t d) { if (!has(r, 2)) return d; uint16_t v = (uint16_t)(r->b[r->n] | r->b[r->n + 1] << 8); r->n += 2; return v; }
static uint32_t g32(rd_t *r, uint32_t d) {
    if (!has(r, 4)) return d;
    uint32_t v = (uint32_t)r->b[r->n] | (uint32_t)r->b[r->n + 1] << 8 | (uint32_t)r->b[r->n + 2] << 16 | (uint32_t)r->b[r->n + 3] << 24; r->n += 4; return v;
}
static int64_t g64(rd_t *r, int64_t d) { if (!has(r, 8)) return d; uint32_t lo = g32(r, 0), hi = g32(r, 0); return (int64_t)((uint64_t)hi << 32 | lo); }
static float gf(rd_t *r, float d) { if (!has(r, 4)) return d; uint32_t u = g32(r, 0); float v; memcpy(&v, &u, 4); return v; }
static float clamp(float v, float lo, float hi) { return v != v ? lo : v < lo ? lo : v > hi ? hi : v; }   /* NaN -> lo */

bool save_read(world_t *w, const uint8_t *buf, int len, int64_t *saved_at, int64_t *first_start) {
    if (!buf || len < 12 || memcmp(buf, "LSAV", 4)) return false;
    const int plen = buf[6] | buf[7] << 8;
    if (8 + plen + 4 > len) return false;
    const uint32_t want = (uint32_t)buf[8 + plen] | (uint32_t)buf[9 + plen] << 8 | (uint32_t)buf[10 + plen] << 16 | (uint32_t)buf[11 + plen] << 24;
    if (crc32(buf, 8 + plen) != want) return false;
    rd_t r = { buf, 8, 8 + plen };                                    /* a field past the end (an older save): its default */
    int64_t at = g64(&r, 0), first = g64(&r, at);
    w->hunger = clamp(gf(&r, w->hunger), 0, 10); w->energy = clamp(gf(&r, w->energy), 0, 10); w->happy = clamp(gf(&r, w->happy), 0, 10);
    w->bored = clamp(gf(&r, w->bored), 0, 10); w->lonely = clamp(gf(&r, w->lonely), 0, 10); w->bladder = clamp(gf(&r, w->bladder), 0, 10);
    w->things.guilty = g8(&r, 0) != 0;
    const uint8_t nm = g8(&r, 0); w->things.n_mess = nm > MESS_MAX ? MESS_MAX : nm;
    for (int i = 0; i < MESS_MAX; i++) { w->things.mess[i][0] = (int16_t)g16(&r, 0); w->things.mess[i][1] = (int16_t)g16(&r, 0); }
    const uint8_t pad = g8(&r, 0), food = g8(&r, 3), water = g8(&r, 3);
    w->things.pad = pad > 2 ? 2 : pad; w->things.food = food > 3 ? 3 : food; w->things.water = water > 3 ? 3 : water;
    actor_t *a = &w->actors[0];
    const uint8_t v = g8(&r, 0); a->view = v < VIEW_COUNT ? (view_t)v : VIEW_ROOM;
    a->x = clamp(gf(&r, a->x), 0, LILI_LOG_W - 1); a->y = clamp(gf(&r, a->y), 0, LILI_LOG_H - 1);
    a->dir = (dir8_t)(g8(&r, 0) & 7); pose_snap(a, g16(&r, a->anim));
    for (int i = 0; i < TOY_COUNT; i++) {
        toy_t *t = &w->things.toys[i];
        const uint8_t tv = g8(&r, (uint8_t)t->view); t->view = tv < VIEW_COUNT ? (view_t)tv : VIEW_ROOM;
        t->x = clamp(gf(&r, t->x), 0, LILI_LOG_W - 1); t->y = clamp(gf(&r, t->y), 0, LILI_LOG_H - 1);
        const int8_t h = (int8_t)g8(&r, 0xFF); t->held_by = h >= 0 && h < MAX_ACTORS ? h : -1; t->z = t->vx = t->vy = t->vz = 0;
        t->rot = (uint8_t)(((int)t->x * 7 + (int)t->y * 13) & 3); t->spin = 0;
    }
    toys_away(w);                                             /* an older save had the plush out */
    w->pets = g32(&r, 0); w->plays = g32(&r, 0); w->last_kind = g8(&r, 7);
    w->set_bright = g8(&r, w->set_bright) % 3; w->set_volume = g8(&r, w->set_volume) % 4; w->set_muted = g8(&r, w->set_muted) != 0;
    w->set_search = g8(&r, w->set_search) != 0; w->bubbles_on = g8(&r, w->bubbles_on) != 0;
    w->bowel = clamp(gf(&r, w->bowel), 0, 10);
    for (int i = 0; i < MESS_MAX; i++) w->things.mess_kind[i] = g8(&r, 0) & 1;
    w->things.pad_poop = g8(&r, 0) != 0;
    { const int8_t tv = (int8_t)g8(&r, 0xFF); w->things.treat_view = tv >= 0 && tv < VIEW_COUNT ? tv : -1; }
    w->things.treat_kind = g8(&r, 0) & 1; w->things.treat_x = (int16_t)g16(&r, 0); w->things.treat_y = (int16_t)g16(&r, 0);
    { const int n = g8(&r, 0); for (int i = 0; i < n; i++) { const uint32_t v = g32(&r, 0); if (i < STAT_COUNT) w->stat[i] = v; } }
    w->first_start = first;
    if (saved_at) *saved_at = at;
    if (first_start) *first_start = first;
    return true;
}
