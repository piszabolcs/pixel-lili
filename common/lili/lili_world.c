/* lili_world.c - M2.1: an empty room and yard, and Lili as a placeholder capsule
 * (spec 7.7: nothing waits for the art). Platform-free C99, no malloc. */
#include "lili_world.h"
#include <string.h>

uint32_t world_rand(world_t *w) {                      /* xorshift32: fast, tiny, and the same sequence on every platform */
    uint32_t x = w->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return w->rng = x;
}

void world_init(world_t *w, uint32_t seed) {
    memset(w, 0, sizeof *w);
    w->seed = seed;
    w->rng = seed ? seed : 0x2545F491u;                /* xorshift must never hold 0 */
    w->view = VIEW_ROOM;
    w->season_force = -1; w->daypart_force = -1;       /* memset gave 0 = "spring": -1 means "from the date / the clock" */
    actor_t *lili = &w->actors[0];
    lili->present = true;
    lili->view = VIEW_ROOM;
    lili->x = LILI_LOG_W / 2.0f;                       /* spec 8.2 spot_center */
    lili->y = 140.0f * LILI_K;
    lili->dir = DIR_S;
    lili->anim = ANIM_IDLE_STAND;
}

static const pak_t *s_pak;
void world_set_pak(const pak_t *p) { s_pak = p && p->data ? p : NULL; }
const pak_t *world_pak(void) { return s_pak; }

void world_tick(world_t *w, float dt) {
    w->clock_s += dt;
    for (int a = 0; a < MAX_ACTORS; a++) if (w->actors[a].present) w->actors[a].anim_t += dt;
}

void world_switch_view(world_t *w) {
    w->view = (view_t)((w->view + 1) % VIEW_COUNT);
}

void world_handle_event(world_t *w, const ev_t *e) {
    if (e->type < EV_COUNT) w->events_seen[e->type]++;
    switch (e->type) {
    case EV_SWITCH_VIEW: world_switch_view(w); break;
    case EV_TIME_SKIP:   if (e->a > 0) w->clock_s += (float)e->a; break;
    case EV_SET_SEASON:  w->season_force = (int8_t)(e->a < 0 || e->a > 3 ? -1 : e->a); break;
    case EV_SET_DAYPART: w->daypart_force = (int8_t)(e->a < 0 || e->a > 3 ? -1 : e->a); break;
    default: break;      /* the finger, the body and the voice: counted above; the reflexes (M4.5) will answer them */
    }
}

/* ---- the placeholder picture (spec 7.7): the map's props as shaded boxes, Lili as a capsule ---- */
#include "palette.h"
#include "maps_gen.h"

const map_t *world_map(const world_t *w, view_t v) { (void)w; return &MAPS[v]; }

/* a prop seen from the 3/4 camera: its footprint (x, y, w, h) on the floor, raised by `height`:
   the top face is lighter, the front face darker, a 1 px outline around both (spec 7.1) */
/* the owner's rug (photo reference/photos/rug_owner_room_2026-10-19.jpg, 300 x 200 cm, lying
   lengthwise into the room). Its pattern as 12 triangles in rug coordinates (u across 0..1,
   v along 0..1, from the far end): green top-left, dark grey top-right, a big off-white band
   across the upper middle, light grey down the left, dark grey in the middle and right, dark
   grey bottom-left, green bottom-right. The vertices: the corners, two points on each edge,
   and two inside (P, Q); the 12 triangles tile the rectangle exactly. */
enum { RUG_GREEN, RUG_WHITE, RUG_LIGHT, RUG_DARK };
static const float RUG_V[12][2] = {
    { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 },                 /* A B C D: the corners */
    { 0.55f, 0 }, { 1, 0.35f }, { 1, 0.75f }, { 0.45f, 1 }, /* E F G H: on the top, right, right, bottom edges */
    { 0, 0.62f }, { 0, 0.28f },                             /* I J: on the left edge */
    { 0.5f, 0.3f }, { 0.5f, 0.68f },                        /* P Q: inside */
};
enum { VA, VB, VC, VD, VE, VF, VG, VH, VI, VJ, VP, VQ };
static const uint8_t RUG_T[12][4] = {                       /* three vertices and a colour */
    { VA, VE, VJ, RUG_GREEN }, { VE, VB, VF, RUG_DARK },  { VJ, VE, VP, RUG_WHITE }, { VE, VF, VP, RUG_WHITE },
    { VJ, VP, VI, RUG_LIGHT }, { VP, VF, VQ, RUG_DARK },  { VF, VG, VQ, RUG_DARK },  { VI, VP, VQ, RUG_LIGHT },
    { VI, VQ, VH, RUG_LIGHT }, { VI, VH, VD, RUG_DARK },  { VQ, VG, VH, RUG_LIGHT }, { VG, VC, VH, RUG_GREEN },
};
static float edge(const float *a, const float *b, float u, float v) { return (b[0] - a[0]) * (v - a[1]) - (b[1] - a[1]) * (u - a[0]); }
static int rug_colour(float u, float v) {                   /* the triangle holding (u, v): all three edge functions agree in sign */
    for (int t = 0; t < 12; t++) {
        const float *a = RUG_V[RUG_T[t][0]], *b = RUG_V[RUG_T[t][1]], *c = RUG_V[RUG_T[t][2]];
        float e0 = edge(a, b, u, v), e1 = edge(b, c, u, v), e2 = edge(c, a, u, v);
        if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) return RUG_T[t][3];
    }
    return RUG_LIGHT;
}
/* the speckle (a shaggy pile) comes from a hash of the pixel's own position, so it is the
   same in every frame: no flicker, no PRNG state */
static void draw_rug(render8_t *r, const map_prop_t *p) {
    const uint8_t col[4] = { pal_ramp(RAMP_ID_RUG_GREEN, 5), pal_ramp(RAMP_ID_RUG_GRAY, 7), pal_ramp(RAMP_ID_RUG_GRAY, 5), pal_ramp(RAMP_ID_RUG_GRAY, 2) };
    r8_fill(r, p->x, p->y, p->w, p->h, pal_ramp(RAMP_ID_RUG_GRAY, 1));          /* the bound edge */
    for (int y = 1; y < p->h - 1; y++)
        for (int x = 1; x < p->w - 1; x++) {
            uint8_t c = col[rug_colour((x + 0.5f) / p->w, (y + 0.5f) / p->h)];
            uint32_t hsh = (uint32_t)(p->x + x) * 73856093u ^ (uint32_t)(p->y + y) * 19349663u;
            if ((hsh >> 7) % 6 == 0) c = (uint8_t)(c - 1);                            /* a darker tuft: one shade down the ramp */
            int lx = p->x + x, ly = p->y + y;
            if (lx >= 0 && ly >= 0 && lx < LILI_LOG_W && ly < LILI_LOG_H) r->px[ly * LILI_LOG_W + lx] = c;
        }
}

static void draw_prop(render8_t *r, const map_prop_t *p) {
    if (p->pattern == 1) { draw_rug(r, p); return; }
    if (p->flat) {                                          /* a door, a pad: drawn on the floor/wall */
        r8_fill(r, p->x, p->y, p->w, p->h, PAL_OUTLINE);
        r8_fill(r, p->x + 1, p->y + 1, p->w - 2, p->h - 2, pal_ramp(p->ramp, 5));
        return;
    }
    int top = p->y - p->height;
    r8_fill(r, p->x - 1, top - 1, p->w + 2, p->h + p->height + 2, PAL_OUTLINE);
    r8_fill(r, p->x, top, p->w, p->h, pal_ramp(p->ramp, 6));                    /* top face */
    r8_fill(r, p->x, top + p->h, p->w, p->height, pal_ramp(p->ramp, 3));        /* front face */
}

static void draw_actor(render8_t *r, const actor_t *a) {    /* the sprite from the pak, or a capsule dog; its paws at (x, y) */
    int x = (int)a->x, y = (int)(a->y - a->z);
    const int K = LILI_K;
    sprite_set_t set; sprite_frame_t fr;
    if (s_pak && a->anim && pak_sprite_set(s_pak, a->anim, &set)) {
        int frame = (int)(a->anim_t * set.fps);
        if (!set.loop && frame >= set.n_frames) frame = set.n_frames - 1;
        if (pak_frame(&set, a->dir, frame, &fr)) { pak_blit(r, &set, &fr, x, y); return; }
    }
    r8_fill(r, x - 11 * K, y - 13 * K, 22 * K, 14 * K, PAL_OUTLINE);
    r8_fill(r, x - 10 * K, y - 12 * K, 20 * K, 12 * K, PAL_FUR + 5);
    r8_fill(r, x - 6 * K, y - 7 * K, 12 * K, 7 * K, PAL_FUR_W + 5);     /* the white chest (L3) */
    r8_fill(r, x - 9 * K, y - 19 * K, 10 * K, 8 * K, PAL_OUTLINE);
    r8_fill(r, x - 8 * K, y - 18 * K, 8 * K, 6 * K, PAL_FUR + 6);       /* the head */
}

/* ---- the painted world (2026-10-23): the owner's pictures from the pak, light and shadow as palette remaps ----
 * The static layer: the door view (the season's yard through the 9 glass panes), the room picture over it,
 * the pad; then where the light lands: the sunbeam through the door, the door's cast shadows of the bench,
 * the bowls and the vase, and the overhead light's contact shadows. Each is a mask in the pak and a 256-byte
 * table (index -> a lighter or darker index of the same picture's palette), so it is one look-up per pixel.
 * The time of day is the palette itself (the LUT): one tone per daypart over the picture's own colours. */
#include <math.h>

int world_season(const world_t *w) { return w->season_force >= 0 && w->season_force <= 3 ? w->season_force : LILI_SEASON_DEFAULT; }
int world_daypart(const world_t *w) {
    if (w->daypart_force >= 0 && w->daypart_force <= 3) return w->daypart_force;
    float h = fmodf(LILI_START_HOUR + w->clock_s / 3600.0f, 24.0f);
    return h >= 6 && h < 12 ? 0 : h >= 12 && h < 18 ? 1 : h >= 18 && h < 21 ? 2 : 3;
}
bool world_painted(void) { pak_image_t im; return s_pak && pak_image(s_pak, IMG_ROOM, &im); }

/* by daypart: the palette tone (multiply, add), and which light / shadow table applies (-1 = none) */
static const struct { float m[3], a[3]; int8_t sun, cast; } DAYPART[4] = {
    { { 1.00f, 0.98f, 0.96f }, { 0, 3, 8 },   LUT_SUN_WEAK,   LUT_CAST_DAY },   /* morning: a little cool */
    { { 1.00f, 1.00f, 1.00f }, { 0, 0, 0 },   LUT_SUN_STRONG, LUT_CAST_DAY },   /* afternoon: the sun through the door */
    { { 0.80f, 0.74f, 0.68f }, { 18, 6, 0 },  LUT_SUN_WEAK,   LUT_CAST_EVE },   /* evening: warmer, the door's light fades */
    { { 0.45f, 0.45f, 0.45f }, { 8, 12, 38 }, -1,             -1 },             /* night: only the overhead lamp's short shadows */
};
static uint16_t bg_palette(const world_t *w) { return w->view == VIEW_ROOM ? PAL_ID_ROOM : (uint16_t)(PAL_ID_YARD_SPRING + world_season(w)); }
static const uint8_t *bg_lut(const world_t *w, int t) { return t < 0 ? NULL : pak_lut(s_pak, LUT_ID(bg_palette(w), t)); }

static void painted_palette(const world_t *w, render8_t *r) {
    uint8_t pal[256][3];
    if (!pak_palette_id(s_pak, bg_palette(w), pal)) return;
    const int dp = world_daypart(w);
    for (int i = 1; i < 256; i++)
        for (int c = 0; c < 3; c++) {
            float v = pal[i][c] * DAYPART[dp].m[c] + DAYPART[dp].a[c];
            pal[i][c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    r8_set_palette(r, pal);
}

static void painted_bg(const world_t *w, render8_t *r) {
    pak_image_t im;
    const int season = world_season(w), dp = world_daypart(w);
    painted_palette(w, r);
    r8_clear(r, 0);
    if (w->view == VIEW_YARD) { if (pak_image(s_pak, (uint16_t)(IMG_YARD_SPRING + season), &im)) pak_image_blit(r, &im); return; }
    if (pak_image(s_pak, (uint16_t)(IMG_DOOR_SPRING + season), &im)) pak_image_blit(r, &im);   /* behind the glass */
    if (pak_image(s_pak, IMG_ROOM, &im)) pak_image_blit(r, &im);
    if (pak_image(s_pak, IMG_PAD, &im)) pak_image_blit(r, &im);                                  /* flat: always under Lili */
    if (pak_image(s_pak, IMG_MASK_SUN, &im)) pak_image_remap(r, &im, bg_lut(w, DAYPART[dp].sun));
    if (pak_image(s_pak, IMG_MASK_CAST, &im)) pak_image_remap(r, &im, bg_lut(w, DAYPART[dp].cast));
    if (pak_image(s_pak, IMG_MASK_SHADE_OUT, &im)) pak_image_remap(r, &im, bg_lut(w, LUT_SHADE_OUT));
    if (pak_image(s_pak, IMG_MASK_SHADE_IN, &im)) pak_image_remap(r, &im, bg_lut(w, LUT_SHADE_IN));
}

/* an actor's shadow, every frame, from the frame's own shape: the overhead light's short shadow under the
   paws (an ellipse as wide as the footprint: core and edge), and by day the cast shadow (every pixel at
   height h above the paws lands h * s towards the viewer; in the room sideways along the ray from the
   door, in the yard a little to the right like the trees' shadows). Levels per pixel, the darkest wins,
   then one table look-up each; nothing above the room's wall line. */
enum { SH_W = 200, SH_H = 72, SH_UP = 12 };
static uint8_t s_cell[96 * 96];                   /* one decoded frame */
static uint8_t s_shade[SH_W * SH_H];              /* 1 contact edge, 2 cast far, 3 cast near, 4 contact core */
/* how high she really is in a pose: the 3/4 camera shows a curled-up dog's back as depth, not height, so
   counting every sprite row as height threw a long flat slab under her that looked like a glass plate
   (the owner, 2026-10-23). Lying is a hand high, sitting and the play bow in between, standing the most. */
static int shadow_hmax(uint16_t anim) {
    switch (anim) {
    case ANIM_SLEEP_CURL: return 10;
    case ANIM_PLAY_BOW: case ANIM_EAT_DRINK: case ANIM_DRINK: return 28;
    case ANIM_IDLE_SIT_BLINK: case ANIM_PANT_SMILE: case ANIM_LOOK_AROUND: case ANIM_BARK_HEAD_FLIP: return 42;
    default: return LILI_SHADOW_HMAX;
    }
}
static void actor_shadow(const world_t *w, render8_t *r, const actor_t *a, const sprite_set_t *set, const sprite_frame_t *fr) {
    if (set->cell_w * set->cell_h > (int)sizeof s_cell) return;
    pak_frame_decode(set, fr, s_cell);
    const int W = set->cell_w, H = set->cell_h, px = (int)a->x, py = (int)(a->y - a->z);
    const int ox = px - set->pivot_x, oy = py - set->pivot_y, rx0 = px - SH_W / 2, ry0 = py - SH_UP;
    memset(s_shade, 0, sizeof s_shade);
    int fx0 = W, fx1 = -1;                                       /* the footprint: the bottom 8 rows above the paws */
    for (int y = set->pivot_y - 8; y < set->pivot_y && y < H; y++)
        for (int x = 0; x < W; x++) if (y >= 0 && s_cell[y * W + x]) { if (x < fx0) fx0 = x; if (x > fx1) fx1 = x; }
    if (fx1 >= fx0) {
        const float cx = ox + (fx0 + fx1) / 2.0f, rx = (fx1 - fx0) / 2.0f + 3, ry = 3 + (fx1 - fx0) / 16.0f;
        for (int y = 0; y < SH_H; y++) for (int x = 0; x < SH_W; x++) {
            float dx = (rx0 + x - cx) / rx, dy = (ry0 + y - (py - 1)) / ry, d = dx * dx + dy * dy;
            if (d <= 0.55f) s_shade[y * SH_W + x] = 4; else if (d <= 1.0f) s_shade[y * SH_W + x] = 1;
        }
    }
    const int dp = world_daypart(w);
    if (DAYPART[dp].cast >= 0) {
        const float s = w->view == VIEW_ROOM ? 0.6f : 0.35f;
        const int hmax = shadow_hmax(a->anim);
        for (int y = 0; y < set->pivot_y && y < H; y++) for (int x = 0; x < W; x++) {
            if (!s_cell[y * W + x]) continue;
            int h = set->pivot_y - y; if (h > hmax) h = hmax;
            float k = w->view == VIEW_ROOM ? (ox + x - LILI_ROOM_LIGHT_X) / (float)(py - LILI_ROOM_LIGHT_Y > 8 ? py - LILI_ROOM_LIGHT_Y : 8) : 0.45f;
            int sx = (int)(ox + x + h * s * k + 0.5f) - rx0, sy = (int)(py + h * s + 0.5f) - ry0;
            const uint8_t lv = h * 2 <= hmax ? 3 : 2;                 /* the half next to her darker, the far half lighter */
            for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {     /* a 2 x 2 splat: no gaps between the rows */
                int qx = sx + dx, qy = sy + dy;
                if (qx >= 0 && qy >= 0 && qx < SH_W && qy < SH_H && s_shade[qy * SH_W + qx] < lv) s_shade[qy * SH_W + qx] = lv;
            }
        }
    }
    /* by day the near half is the full cast shadow and the far half the evening's lighter one; in the
       evening the near half is the lighter one and the far half fades out */
    const int8_t near_t = DAYPART[dp].cast, far_t = DAYPART[dp].cast == LUT_CAST_DAY ? LUT_CAST_EVE : -1;
    const uint8_t *lut[5] = { NULL, bg_lut(w, LUT_SHADE_OUT), bg_lut(w, far_t), bg_lut(w, near_t), bg_lut(w, LUT_SHADE_IN) };
    const int top = w->view == VIEW_ROOM ? LILI_ROOM_FLOOR_Y : 0;
    for (int y = 0; y < SH_H; y++) {
        int cy = ry0 + y; if (cy < top || cy < 0 || cy >= LILI_LOG_H) continue;
        for (int x = 0; x < SH_W; x++) {
            int cx = rx0 + x, lv = s_shade[y * SH_W + x];
            if (!lv || cx < 0 || cx >= LILI_LOG_W || !lut[lv]) continue;
            uint8_t *d = &r->px[cy * LILI_LOG_W + cx]; *d = lut[lv][*d];
        }
    }
    (void)oy;
}

/* a bowl in Lili's eating / drinking frames: the room's own bowl steps aside while she uses it */
static bool prop_in_use(const world_t *w, uint16_t img) {
    for (int a = 0; a < MAX_ACTORS; a++) {
        const actor_t *ac = &w->actors[a];
        if (!ac->present || ac->view != VIEW_ROOM) continue;
        if ((img == IMG_BOWL_FOOD && ac->anim == ANIM_EAT_DRINK) || (img == IMG_BOWL_WATER && ac->anim == ANIM_DRINK)) return true;
    }
    return false;
}

static void painted_actor(const world_t *w, render8_t *r, const actor_t *a) {
    sprite_set_t set; sprite_frame_t fr;
    if (a->anim && pak_sprite_set(s_pak, a->anim, &set)) {
        int frame = (int)(a->anim_t * set.fps);
        if (!set.loop && frame >= set.n_frames) frame = set.n_frames - 1;
        if (pak_frame(&set, a->dir, frame, &fr)) {
            actor_shadow(w, r, a, &set, &fr);
            pak_blit(r, &set, &fr, (int)a->x, (int)(a->y - a->z));
            return;
        }
    }
    draw_actor(r, a);
}

static void painted_fg(const world_t *w, render8_t *r) {
    enum { MAXD = 8 }; int16_t key[MAXD]; int8_t who[MAXD]; int n = 0;   /* who: prop slot, or -1 - actor */
    pak_image_t props[3]; int np = 0;
    if (w->view == VIEW_ROOM) {
        static const uint16_t P[3] = { IMG_VASE, IMG_BOWL_FOOD, IMG_BOWL_WATER };
        /* at a bowl she stands beside the other one (the two sit side by side, her frames face west):
           that one goes under her instead of over her legs */
        const bool at_bowls = prop_in_use(w, IMG_BOWL_FOOD) || prop_in_use(w, IMG_BOWL_WATER);
        for (int i = 0; i < 3; i++)
            if (!prop_in_use(w, P[i]) && pak_image(s_pak, P[i], &props[np])) {
                key[n] = at_bowls && P[i] != IMG_VASE ? (int16_t)-1 : props[np].sort_y; who[n++] = (int8_t)np; np++;
            }
    }
    for (int a = 0; a < MAX_ACTORS && n < MAXD; a++)
        if (w->actors[a].present && w->actors[a].view == w->view) { key[n] = (int16_t)w->actors[a].y; who[n++] = (int8_t)(-1 - a); }
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && key[j - 1] > key[j]; j--) {
            int16_t tk = key[j]; key[j] = key[j - 1]; key[j - 1] = tk;
            int8_t tw = who[j]; who[j] = who[j - 1]; who[j - 1] = tw;
        }
    for (int i = 0; i < n; i++) {
        if (who[i] >= 0) pak_image_blit(r, &props[who[i]]);
        else painted_actor(w, r, &w->actors[-1 - who[i]]);
    }
}

uint32_t world_bg_key(const world_t *w) {               /* placeholder: the view only; painted: view, season and daypart */
    if (!world_painted()) return (uint32_t)w->view + 1;
    return 0x100u | (uint32_t)w->view | (uint32_t)world_season(w) << 2 | (uint32_t)world_daypart(w) << 4;
}

void world_render_bg(const world_t *w, render8_t *r) {
    if (world_painted()) { painted_bg(w, r); return; }
    const map_t *m = world_map(w, w->view);
    r8_clear(r, pal_ramp(m->floor_ramp, 5));
    r8_fill(r, 0, 0, LILI_LOG_W, m->wall_bottom, pal_ramp(m->wall_ramp, 4));
    r8_fill(r, 0, m->wall_bottom - 1, LILI_LOG_W, 1, pal_ramp(m->wall_ramp, 2));   /* the wall's foot */
    for (int i = 0; i < m->n_props; i++) if (m->props[i].flat) draw_prop(r, &m->props[i]);
}

void world_render_cached(const world_t *w, render8_t *r, bgcache_t *c) {
    uint32_t k = world_bg_key(w);
    if (!c->valid || c->key != k) { world_render_bg(w, r); memcpy(c->px, r->px, sizeof c->px); c->key = k; c->valid = 1; }
    else memcpy(r->px, c->px, sizeof c->px);
    world_render_fg(w, r);
}

void world_render(const world_t *w, render8_t *r) { world_render_bg(w, r); world_render_fg(w, r); }

void world_render_fg(const world_t *w, render8_t *r) {
    if (world_painted()) { painted_fg(w, r); return; }
    const map_t *m = world_map(w, w->view);
    /* the Y-sort (spec 7.2 step 3, 8.5): the flat props are already in the static layer; everything else
       by its front edge, so a prop in front of Lili covers her and one behind her is covered. Fixed arrays, no malloc. */
    enum { MAXD = 32 }; int16_t key[MAXD]; int8_t who[MAXD]; int n = 0;   /* who: prop index, or -1 - actor */
    for (int i = 0; i < m->n_props && n < MAXD; i++) {
        if (m->props[i].flat) continue;
        key[n] = m->props[i].sort_y; who[n++] = (int8_t)i;
    }
    for (int a = 0; a < MAX_ACTORS && n < MAXD; a++)
        if (w->actors[a].present && w->actors[a].view == w->view) { key[n] = (int16_t)w->actors[a].y; who[n++] = (int8_t)(-1 - a); }
    for (int i = 1; i < n; i++)                              /* insertion sort: n is a dozen, and it is stable */
        for (int j = i; j > 0 && key[j - 1] > key[j]; j--) {
            int16_t tk = key[j]; key[j] = key[j - 1]; key[j - 1] = tk;
            int8_t tw = who[j]; who[j] = who[j - 1]; who[j - 1] = tw;
        }
    for (int i = 0; i < n; i++) {
        if (who[i] >= 0) draw_prop(r, &m->props[who[i]]);
        else draw_actor(r, &w->actors[-1 - who[i]]);
    }
}

void world_debug_draw(const world_t *w, render8_t *r) {      /* M: the walkable grid and the points (spec 6.3) */
    const map_t *m = world_map(w, w->view);
    for (int gy = 0; gy < MAP_GH; gy++)
        for (int gx = 0; gx < MAP_GW; gx++)
            if (!map_walkable(m, gx * MAP_CELL + 1, gy * MAP_CELL + 1)) r->px[(gy * MAP_CELL + 2) * LILI_LOG_W + gx * MAP_CELL + 2] = PAL_DEBUG;
    for (int s = 0; s < SPOT_COUNT; s++) {
        const map_point_t *p = &m->points[s];
        if (p->z < 0) continue;
        r8_fill(r, p->x - 2, p->y - p->z, 5, 1, PAL_WHITE);
        r8_fill(r, p->x, p->y - p->z - 2, 1, 5, PAL_WHITE);
    }
}
