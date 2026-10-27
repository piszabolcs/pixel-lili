/* lili_world.c - M2.1: an empty room and yard, and Lili as a placeholder capsule
 * (spec 7.7: nothing waits for the art). Platform-free C99, no malloc. */
#include "lili_world.h"
#include "maps_gen.h"
#include "toys.h"
#include "plan.h"
#include "nav.h"
#include <stdlib.h>
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
    w->shake_cover = -1;
    actor_t *lili = &w->actors[0];
    lili->present = true;
    lili->view = VIEW_ROOM;
    lili->x = LILI_LOG_W / 2.0f;                       /* spec 8.2 spot_center */
    lili->y = 140.0f * LILI_K;
    lili->dir = DIR_S;
    lili->anim = ANIM_IDLE_STAND;
    lili->vis = 1.0f;
    toys_init(w);
    w->things.food = w->things.water = 3;                /* the bowls start full, the pad clean */
}

/* deep in a bush (a "hide_" zone of the map she is in) she fades out, out of it she fades back in
   (the owner, 2026-10-23: once she is more than about 60 % in, she should be gone, smoothly) */
static bool in_hide_zone(const actor_t *a) {
    const map_t *m = &MAPS[a->view];
    for (int i = 0; i < m->n_zones; i++) {
        const map_zone_t *z = &m->zones[i];
        if (z->kind == 0 && !strncmp(z->name, "hide", 4) && a->x >= z->a && a->x < z->a + z->c && a->y >= z->b && a->y < z->b + z->d) return true;
    }
    return false;
}

static const pak_t *s_pak;
void world_set_pak(const pak_t *p) { s_pak = p && p->data ? p : NULL; }
const pak_t *world_pak(void) { return s_pak; }

/* the yard cover her paws are in: the smallest whose cut-out rectangle holds (x, y) (a bush, not the tree
   canopy above it), -1 = none */
static int cover_at(const world_t *w, int x, int y) {
    pak_image_t im; int best = -1; long area = 1L << 30;
    for (int k = 0; s_pak && k < IMG_COVERS_PER_SEASON; k++)
        if (pak_image(s_pak, IMG_COVER(world_season(w), k), &im) && x >= im.x && x < im.x + im.w && y >= im.y && y < im.y + im.h && (long)im.w * im.h < area) {
            area = (long)im.w * im.h; best = k;
        }
    return best;
}

void world_tick(world_t *w, float dt) {
    w->clock_s += dt;
    if (w->shake_cover >= 0) w->shake_t += dt;
    if (w->sparkle_t > 0) w->sparkle_t -= dt;
    toys_tick(w, dt);
    for (int i = 0; i < MAX_ACTORS; i++) {
        actor_t *a = &w->actors[i];
        if (!a->present) continue;
        a->anim_t += dt;
        const bool hide = in_hide_zone(a);
        const plan_t *pl = &w->plans[i];
        const bool gone = hide || (pl->hide == 1 && pl->peek_t <= 0) || pl->hide == 3;   /* under the bench, or out of the door */
        /* the owner (2026-10-27): the bush stays still, it shakes once as she goes into it */
        if (hide && !w->bodies[i].in_hide && a->view == VIEW_YARD) { w->shake_cover = (int8_t)cover_at(w, (int)a->x, (int)a->y); w->shake_t = 0; }
        w->bodies[i].in_hide = hide;
        const float target = gone ? 0.0f : 1.0f, step = dt * 1.6f;   /* a 0.6 s fade */
        a->vis = a->vis < target ? (a->vis + step > target ? target : a->vis + step) : (a->vis - step < target ? target : a->vis - step);
    }
}

/* the shake: the cover's picture a pixel or two to the side and back, 0.45 s (spec 8.5's 3 frames, each twice) */
static int shake_dx(const world_t *w, int k) {
    static const int8_t DX[9] = { 2, -2, 1, -1, 1, -1, 0, 0, 0 };
    if (k != w->shake_cover || w->shake_t >= 0.45f || w->shake_t < 0) return 0;
    return DX[(int)(w->shake_t * 20.0f) % 9];
}

void world_finger(world_t *w, bool down, int x, int y) { w->finger_down = down; if (down) { w->finger_x = (int16_t)x; w->finger_y = (int16_t)y; } }

void world_switch_view(world_t *w) {
    w->view = (view_t)((w->view + 1) % VIEW_COUNT);
}

void world_handle_event(world_t *w, const ev_t *e) {
    if (e->type < EV_COUNT) w->events_seen[e->type]++;
    if (e->type == EV_TAP && world_care_tap(w, e->a, e->b)) return;   /* a bowl, the pad, a spot: the owner's care (spec 8.10) */
    reflex_event(w, e);                                  /* the reflexes first (spec 8.8): 0 ms, past the brain */
    switch (e->type) {
    case EV_SWITCH_VIEW: world_switch_view(w); break;
    case EV_TIME_SKIP:   if (e->a > 0) w->clock_s += (float)e->a; break;
    case EV_SET_SEASON:  w->season_force = (int8_t)(e->a < 0 || e->a > 3 ? -1 : e->a); break;
    case EV_SET_DAYPART: w->daypart_force = (int8_t)(e->a < 0 || e->a > 3 ? -1 : e->a); break;
    /* the toys under the finger (spec 5.3 DRAG_TOY, 8.9): a flick throws it, a drag carries it, a drop lets it fall */
    case EV_FLICK:       if (e->c >= 0 && e->c < TOY_COUNT && w->things.toys[e->c].held_by < 0) toy_throw(w, e->c, e->a, e->b); break;
    case EV_DRAG:        if (e->c >= 0 && e->c < TOY_COUNT && w->things.toys[e->c].held_by < 0) {
                             toy_t *t = &w->things.toys[e->c];
                             if (nav_clear(w->view, e->a, e->b, 0)) { t->x = e->a; t->y = e->b; }
                             t->z = 6; t->vx = t->vy = t->vz = 0;                     /* lifted a little while it is held */
                         }
                         break;
    case EV_DROP:        if (e->c >= 0 && e->c < TOY_COUNT) w->things.toys[e->c].vz = 0; break;   /* it falls from where it is */
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

/* the frame an actor shows now: its animation's set (offset: ANIM_YARD_OFFSET for the yard's size), or the set
   it plays backwards (stand_up is sit_down reversed: ANIM_REVERSE_OF, tools/pixellab_to_sprites.py REVERSED).
   A one-shot set holds its last frame; reversed, that is the first frame of the set it plays. */
bool world_actor_frame(const actor_t *a, uint16_t offset, sprite_set_t *set, sprite_frame_t *fr) {
    const uint16_t rev = ANIM_REVERSE_OF(a->anim);
    if (!s_pak || !a->anim || !pak_sprite_set(s_pak, (uint16_t)((rev ? rev : a->anim) + offset), set)) return false;
    int frame = (int)(a->anim_t * set->fps);
    if (!set->loop && frame >= set->n_frames) frame = set->n_frames - 1;
    if (rev) frame = set->n_frames - 1 - frame % set->n_frames;
    return pak_frame(set, a->dir, frame, fr);
}

static void draw_actor(render8_t *r, const actor_t *a) {    /* the sprite from the pak, or a capsule dog; its paws at (x, y) */
    int x = (int)a->x, y = (int)(a->y - a->z);
    const int K = LILI_K;
    sprite_set_t set; sprite_frame_t fr;
    if (world_actor_frame(a, 0, &set, &fr)) { pak_blit(r, &set, &fr, x, y); return; }
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
    if (pak_image(s_pak, IMG_MASK_CAST_FAR, &im)) pak_image_remap(r, &im, bg_lut(w, DAYPART[dp].cast == LUT_CAST_DAY ? LUT_CAST_EVE : -1));
    if (pak_image(s_pak, IMG_MASK_SHADE_OUT, &im)) pak_image_remap(r, &im, bg_lut(w, LUT_SHADE_OUT));
    if (pak_image(s_pak, IMG_MASK_SHADE_IN, &im)) pak_image_remap(r, &im, bg_lut(w, LUT_SHADE_IN));
}

/* an actor's shadow, every frame, from the frame's own shape: the overhead light's short shadow under the
   paws (an ellipse as wide as the footprint: core and edge), and by day the cast shadow (every pixel at
   height h above the paws lands h * s towards the viewer; in the room sideways along the ray from the
   door, in the yard a little to the right like the trees' shadows). Levels per pixel, the darkest wins,
   then one table look-up each; nothing above the room's wall line. */
enum { SH_W = 200, SH_H = 72, SH_UP = 12 };
static uint8_t s_cell[LILI_SPRITE_CELL_MAX * LILI_SPRITE_CELL_MAX];   /* one decoded frame */
static uint8_t s_shade[SH_W * SH_H];              /* 1 contact edge, 2 cast far, 3 cast middle, 4 cast near, 5 contact core */
/* how high she really is in a pose: the 3/4 camera shows a curled-up dog's back as depth, not height, so
   counting every sprite row as height threw a long flat slab under her that looked like a glass plate
   (the owner, 2026-10-23). Lying is a hand high, sitting and the play bow in between, standing the most. */
static int shadow_hmax(uint16_t anim) {
    switch (anim) {
    case ANIM_SLEEP_CURL: return 10;
    case ANIM_CHEW_TOY: case ANIM_CHEW_TREAT: case ANIM_LIE_DOWN_ROPE: return 18;                  /* lying, head up */
    case ANIM_PLAY_BOW: case ANIM_EAT_DRINK: case ANIM_DRINK: case ANIM_LIE_DOWN: case ANIM_TUG_ROPE:
    case ANIM_EAT_TREAT: case ANIM_SNIFF: return 28;                                                /* head down, or going down */
    case ANIM_IDLE_SIT_BLINK: case ANIM_PANT_SMILE: case ANIM_LOOK_AROUND: case ANIM_BARK_HEAD_FLIP: case ANIM_HEAD_TILT:
    case ANIM_SUN_SIT: case ANIM_ASK_PET: case ANIM_PET_ENJOY: case ANIM_SQUAT: case ANIM_PEE: case ANIM_POOP: return 42;   /* sitting, squatting */
    default: return LILI_SHADOW_HMAX;
    }
}
static void actor_shadow(const world_t *w, render8_t *r, const actor_t *a, const sprite_set_t *set, const sprite_frame_t *fr) {
    if (set->cell_w * set->cell_h > (int)sizeof s_cell) return;
    pak_frame_decode(set, fr, s_cell);
    const int W = set->cell_w, H = set->cell_h, px = (int)a->x, py = (int)(a->y - a->z);
    const int ox = px - set->pivot_x, oy = py - set->pivot_y, rx0 = px - SH_W / 2, ry0 = py - SH_UP;
    memset(s_shade, 0, sizeof s_shade);
    int fx0 = W, fx1 = -1;                                       /* the footprint: the bottom rows above the paws */
    for (int y = set->pivot_y - 1 - set->cell_h / 12; y < set->pivot_y && y < H; y++)
        for (int x = 0; x < W; x++) if (y >= 0 && s_cell[y * W + x]) { if (x < fx0) fx0 = x; if (x > fx1) fx1 = x; }
    if (fx1 >= fx0) {
        const float cx = ox + (fx0 + fx1) / 2.0f, rx = (fx1 - fx0) / 2.0f + 4, ry = 3 + (fx1 - fx0) / 12.0f;
        for (int y = 0; y < SH_H; y++) for (int x = 0; x < SH_W; x++) {
            float dx = (rx0 + x - cx) / rx, dy = (ry0 + y - (py - 1)) / ry, d = dx * dx + dy * dy;
            if (d <= 0.55f) s_shade[y * SH_W + x] = 5; else if (d <= 1.0f) s_shade[y * SH_W + x] = 1;
        }
    }
    const int dp = world_daypart(w);
    if (DAYPART[dp].cast >= 0) {
        /* how far the light throws it: a quarter of her height in the room, a little less outdoors; the height
           cap scales with the set's size (0.75 in the room, 0.625 in the yard: tools/pixellab_to_sprites.py SIZES),
           not with the cell, which is cropped to each set's frames since M3.5 */
        const float s = w->view == VIEW_ROOM ? 0.25f : 0.22f;
        const int hmax = shadow_hmax(a->anim) * (set->anim_id >= ANIM_YARD_OFFSET ? 5 : 6) / 8;
        /* the owner's rules (2026-10-23): the shadow starts at the paws, always, and it is her shape, not a block.
           So every pixel of her lands on one point (her shape), but the throw is short (a quarter of her height),
           so the body's shadow stays against the paws, and the dark contact patch under the paws joins the two.
           The far half is stippled (every other pixel): a soft edge instead of a hard slab.
           (Tried and dropped the same day: a long throw, 0.6, floated ahead of the thin legs; an unbroken strip
           per column from the paws was attached but a square block.) */
        const float ks = 0.6f;                                          /* the door's sideways pull, softened */
        const float reach = hmax * s;
        for (int y = 0; y < set->pivot_y && y < H; y++) for (int x = 0; x < W; x++) {
            if (!s_cell[y * W + x]) continue;
            int h = set->pivot_y - y; if (h > hmax) h = hmax;
            float k = w->view == VIEW_ROOM ? ks * (ox + x - LILI_ROOM_LIGHT_X) / (float)(py - LILI_ROOM_LIGHT_Y > 8 ? py - LILI_ROOM_LIGHT_Y : 8) : 0.45f;
            const float d = h * s;
            const uint8_t lv = d <= reach * 0.5f ? 4 : 2;                /* near: the full cast table; far: lighter, stippled */
            int sx = (int)(ox + x + d * k + 0.5f) - rx0, sy = (int)(py + d + 0.5f) - ry0;
            for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {     /* a 2 x 2 splat: no holes between the rows */
                int qx = sx + dx, qy = sy + dy;
                if (lv == 2 && ((rx0 + qx + ry0 + qy) & 1)) continue;
                if (qx >= 0 && qy >= 0 && qx < SH_W && qy < SH_H && s_shade[qy * SH_W + qx] < lv) s_shade[qy * SH_W + qx] = lv;
            }
        }
    }
    /* levels: 1 contact edge, 2 far, 3 middle, 4 near the paws, 5 contact core. By day near = the full cast
       shadow, middle = the contact edge's, far = the evening's lighter one; in the evening one step lighter
       each and the far part gone; at night only the contact shadow */
    const bool day = DAYPART[dp].cast == LUT_CAST_DAY;
    const uint8_t *lut[6] = { NULL, bg_lut(w, LUT_SHADE_OUT), bg_lut(w, day ? LUT_CAST_EVE : -1), bg_lut(w, day ? LUT_SHADE_OUT : LUT_CAST_EVE),
                              bg_lut(w, day ? LUT_CAST_DAY : LUT_SHADE_OUT), bg_lut(w, LUT_SHADE_IN) };
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

static void painted_actor(const world_t *w, render8_t *r, const actor_t *a) {
    sprite_set_t set; sprite_frame_t fr;
    if (a->vis <= 0.02f) return;                                          /* gone into a bush */
    /* outdoors the smaller set: the yard is painted at a smaller scale than the room (the owner, 2026-10-23) */
    if ((w->view == VIEW_YARD && world_actor_frame(a, ANIM_YARD_OFFSET, &set, &fr)) || world_actor_frame(a, 0, &set, &fr)) {
        if (a->vis >= 0.5f) actor_shadow(w, r, a, &set, &fr);
        /* fading in or out of a bush: an ordered (Bayer 4 x 4) dither, the 8-bit canvas has no alpha */
        pak_blit_dither(r, &set, &fr, (int)a->x, (int)(a->y - a->z), (int)(a->vis * 16.0f + 0.5f));
        return;
    }
    draw_actor(r, a);
}

/* a bowl in Lili's eating / drinking frames: the room's own bowl steps aside while she uses it, so her head
   is in the bowl her frames carry (the room's bowl in front of her hid her head: the owner, 2026-10-23) */
static bool prop_in_use(const world_t *w, uint16_t img) {
    for (int a = 0; a < MAX_ACTORS; a++) {
        const actor_t *ac = &w->actors[a];
        if (!ac->present || ac->view != VIEW_ROOM) continue;
        if ((img == IMG_BOWL_FOOD && ac->anim == ANIM_EAT_DRINK) || (img == IMG_BOWL_WATER && ac->anim == ANIM_DRINK)) return true;
    }
    return false;
}


/* ---- the toys and the spots on the floor (placeholder shapes until M3.8's art): drawn in Lili's 40 palette slots
   (144..183, the same colours in every background's palette), picked by brightness from the canvas's own LUT ---- */
static int lum565(uint16_t c) { return ((c >> 11) & 31) * 2 + ((c >> 5) & 63) + (c & 31); }
static void lili_slots(const render8_t *r, uint8_t *dark, uint8_t *mid, uint8_t *light, uint8_t *warm) {
    int dl = 1 << 30, ll = -1, wm = -(1 << 30), best_mid = 1 << 30; *dark = *mid = *light = *warm = 144;
    for (int i = 144; i < 184; i++) {
        const uint16_t c = r->lut[i]; const int L = lum565(c), R = (c >> 11) & 31, B = c & 31;
        if (L < dl) { dl = L; *dark = (uint8_t)i; }
        if (L > ll) { ll = L; *light = (uint8_t)i; }
        if (R * 2 - B * 2 - abs(L - 90) / 4 > wm) { wm = R * 2 - B * 2 - abs(L - 90) / 4; *warm = (uint8_t)i; }
        if (abs(L - 70) < best_mid) { best_mid = abs(L - 70); *mid = (uint8_t)i; }
    }
}
static void draw_toy(const world_t *w, render8_t *r, int k) {
    const toy_t *t = &w->things.toys[k];
    if (t->held_by >= 0 && k == TOY_ROPE && w->actors[t->held_by].anim == ANIM_CARRY_TROT) return;   /* those frames carry the rope */
    uint8_t dark, mid, light, warm; lili_slots(r, &dark, &mid, &light, &warm);
    const int x = (int)t->x, y = (int)(t->y - t->z);
    if (k == TOY_ROPE) {                                     /* a knotted rope: a light bar with warm stripes, knots at the ends */
        r8_fill(r, x - 9, y - 4, 18, 5, dark); r8_fill(r, x - 8, y - 3, 16, 3, light);
        for (int i = -6; i <= 6; i += 4) r8_fill(r, x + i, y - 3, 2, 3, warm);
        r8_fill(r, x - 11, y - 5, 4, 7, dark); r8_fill(r, x - 10, y - 4, 2, 5, warm);
        r8_fill(r, x + 7, y - 5, 4, 7, dark); r8_fill(r, x + 8, y - 4, 2, 5, warm);
    } else {                                                 /* a little plush: a round brown body, a light face, two ears */
        r8_fill(r, x - 6, y - 11, 12, 11, dark); r8_fill(r, x - 5, y - 10, 10, 9, mid); r8_fill(r, x - 3, y - 7, 6, 4, light);
        r8_fill(r, x - 6, y - 13, 3, 3, dark); r8_fill(r, x + 3, y - 13, 3, 3, dark);
        r8_fill(r, x - 2, y - 6, 1, 1, dark); r8_fill(r, x + 1, y - 6, 1, 1, dark);
    }
}
static void draw_mess(const world_t *w, render8_t *r) {
    if (w->view != VIEW_ROOM) return;
    uint8_t dark, mid, light, warm; lili_slots(r, &dark, &mid, &light, &warm);
    for (int i = 0; i < w->things.n_mess; i++) {             /* a puddle: a flat warm ellipse */
        const int x = w->things.mess[i][0], y = w->things.mess[i][1];
        r8_fill(r, x - 8, y - 2, 16, 4, warm); r8_fill(r, x - 11, y - 1, 22, 2, warm); r8_fill(r, x - 5, y - 2, 6, 1, light);
    }
}

/* ---- the care taps (spec 8.10, M4.7): a bowl refilled, the pad changed, a spot cleaned (the last one forgives
   her: R1); each with a sparkle. The hit areas are the props' pictures (or the map's boxes without the pak) ---- */
static bool prop_box(const world_t *w, uint16_t img, const char *name, int *x0, int *y0, int *x1, int *y1) {
    pak_image_t im;
    if (s_pak && pak_image(s_pak, img, &im)) { *x0 = im.x; *y0 = im.y; *x1 = im.x + im.w; *y1 = im.y + im.h; return true; }
    const map_t *m = &MAPS[VIEW_ROOM];
    for (int i = 0; i < m->n_props; i++) if (!strcmp(m->props[i].name, name)) {
        const map_prop_t *p = &m->props[i]; *x0 = p->x; *y0 = p->y - p->height; *x1 = p->x + p->w; *y1 = p->y + p->h; return true;
    }
    (void)w; return false;
}
static bool tap_in(const world_t *w, uint16_t img, const char *name, int x, int y) {
    int x0, y0, x1, y1;
    return prop_box(w, img, name, &x0, &y0, &x1, &y1) && x >= x0 - 4 && x < x1 + 4 && y >= y0 - 4 && y < y1 + 4;
}
static void sparkle(world_t *w, int x, int y) { w->sparkle_x = (int16_t)x; w->sparkle_y = (int16_t)y; w->sparkle_t = 0.6f; }
bool world_care_tap(world_t *w, int x, int y) {
    things_t *t = &w->things;
    if (w->view != VIEW_ROOM) return false;
    for (int i = 0; i < t->n_mess; i++)
        if (abs(x - t->mess[i][0]) <= 16 && abs(y - t->mess[i][1]) <= 10) {
            sparkle(w, t->mess[i][0], t->mess[i][1]);
            t->mess[i][0] = t->mess[t->n_mess - 1][0]; t->mess[i][1] = t->mess[t->n_mess - 1][1]; t->n_mess--;
            if (!t->n_mess) reflex_forgiven(w);                  /* R1: the last one */
            return true;
        }
    if (tap_in(w, IMG_BOWL_FOOD, "bowl_food", x, y) && t->food < 3) { t->food = 3; sparkle(w, x, y); return true; }
    if (tap_in(w, IMG_BOWL_WATER, "bowl_water", x, y) && t->water < 3) { t->water = 3; sparkle(w, x, y); return true; }
    if (tap_in(w, IMG_PAD, "pad", x, y) && t->pad) { t->pad = 0; sparkle(w, x, y); return true; }
    return false;
}
static void draw_sparkle(const world_t *w, render8_t *r) {
    if (w->sparkle_t <= 0 || w->view != VIEW_ROOM) return;
    uint8_t dark, mid, light, warm; lili_slots(r, &dark, &mid, &light, &warm);
    const int k = (int)(w->sparkle_t * 10) % 3, x = w->sparkle_x, y = w->sparkle_y;
    static const int8_t P[3][4][2] = { { { -8, -6 }, { 6, -10 }, { 0, 2 }, { 10, 0 } }, { { -4, -12 }, { 8, -4 }, { -10, 0 }, { 2, -2 } }, { { 0, -8 }, { -8, -2 }, { 8, -12 }, { 4, 4 } } };
    for (int i = 0; i < 4; i++) {                            /* little four-point stars, twinkling */
        const int sx = x + P[k][i][0], sy = y + P[k][i][1];
        r8_fill(r, sx - 2, sy, 5, 1, light); r8_fill(r, sx, sy - 2, 1, 5, light);
    }
}

static void painted_fg(const world_t *w, render8_t *r) {
    enum { MAXD = 16 }; int16_t key[MAXD]; int8_t who[MAXD]; int n = 0;  /* who: prop slot, or -1 - actor */
    pak_image_t props[IMG_COVERS_PER_SEASON + 3]; int np = 0;
    if (w->view == VIEW_YARD)                                             /* the trees, bushes and the bench she goes behind */
        for (int k = 0; k < IMG_COVERS_PER_SEASON && np < IMG_COVERS_PER_SEASON; k++)
            if (pak_image(s_pak, IMG_COVER(world_season(w), k), &props[np])) { props[np].x += shake_dx(w, k); key[n] = props[np].sort_y; who[n++] = (int8_t)np; np++; }
    if (w->view == VIEW_ROOM) {
        /* the bowls stand apart (tools/bg_build.py PROP_MOVE) so she has room beside each */
        static const uint16_t P[3] = { IMG_VASE, IMG_BOWL_FOOD, IMG_BOWL_WATER };
        for (int i = 0; i < 3; i++)
            if (!prop_in_use(w, P[i]) && pak_image(s_pak, P[i], &props[np])) { key[n] = props[np].sort_y; who[n++] = (int8_t)np; np++; }
    }
    for (int a = 0; a < MAX_ACTORS && n < MAXD; a++)
        if (w->actors[a].present && w->actors[a].view == w->view) { key[n] = (int16_t)w->actors[a].y; who[n++] = (int8_t)(-1 - a); }
    for (int t = 0; t < TOY_COUNT && n < MAXD; t++)
        if (w->things.toys[t].view == w->view) { key[n] = (int16_t)w->things.toys[t].y; who[n++] = (int8_t)(-10 - t); }
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && key[j - 1] > key[j]; j--) {
            int16_t tk = key[j]; key[j] = key[j - 1]; key[j - 1] = tk;
            int8_t tw = who[j]; who[j] = who[j - 1]; who[j - 1] = tw;
        }
    draw_mess(w, r);                                                      /* flat on the floor: under everything */
    for (int i = 0; i < n; i++) {
        if (who[i] >= 0) pak_image_blit(r, &props[who[i]]);
        else if (who[i] <= -10) draw_toy(w, r, -10 - who[i]);
        else painted_actor(w, r, &w->actors[-1 - who[i]]);
    }
    draw_sparkle(w, r);
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

void world_gesture_ctx(const world_t *w, gesture_ctx_t *c) {
    memset(c, 0, sizeof *c);
    const actor_t *a = &w->actors[0];
    sprite_set_t set; sprite_frame_t fr;
    if (a->present && a->view == w->view && a->vis > 0.5f) {
        const bool yard = w->view == VIEW_YARD;
        if ((yard && world_actor_frame(a, ANIM_YARD_OFFSET, &set, &fr)) || world_actor_frame(a, 0, &set, &fr)) {
            const int x = (int)a->x - set.pivot_x, y = (int)(a->y - a->z) - set.pivot_y;
            c->lili = (g_rect_t){ (int16_t)x, (int16_t)y, (int16_t)(x + set.cell_w), (int16_t)(y + set.cell_h) };
        } else {                                                    /* the capsule placeholder's size */
            c->lili = (g_rect_t){ (int16_t)(a->x - 22), (int16_t)(a->y - a->z - 38), (int16_t)(a->x + 22), (int16_t)(a->y - a->z + 2) };
        }
        c->bowing = a->anim == ANIM_PLAY_BOW;
    }
    for (int t = 0; t < TOY_COUNT; t++) {                            /* the loose toys here, where they are drawn */
        const toy_t *ty = &w->things.toys[t];
        if (ty->view != w->view || ty->held_by >= 0 || c->n_toys >= GESTURE_TOYS_MAX) continue;
        c->toy_x[c->n_toys] = (int16_t)ty->x; c->toy_y[c->n_toys] = (int16_t)(ty->y - ty->z - 4); c->n_toys++;
    }
    /* the door: the room's door picture, the yard's house door at the bottom (spot_door_yard, a finger wide around it) */
    const map_t *m = world_map(w, w->view);
    for (int i = 0; i < m->n_props; i++) if (!strcmp(m->props[i].name, "door")) {
        const map_prop_t *p = &m->props[i]; c->door = (g_rect_t){ p->x, p->y, (int16_t)(p->x + p->w), (int16_t)(p->y + p->h) };
    }
    const map_point_t *dy = &m->points[SPOT_DOOR_YARD];
    if (w->view == VIEW_YARD && dy->z >= 0) c->door = (g_rect_t){ (int16_t)(dy->x - 32), (int16_t)(dy->y - 12), (int16_t)(dy->x + 32), (int16_t)LILI_LOG_H };
}
