/* lili_main.c - the Pixel Lili simulator. The same common/lili code the firmware
 * runs, in an SDL window through LVGL (the pocket-tank's sim recipe).
 *
 *   ./lilisim                         the window: the picture + the inspector (buttons, state, event console)
 *   ./lilisim --inspector-shot o.ppm  the whole window to a PPM (SDL_VIDEODRIVER=dummy: no display needed)
 *   ./lilisim --selftest              headless checks, exit 0 = green
 *   ./lilisim --bench                 headless: render + flush cost per frame (spec 6.4)
 *   ./lilisim --screenshot out.ppm    headless, one frame to a PPM
 *   ./lilisim --demo                  the window with the demo tour running (G toggles it)
 *   ./lilisim --demo-record DIR [fps] headless, one round of the demo tour as PPM frames
 *   ./lilisim --anim-viewer [name]    the window: one animation from all 8 directions (A/arrows, Tab yard size, Space pause)
 *   ./lilisim --anim-viewer-shot DIR [frame]  headless, DIR/<anim>.ppm of the viewer for every animation
 *   ./lilisim --snapshot DIR          headless, both views x 4 seasons x 4 dayparts as DIR/<view>_<season>_<daypart>.ppm
 *   ./lilisim [--fresh]               the window keeps Lili in lili_save.bin: the time away is lived when it opens
 *                                     (spec 9.6), and she is saved every 5 minutes and at Q (--fresh: a new start)
 *
 * The fish simulator (main.c) is untouched and still builds with make SQUARE=1. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "lili_world.h"
#include "render8.h"
#include "palette.h"
#include "mask.h"
#include "maps_gen.h"
#include "events.h"
#include "pak.h"
#include "lvgl/lvgl.h"
#include <SDL2/SDL.h>

static world_t s_world;
static render8_t s_r8;
static bgcache_t s_bg;
static pak_t s_pak;                          /* ../assets/build/assets.pak, read once at start (the sim may malloc here: not the frame loop) */                       /* the static layer, drawn once per view (M2.6) */
static uint16_t s_fb[LILI_PHYS_W * LILI_PHYS_H];
static mask_t s_mask;
static bool s_debug;                                   /* M: the corner-zone debug layer */

static void pak_load(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) { printf("pak: no %s (placeholder graphics)\n", path); return; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) == (size_t)n && pak_open(&s_pak, buf, (uint32_t)n)) { world_set_pak(&s_pak); printf("pak: %s, %ld B, %d entries\n", path, n, s_pak.n); }
    else printf("pak: %s is not a valid LPAK (placeholder graphics)\n", path);
    fclose(f);
}
static void canvas_init(void) {
    uint8_t pal[256][3];
    if (!pak_palette(&s_pak, pal)) pal_master(pal);        /* the pak's palette, else the one compiled in (the same bytes) */
    r8_set_palette(&s_r8, pal);
    mask_init(&s_mask);
}
static void frame(void) {
    world_render_cached(&s_world, &s_r8, &s_bg);
    if (s_debug) { world_debug_draw(&s_world, &s_r8); mask_debug_draw(&s_r8, PAL_DEBUG); }
    r8_flush_rows(&s_r8, s_fb, 0, LILI_PHYS_H);        /* the sim takes the whole frame; the panel takes stripes */
    mask_apply_rows(&s_mask, s_fb, 0, LILI_PHYS_H);    /* the glass's corners and edge fade, on the same rows */
}

/* ---------- headless ---------- */
static void write_ppm(const char *path, const uint16_t *fb) {
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", LILI_PHYS_W, LILI_PHYS_H);
    for (int i = 0; i < LILI_PHYS_W * LILI_PHYS_H; i++) {
        uint16_t p = fb[i];
        unsigned char rgb[3] = { (unsigned char)(((p >> 11) & 31) << 3), (unsigned char)(((p >> 5) & 63) << 2), (unsigned char)((p & 31) << 3) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); return 1; } } while (0)
static uint16_t px(int lx, int ly) { return s_fb[(ly * LILI_SCALE) * LILI_PHYS_W + lx * LILI_SCALE]; }
/* the placeholder shapes (spec 7.7) are still the picture without a pak: their tests switch the pak off */
static void placeholder_mode(bool on) {
    uint8_t pal[256][3];
    world_set_pak(on || !s_pak.data ? NULL : &s_pak);
    if (on) { pal_master(pal); r8_set_palette(&s_r8, pal); }
}
/* the animations in spec 7.4's order: A01-A09 (M3.5; sit down, then stand up, the same frames backwards),
   A10-A27 (M3.6), then the ones beyond the spec's list; the A key and the --anim-viewer step through it */
static const struct { uint16_t id; const char *name; } ANIMS[] = {
    { ANIM_IDLE_STAND, "idle_stand" }, { ANIM_IDLE_SIT_BLINK, "idle_sit_blink" }, { ANIM_SIT_DOWN, "sit_down" },
    { ANIM_STAND_UP, "stand_up" }, { ANIM_WALK, "walk" }, { ANIM_TROT, "trot" }, { ANIM_RUN, "run" },
    { ANIM_TIPTOE, "tiptoe" }, { ANIM_SCURRY, "scurry" }, { ANIM_PANT_SMILE, "pant_smile" },
    { ANIM_HEAD_TILT, "head_tilt" }, { ANIM_PLAY_BOW, "play_bow" }, { ANIM_BARK_HEAD_FLIP, "bark_head_flip" },
    { ANIM_CHEW_TOY, "chew_toy" }, { ANIM_PET_ENJOY, "pet_enjoy" }, { ANIM_EAT_DRINK, "eat_drink" },
    { ANIM_SLEEP_CURL, "sleep_curl" }, { ANIM_SUN_SIT, "sun_sit" }, { ANIM_SQUAT, "squat" },
    { ANIM_LOOK_AROUND, "look_around" }, { ANIM_SNIFF, "sniff" }, { ANIM_STARTLE, "startle" },
    { ANIM_POUNCE, "pounce" }, { ANIM_ASK_PET, "ask_pet" }, { ANIM_DRINK, "drink" },
    { ANIM_LIE_DOWN, "lie_down" }, { ANIM_PEE, "pee" }, { ANIM_POOP, "poop" }, { ANIM_TUG_ROPE, "tug_rope" },
    { ANIM_EAT_TREAT, "eat_treat" }, { ANIM_CARRY_TROT, "carry_trot" }, { ANIM_LIE_DOWN_ROPE, "lie_down_rope" },
    { ANIM_CHEW_TREAT, "chew_treat" } };
enum { N_ANIMS = (int)(sizeof ANIMS / sizeof ANIMS[0]) };
static const map_prop_t *prop_named(const map_t *m, const char *name) {
    for (int i = 0; i < m->n_props; i++) if (!strcmp(m->props[i].name, name)) return &m->props[i];
    return NULL;
}

static int selftest(void) {
    const int K = LILI_K;                               /* the tests below were written on the 240 design grid */
    canvas_init();
    /* --- render8: the LUT, the 2x, the stripes, the transparency --- */
    CHECK(s_r8.lut[PAL_WHITE] == 0xFFFF || s_r8.lut[PAL_WHITE] == 0xFFDE, "white maps to RGB565 white");
    CHECK(s_r8.lut[PAL_OUTLINE] == (uint16_t)(((PAL_MASTER[1][0] & 0xF8) << 8) | ((PAL_MASTER[1][1] & 0xFC) << 3) | (PAL_MASTER[1][2] >> 3)), "the outline's RGB565 is exact (the master palette's slot 1)");
    r8_clear(&s_r8, PAL_FLOOR + 5); r8_fill(&s_r8, 10, 20, 1, 1, PAL_DEBUG);
    r8_flush_rows(&s_r8, s_fb, 0, LILI_PHYS_H);
    for (int dy = 0; dy < LILI_SCALE; dy++) for (int dx = 0; dx < LILI_SCALE; dx++)
        CHECK(s_fb[(20 * LILI_SCALE + dy) * LILI_PHYS_W + 10 * LILI_SCALE + dx] == s_r8.lut[PAL_DEBUG], "one logical pixel = LILI_SCALE^2 physical ones");
    CHECK(s_fb[(20 * LILI_SCALE + LILI_SCALE) * LILI_PHYS_W + 10 * LILI_SCALE] == s_r8.lut[PAL_FLOOR + 5] && s_fb[20 * LILI_SCALE * LILI_PHYS_W + 11 * LILI_SCALE] == s_r8.lut[PAL_FLOOR + 5], "and no more");
    static uint16_t stripe[24 * LILI_PHYS_W];
    for (int y0 = 0; y0 < LILI_PHYS_H; y0 += 24) {     /* the panel's 24-row stripes give the same picture */
        r8_flush_rows(&s_r8, stripe, y0, 24);
        CHECK(!memcmp(stripe, &s_fb[y0 * LILI_PHYS_W], sizeof stripe), "a 24-row stripe equals the full flush");
    }
    r8_flush_rows(&s_r8, stripe, 41, 3);               /* a stripe starting on an odd row */
    CHECK(!memcmp(stripe, &s_fb[41 * LILI_PHYS_W], 3 * LILI_PHYS_W * sizeof(uint16_t)), "an odd-row stripe equals the full flush");
    const uint8_t spr[4] = { PAL_WHITE, PAL_CLEAR, PAL_CLEAR, PAL_WHITE };
    r8_clear(&s_r8, PAL_FLOOR); r8_blit(&s_r8, spr, 2, 2, 5, 5);
    CHECK(s_r8.px[5 * LILI_LOG_W + 5] == PAL_WHITE && s_r8.px[5 * LILI_LOG_W + 6] == PAL_FLOOR, "index 0 is transparent in a blit");
    r8_fill(&s_r8, -5, -5, 10, 10, PAL_WHITE); r8_fill(&s_r8, LILI_LOG_W - 5, LILI_LOG_H - 5, 20, 20, PAL_WHITE);
    CHECK(s_r8.px[0] == PAL_WHITE && s_r8.px[LILI_LOG_W * LILI_LOG_H - 1] == PAL_WHITE, "fills clip at the edges");

    /* --- the glass mask (M1.7: the 72 px corner) --- */
    CHECK(mask_att(&s_mask, 0, 0) == 0 && mask_att(&s_mask, 479, 479) == 0, "the corners are black");
    CHECK(mask_att(&s_mask, 240, 240) == 255 && mask_att(&s_mask, 10, 240) == 255, "the middle and 10 px in from an edge are untouched");
    CHECK(mask_att(&s_mask, 0, 240) > 0 && mask_att(&s_mask, 0, 240) < 64, "the outermost column fades, not cut (the glass shows px 0, M1.7)");
    CHECK(mask_att(&s_mask, 24, 462) == 0 && mask_att(&s_mask, 32, 462) > 0, "y 462: x 24 is under the bezel, x 32 is not (the M1.7 footer sum)");
    for (int y = 0; y < LILI_PHYS_H; y += 7) for (int x = 0; x < LILI_PHYS_W; x += 7)
        CHECK(mask_att(&s_mask, x, y) == mask_att(&s_mask, 479 - x, y) && mask_att(&s_mask, x, y) == mask_att(&s_mask, x, 479 - y), "the frame is symmetric");
    long touched = 0; for (int y = 0; y < MASK_Q; y++) touched += s_mask.span[y];
    CHECK(touched * 4 < LILI_PHYS_W * LILI_PHYS_H / 8, "the mask touches only the frame's band");
    for (int i = 0; i < LILI_PHYS_W * LILI_PHYS_H; i++) s_fb[i] = 0xFFFF;
    mask_apply_rows(&s_mask, s_fb, 0, LILI_PHYS_H);
    CHECK(s_fb[0] == 0 && s_fb[240 * LILI_PHYS_W + 240] == 0xFFFF, "apply: corner black, centre white");
    static uint16_t full[LILI_PHYS_W * LILI_PHYS_H]; memcpy(full, s_fb, sizeof full);
    for (int i = 0; i < LILI_PHYS_W * LILI_PHYS_H; i++) s_fb[i] = 0xFFFF;
    for (int y0 = 0; y0 < LILI_PHYS_H; y0 += 24) mask_apply_rows(&s_mask, &s_fb[y0 * LILI_PHYS_W], y0, 24);
    CHECK(!memcmp(full, s_fb, sizeof full), "the mask in 24-row stripes equals the full pass");
    CHECK(!mask_point_safe(2, 2) && mask_point_safe(120, 120) && mask_point_safe(120, 3), "safe zone: a corner no, the middle yes, an edge's middle yes");
    CHECK(mask_point_safe(198, 196) && mask_point_safe(114, 181), "spec 8.2: the water bowl and the pad centres are safe");

    /* --- the maps (assets/maps/<name>.json -> maps_gen.c) --- */
    for (int v = 0; v < VIEW_COUNT; v++) {
        const map_t *m = &MAPS[v];
        for (int sp = 0; sp < SPOT_COUNT; sp++) {
            const map_point_t *p = &m->points[sp];
            if (p->z == 0 && !map_walkable(m, p->x, p->y)) { printf("FAIL: %s point %d (%d,%d) is not walkable\n", m->name, sp, p->x, p->y); return 1; }
        }
    }
    const map_t *room = &MAPS[VIEW_ROOM], *yard = &MAPS[VIEW_YARD];
    CHECK(map_walkable(room, 52 * K, 126 * K) && room->points[SPOT_RUG].z == 0, "the rug is walkable: she plays and sleeps on it (owner, 2026-10-19)");
    CHECK(room->points[SPOT_PAD].x < LILI_LOG_W / 3 && room->points[SPOT_BOWL_FOOD].x >= LILI_LOG_W / 2 && room->points[SPOT_BOWL_WATER].x >= LILI_LOG_W / 2,
          "the pad in the left third, food and water in the right half (owner, 2026-10-19; moved apart 2026-10-23 so she can eat and drink beside them)");
    CHECK(room->points[SPOT_DECO].x >= LILI_LOG_W / 3 && room->points[SPOT_DECO].x < 2 * LILI_LOG_W / 3, "the season's decoration in the middle third");
    CHECK(map_walkable(room, 208 * K, 150 * K), "the nook under the bench is walkable (H10, H11)");
    CHECK(!map_walkable(yard, 24 * K, 165 * K) && !map_walkable(yard, 120 * K, 10 * K), "the yard: a tree trunk and the fence block (the painted yard, 2026-10-23)");
    for (int i = 0; i < room->n_props; i++) {                /* every interactive thing is wholly on the glass, 2 px in */
        const map_prop_t *p = &room->props[i];
        if (strcmp(p->name, "pad") && strcmp(p->name, "bowl_food") && strcmp(p->name, "bowl_water")) continue;
        if (!mask_rect_visible(p->x, p->y - p->height, p->w, p->h + p->height, 2)) { printf("FAIL: %s is not wholly on the glass\n", p->name); return 1; }
    }
    CHECK(room->props[0].x >= 0, "props exist");
    const map_prop_t *side = prop_named(room, "bench_side");
    int floor_right = 232 * K;                          /* the painted room's floor ends at its right wall (room.json floor) */
    CHECK(side && floor_right - (side->x + side->w) <= 5 * K, "the bench stands at the right wall (owner, 2026-10-19; the painted room, 2026-10-23)");
    CHECK(side->y + side->h <= 180 * K && mask_rect_visible(side->x, side->y - side->height, side->w, side->h + side->height, 2),
          "the bench keeps its length and stays clear of the bottom-right corner arc");
    const map_prop_t *rug = prop_named(room, "rug");
    CHECK(rug && rug->x <= room->points[SPOT_RUG].x && room->points[SPOT_RUG].x < rug->x + rug->w && rug->y <= room->points[SPOT_RUG].y && room->points[SPOT_RUG].y < rug->y + rug->h,
          "the rug spot is on the painted rug (it is the painting's shape now, not 3:2)");
    /* the Y-sort, in the yard (H13): behind a bush Lili is covered by it, in front of it she covers it (placeholder shapes) */
    placeholder_mode(true);
    const map_prop_t *bush = prop_named(yard, "bush_3");
    CHECK(bush, "the yard has a bush at the top");
    world_init(&s_world, 1); actor_t *l = &s_world.actors[0];
    s_world.view = VIEW_YARD; l->view = VIEW_YARD;
    const int bx = bush->x + bush->w / 2;
    l->x = bx; l->y = bush->y - 4 * K; frame();         /* just behind the footprint: the bush's raised top face hides her */
    uint16_t behind = px(bx, bush->y - 10 * K);
    l->y = bush->y + bush->h + 36 * K; frame();
    int front_dog = 0;                                  /* the sprite above the pivot: anything that is not grass */
    for (int yy = (int)l->y - 20 * K; yy < (int)l->y; yy++) for (int xx = bx - 4 * K; xx < bx + 4 * K; xx++) {
        uint16_t c = px(xx, yy); int grass = 0;
        for (int sh = 0; sh < 8; sh++) grass |= c == s_r8.lut[pal_ramp(RAMP_ID_GRASS, sh)];
        front_dog += !grass;
    }
    CHECK(behind == s_r8.lut[pal_ramp(RAMP_ID_HEDGE, 6)], "behind a bush, the bush covers Lili (H13)");
    CHECK(front_dog > 60 * K * K, "in front of a bush, Lili is drawn over the grass");
    world_init(&s_world, 1); frame();
    uint16_t rugpx = px(rug->x + 6 * K, rug->y + 10 * K);
    int on_rug = 0; for (int sh = 0; sh < 8; sh++) on_rug |= rugpx == s_r8.lut[pal_ramp(RAMP_ID_RUG_GRAY, sh)] || rugpx == s_r8.lut[pal_ramp(RAMP_ID_RUG_GREEN, sh)];
    CHECK(on_rug, "the rug is drawn where the bed was");
    placeholder_mode(false);

    /* --- the event queue (spec 6.3: the one door into the world) --- */
    ev_queue_t q; ev_init(&q); ev_t e;
    CHECK(!ev_pop(&q, &e), "an empty queue pops nothing");
    ev_push(&q, EV_TAP, 10, 20); ev_push(&q, EV_WHISTLE, 0, 0);
    CHECK(ev_pop(&q, &e) && e.type == EV_TAP && e.a == 10 && e.b == 20, "first in, first out");
    CHECK(ev_pop(&q, &e) && e.type == EV_WHISTLE && !ev_pop(&q, &e), "and then it is empty again");
    for (int i = 0; i < EV_QUEUE_LEN + 8; i++) ev_push(&q, EV_TIME_SKIP, i, 0);          /* 8 more than it holds: wraps, drops the oldest */
    CHECK(q.dropped == 8 && q.count == EV_QUEUE_LEN, "a full queue drops the oldest 8");
    CHECK(ev_pop(&q, &e) && e.a == 8, "the survivors start at the 9th event");
    CHECK(!strcmp(ev_name(EV_SHAKE), "SHAKE") && !strcmp(ev_name(EV_APPROACH), "APPROACH") && !strcmp(ev_name(EV_SET_DAYPART), "SET_DAYPART") && (int)(sizeof s_world.events_seen / sizeof s_world.events_seen[0]) == EV_COUNT,
          "event names, and the world's counters cover every type");
    world_init(&s_world, 1);
    ev_t sv = { EV_SWITCH_VIEW, 0, 0, 0 }, ts = { EV_TIME_SKIP, 3600, 0, 0 }, ss = { EV_SET_SEASON, 3, 0, 0 }, dp = { EV_SET_DAYPART, 9, 0, 0 }, wh = { EV_WHISTLE, 0, 0, 0 };
    world_handle_event(&s_world, &sv); world_handle_event(&s_world, &ts); world_handle_event(&s_world, &ss); world_handle_event(&s_world, &dp); world_handle_event(&s_world, &wh);
    CHECK(s_world.view == VIEW_YARD && s_world.clock_s == 3600.0f && s_world.season_force == 3, "view, time skip and season events act on the world");
    CHECK(s_world.daypart_force == -1, "an out-of-range daypart means from-the-clock");
    CHECK(s_world.events_seen[EV_WHISTLE] == 1 && s_world.events_seen[EV_SWITCH_VIEW] == 1, "every event is counted");

    /* --- the pak (M3.3): a bad pack is refused, a sprite frame lands at the pivot and is clipped --- */
    {
        static uint8_t bad[32] = "LPAK";
        pak_t t; CHECK(!pak_open(&t, bad, sizeof bad) && !pak_open(&t, NULL, 0), "a bad or missing pack is refused (placeholder graphics then)");
        if (s_pak.data) {
            sprite_set_t set; sprite_frame_t fr;
            /* M3.5 (2026-10-27): A01-A09 in every direction, 8 frames, in the room's and the yard's size */
            static const uint16_t FIRST[] = { ANIM_IDLE_STAND, ANIM_IDLE_SIT_BLINK, ANIM_SIT_DOWN, ANIM_WALK, ANIM_TROT, ANIM_RUN, ANIM_TIPTOE, ANIM_SCURRY, ANIM_PANT_SMILE };
            for (int i = 0; i < (int)(sizeof FIRST / sizeof FIRST[0]); i++) for (int off = 0; off <= ANIM_YARD_OFFSET; off += ANIM_YARD_OFFSET) {
                sprite_set_t a;
                if (!pak_sprite_set(&s_pak, (uint16_t)(FIRST[i] + off), &a) || a.dir_mask != 0xFF || a.n_frames != 8) { printf("FAIL: animation %d: not 8 frames in all 8 directions\n", FIRST[i] + off); return 1; }
            }
            for (int i = 0; i < N_ANIMS; i++) {                          /* M3.6: every animation the sim lists is in the pack, in both sizes */
                sprite_set_t a; const uint16_t id = ANIM_REVERSE_OF(ANIMS[i].id) ? ANIM_REVERSE_OF(ANIMS[i].id) : ANIMS[i].id;
                if (!pak_sprite_set(&s_pak, id, &a) || !pak_sprite_set(&s_pak, (uint16_t)(id + ANIM_YARD_OFFSET), &a)) { printf("FAIL: %s (%d) is not in the pack in both sizes\n", ANIMS[i].name, id); return 1; }
            }
            for (int id = 1; id < ANIM_YARD_OFFSET * 2; id++) {          /* every set the pack has: a cropped cell that holds its pivot and fits the shadow's buffer */
                sprite_set_t a;
                if (!pak_sprite_set(&s_pak, (uint16_t)id, &a)) continue;
                if (a.cell_w > LILI_SPRITE_CELL_MAX || a.cell_h > LILI_SPRITE_CELL_MAX || a.pivot_x >= a.cell_w || a.pivot_y > a.cell_h) { printf("FAIL: sprite set %d: cell %dx%d, pivot (%d, %d)\n", id, a.cell_w, a.cell_h, a.pivot_x, a.pivot_y); return 1; }
            }
            CHECK(pak_sprite_set(&s_pak, ANIM_IDLE_STAND, &set) && pak_frame(&set, DIR_S, 0, &fr) && fr.rle_len > 0, "frame S/0 exists");
            {   /* the size the owner picked (2026-10-23): standing, 0.75 of the 79 px masters (tools/pixellab_norm.py REF) */
                static uint8_t cell[LILI_SPRITE_CELL_MAX * LILI_SPRITE_CELL_MAX]; int top = -1;
                pak_frame_decode(&set, &fr, cell);
                for (int y = 0; y < set.cell_h && top < 0; y++) for (int x = 0; x < set.cell_w; x++) if (cell[y * set.cell_w + x]) { top = y; break; }
                printf("standing in the room: %d px tall\n", set.pivot_y - top);
                CHECK(top >= 0 && set.pivot_y - top >= 56 && set.pivot_y - top <= 62, "standing she is 56-62 px tall in the room (0.75 x 79, as before the normalized frames)");
            }
            const int cx = 120 * K, cy = 140 * K;
            r8_clear(&s_r8, PAL_FLOOR + 5); pak_blit(&s_r8, &set, &fr, cx, cy);
            int painted = 0, out_of_cell = 0;
            for (int y = 0; y < LILI_LOG_H; y++) for (int x = 0; x < LILI_LOG_W; x++) if (s_r8.px[y * LILI_LOG_W + x] != PAL_FLOOR + 5) {
                painted++; if (x < cx - set.pivot_x || x >= cx - set.pivot_x + set.cell_w || y < cy - set.pivot_y || y >= cy - set.pivot_y + set.cell_h) out_of_cell++;
            }
            CHECK(painted > 200 * K * K && !out_of_cell, "the sprite paints inside its cell around the pivot");
            int paws = 0;                               /* the bottom rows, a paw's width either side of the pivot (a front view has a gap in the middle) */
            for (int dy = 1; dy <= 3 * K; dy++) for (int dx = -12; dx <= 12; dx++) paws |= s_r8.px[(cy - dy) * LILI_LOG_W + cx + dx] != PAL_FLOOR + 5;
            CHECK(paws, "the paws stand on the pivot row");
            r8_clear(&s_r8, 0); pak_blit(&s_r8, &set, &fr, 2, 2); pak_blit(&s_r8, &set, &fr, LILI_LOG_W - 2, LILI_LOG_H - 2);   /* clipped at both corners: no crash */
            CHECK(pak_frame(&set, DIR_NE, 15, &fr), "a frame index wraps");
            sprite_set_t one; CHECK(pak_sprite_set(&s_pak, ANIM_EAT_DRINK, &one) && one.dir_mask == (1 << DIR_W) && pak_frame(&one, DIR_E, 0, &fr), "a direction the set lacks falls back to the nearest it has");
        } else printf("note: no pak loaded, the sprite tests were skipped\n");
    }

    /* --- the static-layer cache (M2.6): the same picture as the plain render, on a miss and on a hit, in both views --- */
    {
        static render8_t ra, rb; static bgcache_t bc;
        memset(&bc, 0, sizeof bc); world_init(&s_world, 7);
        for (int v = 0; v < 3; v++) {
            world_render(&s_world, &ra); world_render_cached(&s_world, &rb, &bc);
            CHECK(!memcmp(ra.px, rb.px, sizeof ra.px), "cached render (a miss) = the plain render");
            s_world.actors[0].x += 17; world_render(&s_world, &ra); world_render_cached(&s_world, &rb, &bc);
            CHECK(!memcmp(ra.px, rb.px, sizeof ra.px), "cached render (a hit, Lili moved) = the plain render");
            world_switch_view(&s_world);
        }
        CHECK(world_bg_key(&s_world) != bc.key, "a view switch changes the key, so the next render redraws the static layer");
    }

    /* --- M3.5 (2026-10-27): standing up is sitting down backwards (ANIM_REVERSE_OF), in both views and sizes --- */
    if (s_pak.data) {
        static render8_t ra, rb; sprite_set_t sd, none; int frames = 0, same = 0;
        world_init(&s_world, 9); actor_t *d = &s_world.actors[0];
        CHECK(pak_sprite_set(&s_pak, ANIM_SIT_DOWN, &sd) && !sd.loop && !pak_sprite_set(&s_pak, ANIM_STAND_UP, &none),
              "sitting down plays once, standing up has no frames of its own");
        for (int v = 0; v < VIEW_COUNT; v++, world_switch_view(&s_world)) {
            d->view = s_world.view;
            for (int f = 0; f < sd.n_frames; f++, frames++) {          /* frame f of one = frame n-1-f of the other, a different direction each */
                d->dir = (dir8_t)(f % DIR_COUNT);
                d->anim = ANIM_SIT_DOWN; d->anim_t = (f + 0.5f) / sd.fps;                   world_render(&s_world, &ra);
                d->anim = ANIM_STAND_UP; d->anim_t = (sd.n_frames - 1 - f + 0.5f) / sd.fps; world_render(&s_world, &rb);
                same += !memcmp(ra.px, rb.px, sizeof ra.px);
            }
        }
        CHECK(same == frames, "every frame of standing up is a frame of sitting down, in the reverse order (room and yard)");
        s_world.view = d->view = VIEW_ROOM; d->dir = DIR_S;
        d->anim = ANIM_STAND_UP; d->anim_t = 30.0f; world_render(&s_world, &ra);
        d->anim = ANIM_SIT_DOWN; d->anim_t = 0;     world_render(&s_world, &rb);
        CHECK(!memcmp(ra.px, rb.px, sizeof ra.px), "standing up ends (and holds) where sitting down starts: no jump between the two");
        d->anim = ANIM_STAND_UP; d->anim_t = 0;     world_render(&s_world, &rb);
        CHECK(memcmp(ra.px, rb.px, sizeof ra.px), "standing up moves: its first and last pictures differ");
    }

    /* --- the painted world (2026-10-23): the owner's pictures, the door view, light and shadow --- */
    if (world_painted()) {
        static render8_t bg, sc;
        world_init(&s_world, 5); actor_t *d = &s_world.actors[0];
        const int pane_x = 211, pane_y = 50;                  /* the door's top-left pane (tools/bg_prepare.py PANE_X/Y x 1.2) */
        s_world.season_force = 0; world_render(&s_world, &bg); uint8_t spring = bg.px[pane_y * LILI_LOG_W + pane_x];
        s_world.season_force = 3; world_render(&s_world, &bg); uint8_t winter = bg.px[pane_y * LILI_LOG_W + pane_x];
        CHECK(spring && winter && spring != winter, "the door's glass shows the season's yard (spring and winter differ)");
        s_world.season_force = 2;
        int shadow[4] = { 0 };
        for (int dp = 1; dp <= 3; dp += 2) {                  /* the afternoon and the night */
            s_world.daypart_force = (int8_t)dp;
            d->x = 240; d->y = 300; d->anim = ANIM_IDLE_SIT_BLINK; d->dir = DIR_S; d->anim_t = 0;
            d->present = false; world_render(&s_world, &bg);
            d->present = true;  world_render(&s_world, &sc);
            sprite_set_t set; sprite_frame_t fr; static uint8_t cell[LILI_SPRITE_CELL_MAX * LILI_SPRITE_CELL_MAX];
            CHECK(pak_sprite_set(&s_pak, d->anim, &set) && pak_frame(&set, d->dir, 0, &fr), "the sitting frame exists");
            pak_frame_decode(&set, &fr, cell);
            for (int y = 0; y < LILI_LOG_H; y++) for (int x = 0; x < LILI_LOG_W; x++) {
                int cx = x - (240 - set.pivot_x), cy = y - (300 - set.pivot_y);
                bool body = cx >= 0 && cy >= 0 && cx < set.cell_w && cy < set.cell_h && cell[cy * set.cell_w + cx];
                if (!body && bg.px[y * LILI_LOG_W + x] != sc.px[y * LILI_LOG_W + x]) shadow[dp]++;
            }
        }
        printf("shadow pixels around her: afternoon %d, night %d\n", shadow[1], shadow[3]);
        CHECK(shadow[3] > 50 && shadow[1] > shadow[3] + 40, "Lili throws a shadow; by day the door's light adds her cast shadow (short, from the paws: the owner, 2026-10-23)");
        s_world.daypart_force = 1; world_render(&s_world, &bg); uint16_t day = bg.lut[150 + 50];
        s_world.daypart_force = 3; world_render(&s_world, &bg); uint16_t night = bg.lut[150 + 50];
        CHECK(((night >> 11) + ((night >> 5) & 63) / 2) < ((day >> 11) + ((day >> 5) & 63) / 2), "the night palette is darker than the afternoon's");
        pak_image_t bowl; CHECK(pak_image(&s_pak, IMG_BOWL_FOOD, &bowl), "the food bowl is a prop");
        const int bx = bowl.x + bowl.w / 2;
        s_world.daypart_force = 3; d->present = true; d->anim_t = 0;
        d->x = bx + 16; d->y = bowl.y + bowl.h + 1; d->anim = ANIM_EAT_DRINK; d->dir = DIR_W; world_render(&s_world, &sc);   /* the demo's spot (lili_demo.c) */
        {
            sprite_set_t set; sprite_frame_t fr; static uint8_t cell[LILI_SPRITE_CELL_MAX * LILI_SPRITE_CELL_MAX]; int seen = 0, hidden = 0;
            CHECK(pak_sprite_set(&s_pak, d->anim, &set) && pak_frame(&set, d->dir, 0, &fr), "the eating frame exists");
            pak_frame_decode(&set, &fr, cell);
            for (int y = bowl.y; y < bowl.y + bowl.h; y++) for (int x = bowl.x; x < bowl.x + bowl.w; x++) {
                int cx = x - ((int)d->x - set.pivot_x), cy = y - ((int)d->y - set.pivot_y);
                if (cx < 0 || cy < 0 || cx >= set.cell_w || cy >= set.cell_h || !cell[cy * set.cell_w + cx]) continue;
                if (sc.px[y * LILI_LOG_W + x] == cell[cy * set.cell_w + cx]) seen++; else hidden++;
            }
            CHECK(seen > 80 && !hidden, "eating, her head is in the bowl her frames carry: the room's bowl steps aside, nothing covers her (the owner, 2026-10-23)");
        }
        pak_image_t water; CHECK(pak_image(&s_pak, IMG_BOWL_WATER, &water) && water.x > bx + 16 + 27, "the water bowl is past her tail while she eats");
        d->x = 300; d->y = 230; world_tick(&s_world, 0.01f); CHECK(d->vis > 0.99f, "out of the bushes she is fully seen");
        s_world.view = VIEW_YARD; d->view = VIEW_YARD;
        const map_t *ym = &MAPS[VIEW_YARD]; const map_zone_t *hz = NULL;
        for (int i = 0; i < ym->n_zones; i++) if (!strncmp(ym->zones[i].name, "hide", 4) && ym->zones[i].kind == 0) hz = &ym->zones[i];
        CHECK(hz, "the yard has a hiding zone");
        d->x = hz->a + hz->c / 2; d->y = hz->b + hz->d / 2;
        for (int i = 0; i < 30; i++) world_tick(&s_world, 1.0f / 30.0f);
        CHECK(d->vis < 0.02f, "deep in a bush she fades out within a second");
        s_world.view = VIEW_ROOM; d->view = VIEW_ROOM; d->vis = 1.0f;
    } else printf("note: no painted backgrounds in the pak, those tests were skipped\n");

    /* --- the world (placeholder shapes) --- */
    placeholder_mode(true);
    world_init(&s_world, 1234);
    for (int i = 0; i < 600; i++) world_tick(&s_world, 1.0f / 60.0f);
    CHECK(s_world.clock_s > 9.9f && s_world.clock_s < 10.1f, "10 s of ticks");
    s_world.actors[0].anim = 0;                         /* the capsule */
    frame();
    const actor_t *a = &s_world.actors[0];
    CHECK(px((int)a->x, (int)a->y - 6) != px(120, 220), "the placeholder dog is drawn in the room");
    world_switch_view(&s_world); frame();
    CHECK(px((int)a->x, (int)a->y - 6) == s_r8.lut[pal_ramp(RAMP_ID_GRASS, 5)], "the dog stays in the room when the yard is shown (bare grass there)");
    CHECK(s_world.view == VIEW_YARD, "Tab goes to the yard");
    world_switch_view(&s_world);
    CHECK(s_world.view == VIEW_ROOM, "and back to the room");
    placeholder_mode(false);
    world_t a1, a2; world_init(&a1, 7); world_init(&a2, 7);
    for (int i = 0; i < 1000; i++) CHECK(world_rand(&a1) == world_rand(&a2), "seeded PRNG is deterministic");
    world_init(&a1, 0); CHECK(world_rand(&a1) != 0, "seed 0 does not stall the PRNG");
    printf("selftest: ok (world %zu B, canvas %zu B vs %zu B as RGB565, %d actor(s), logical %dx%d, corner %d px)\n",
           sizeof(world_t), sizeof s_r8.px, sizeof s_fb, MAX_ACTORS, LILI_LOG_W, LILI_LOG_H, LILI_CORNER_R_PHYS);
    return 0;
}

/* ---------- the demo tour: common/lili/lili_demo.c (the panel runs the same script) ---------- */
#include "lili_demo.h"
#include "nav.h"
#include "gesture.h"
#include "plan.h"
#include "toys.h"
static nav_scratch_t s_nav;                           /* the path search's work area (nav.h) */
/* --demo-record DIR [fps]: one round of the tour, headless, as DIR/f00000.ppm ... (tools: ffmpeg -> mp4) */
static int demo_record(const char *dir, int fps) {
    canvas_init(); world_init(&s_world, 42); nav_init(&s_nav); demo_start();
    const float dt = 1.0f / (float)fps;
    char path[512]; int n = 0;
    snprintf(path, sizeof path, "%s/log.txt", dir); FILE *lg = fopen(path, "w");   /* frame, step, x, y, anim, vis: to find a moment */
    while (!demo_rounds() && n < 400 * fps) {                    /* until the tour comes round to its start */
        demo_tick(&s_world, dt); world_tick(&s_world, dt); frame();
        const actor_t *l = &s_world.actors[0];
        if (lg) fprintf(lg, "%d %d %.0f %.0f %u %.2f %d\n", n, demo_step(), l->x, l->y, l->anim, l->vis, s_world.view);
        snprintf(path, sizeof path, "%s/f%05d.ppm", dir, n++); write_ppm(path, s_fb);
    }
    if (lg) fclose(lg);
    printf("demo: %d frames at %d fps (%.1f s) -> %s\n", n, fps, n / (float)fps, dir);
    return 0;
}

/* --snapshot DIR (spec 6.3, M3.7 [G]): the static scenes for approval: both views x 4 seasons x 4 dayparts, Lili
   sitting on the room's centre spot / the yard's grass, DIR/<view>_<season>_<daypart>.ppm (tools/snapshot_review.py) */
static int snapshot(const char *dir) {
    static const char *V[VIEW_COUNT] = { "room", "yard" }, *S[4] = { "spring", "summer", "autumn", "winter" },
                      *D[4] = { "morning", "afternoon", "evening", "night" };
    canvas_init(); world_init(&s_world, 3);
    actor_t *l = &s_world.actors[0]; char path[512]; int n = 0;
    for (int v = 0; v < VIEW_COUNT; v++) {
        const map_point_t *p = &world_map(&s_world, (view_t)v)->points[v == VIEW_ROOM ? SPOT_CENTER : SPOT_GRASS];
        s_world.view = l->view = (view_t)v; l->anim = ANIM_IDLE_SIT_BLINK; l->dir = DIR_S; l->anim_t = 0;
        if (p->z >= 0) { l->x = p->x; l->y = p->y; }
        for (int s = 0; s < 4; s++) for (int d = 0; d < 4; d++) {
            s_world.season_force = (int8_t)s; s_world.daypart_force = (int8_t)d;
            frame(); snprintf(path, sizeof path, "%s/%s_%s_%s.ppm", dir, V[v], S[s], D[d]); write_ppm(path, s_fb); n++;
        }
    }
    printf("snapshot: %d scenes -> %s\n", n, dir);
    return 0;
}

/* --selftest-nav (spec 6.4, M4.1): every point of both maps is reachable from the door and back, no path goes
   through a wall or a thing, the steering arrives on the spot without flickering between two directions, the
   gaits keep their order, an unreachable goal gives the nearest reachable place */
static int selftest_nav(void) {
    nav_init(&s_nav); world_init(&s_world, 11);
    actor_t *a = &s_world.actors[0]; body_t *b = &s_world.bodies[0];
    int legs = 0, worst_turns = 0; float worst_s = 0;
    for (int v = 0; v < VIEW_COUNT; v++) {
        const map_t *m = &MAPS[v];
        const map_point_t *door = &m->points[v == VIEW_ROOM ? SPOT_DOOR : SPOT_DOOR_YARD];
        CHECK(door->z >= 0 && nav_clear((view_t)v, door->x, door->y, 0), "each view has its door spot on the floor");
        for (int s = 0; s < SPOT_COUNT; s++) {
            const map_point_t *p = &m->points[s];
            if (p->z < 0 || s == SPOT_DOOR || s == SPOT_DOOR_YARD) continue;
            for (int back = 0; back < 2; back++) {               /* the door to the spot, then the spot to the door */
                const map_point_t *from = back ? p : door, *to = back ? door : p;
                memset(b, 0, sizeof *b); b->style = STYLE_TROT;
                if (!nav_plan((view_t)v, from->x, from->y, to->x, to->y, b) || !b->n) {
                    printf("FAIL: %s: %s -> %s has no path\n", m->name, SPOT_NAMES[back ? s : SPOT_DOOR], SPOT_NAMES[back ? SPOT_DOOR : s]); return 1;
                }
                float px = from->x, py = from->y;
                for (int i = 0; i < b->n; i++) {                 /* every leg on the floor: no wall, no bench, no bush trunk */
                    if (!nav_line_clear((view_t)v, px, py, b->path[i][0], b->path[i][1], 0)) { printf("FAIL: %s: the path to %s crosses a blocked cell at leg %d\n", m->name, SPOT_NAMES[s], i); return 1; }
                    px = b->path[i][0]; py = b->path[i][1];
                }
                CHECK(b->path[b->n - 1][0] == to->x && b->path[b->n - 1][1] == to->y, "the path ends on the spot itself");
                a->view = (view_t)v; a->x = from->x; a->y = from->y; a->dir = DIR_S;
                int turns = 0; float t = 0; dir8_t last = a->dir;
                while (!nav_follow(a, b, 1.0f / 30.0f) && t < 30.0f) {
                    t += 1.0f / 30.0f;
                    if (!map_walkable(m, (int)a->x, (int)a->y)) { printf("FAIL: %s: she stepped off the floor at (%.0f, %.0f)\n", m->name, a->x, a->y); return 1; }
                    if (a->dir != last) { turns++; last = a->dir; }
                }
                CHECK(t < 30.0f && a->x == to->x && a->y == to->y, "she arrives on the spot within 30 s");
                CHECK(a->anim == ANIM_TROT, "the gait's animation plays on the way");
                if (turns > worst_turns) worst_turns = turns;
                if (t > worst_s) worst_s = t;
                legs++;
            }
        }
    }
    printf("nav: %d legs between the doors and every spot, the longest %.1f s, at most %d turns\n", legs, worst_s, worst_turns);
    CHECK(worst_turns <= 12, "no flicker: a few turns per leg, not one per frame");
    for (int st = 1; st < STYLE_COUNT; st++) CHECK(nav_speed((nav_style_t)st, VIEW_ROOM) > nav_speed((nav_style_t)(st - 1), VIEW_ROOM), "the gaits get faster in spec 8.4's order");
    CHECK(nav_speed(STYLE_WALK, VIEW_YARD) < nav_speed(STYLE_WALK, VIEW_ROOM), "the same gait covers fewer px in the smaller yard");
    CHECK(nav_anim(STYLE_SPRINT) == ANIM_RUN && nav_anim(STYLE_TIPTOE) == ANIM_TIPTOE && nav_anim(STYLE_SCURRY) == ANIM_SCURRY, "each gait has its animation");
    CHECK(nav_dir(0, 10) == DIR_S && nav_dir(-10, 0) == DIR_W && nav_dir(10, -10) == DIR_NE, "a screen vector's direction");
    CHECK(nav_dir_hyst(DIR_S, 5.5f, 10) == DIR_S && nav_dir(5.5f, 10) == DIR_SE && nav_dir_hyst(DIR_S, 10, 10) == DIR_SE, "the facing keeps until the vector is clearly past the sector (29 degrees: SE without hysteresis, still S with it; 45 turns SE)");
    memset(b, 0, sizeof *b);                                     /* a goal in a wall: false, and the path ends at the nearest floor */
    CHECK(!nav_plan(VIEW_ROOM, 240, 300, 5, 5, b) && b->n > 0 && nav_clear(VIEW_ROOM, b->path[b->n - 1][0], b->path[b->n - 1][1], 0), "an unreachable goal: the nearest reachable place");
    printf("selftest-nav: ok\n");
    return 0;
}

/* --selftest-render [--update] (spec 6.4, M4.2): the glass's corners, the daypart's palette, the Y-sort with the
   yard's covers, the bush that shakes once as she goes in, and the reference images: the 32 --snapshot scenes'
   hashes against assets/ref/render_hashes.txt (a changed picture fails until a person has looked at it and
   rerun this with --update) */
static uint32_t fb_hash(void) { uint32_t h = 2166136261u; for (int i = 0; i < LILI_PHYS_W * LILI_PHYS_H; i++) { h ^= s_fb[i]; h *= 16777619u; } return h; }
static int selftest_render(bool update) {
    if (!world_painted()) { printf("note: no painted world in the pak, selftest-render skipped\n"); return 0; }
    canvas_init(); nav_init(&s_nav); world_init(&s_world, 21);
    actor_t *l = &s_world.actors[0];
    s_world.season_force = 1; s_world.daypart_force = 1; frame();
    CHECK(s_fb[0] == 0 && s_fb[LILI_PHYS_W - 1] == 0 && s_fb[(LILI_PHYS_H - 1) * LILI_PHYS_W] == 0 && s_fb[LILI_PHYS_W * LILI_PHYS_H - 1] == 0, "the four corners are under the glass's black frame");
    long lum[2] = { 0, 0 };
    for (int k = 0; k < 2; k++) {                                /* the afternoon, then the night: the same room, darker */
        s_world.daypart_force = (int8_t)(k ? 3 : 1); frame();
        for (int i = 0; i < LILI_PHYS_W * LILI_PHYS_H; i += 7) lum[k] += (s_fb[i] >> 11) + ((s_fb[i] >> 5) & 63) / 2 + (s_fb[i] & 31);
    }
    CHECK(lum[1] * 10 < lum[0] * 7, "the night's palette is at least 30 % darker than the afternoon's");
    /* the Y-sort: the yard's widest cover (the bench), Lili just behind its front edge and just in front of it */
    s_world.view = l->view = VIEW_YARD; s_world.daypart_force = 1; l->anim = ANIM_IDLE_STAND; l->dir = DIR_S; l->vis = 1;
    pak_image_t cv, best = { 0 }; int wide = -1;
    for (int k = 0; k < IMG_COVERS_PER_SEASON; k++) if (pak_image(&s_pak, IMG_COVER(world_season(&s_world), k), &cv) && cv.w > best.w) { best = cv; wide = k; }
    CHECK(wide >= 0 && best.sort_y > 0, "the yard has covers with a sort line");
    static render8_t without, with;
    int seen[2] = { 0, 0 };
    for (int front = 0; front < 2; front++) {
        l->x = best.x + best.w / 2; l->y = front ? best.sort_y + 3 : best.sort_y - 3;
        l->present = false; world_render(&s_world, &without);
        l->present = true;  world_render(&s_world, &with);
        for (int i = 0; i < LILI_LOG_W * LILI_LOG_H; i++) seen[front] += without.px[i] != with.px[i];
    }
    printf("render: Lili behind the widest yard cover %d px seen, in front of it %d px\n", seen[0], seen[1]);
    CHECK(seen[1] > 1000 && seen[0] < seen[1] * 3 / 4, "behind a cover's sort line she is partly hidden, in front of it she is all seen");
    /* the bush: still until she goes into it, then a short shake, then still again (the owner, 2026-10-27) */
    const map_t *ym = &MAPS[VIEW_YARD]; const map_zone_t *hz = NULL;
    for (int i = 0; i < ym->n_zones; i++) if (!strncmp(ym->zones[i].name, "hide", 4)) { hz = &ym->zones[i]; break; }
    CHECK(hz, "the yard has a hiding zone");
    l->x = hz->a + hz->c / 2; l->y = hz->b - 6; world_tick(&s_world, 0.02f);
    CHECK(s_world.shake_cover < 0, "outside the bush nothing shakes");
    l->present = false; world_render(&s_world, &without);        /* the still bush, without her in the picture */
    l->present = true; l->y = hz->b + hz->d / 2; world_tick(&s_world, 0.02f);
    CHECK(s_world.shake_cover >= 0, "stepping into the hiding zone starts the bush's shake");
    l->present = false;
    world_render(&s_world, &with); int moved = 0;
    for (int i = 0; i < LILI_LOG_W * LILI_LOG_H; i++) moved += without.px[i] != with.px[i];
    for (int i = 0; i < 30; i++) world_tick(&s_world, 1.0f / 30.0f);
    static render8_t later; world_render(&s_world, &later); int after = 0;
    for (int i = 0; i < LILI_LOG_W * LILI_LOG_H; i++) after += without.px[i] != later.px[i];
    printf("render: the bush's shake moves %d px, a second later %d px differ from the still bush\n", moved, after);
    CHECK(moved > 50 && after == 0, "the bush shakes once, then stands still again");
    l->present = true;
    /* the reference images */
    static const char *REF = "../assets/ref/render_hashes.txt";
    static const char *V[VIEW_COUNT] = { "room", "yard" };
    char lines[64][48]; int n = 0;
    world_init(&s_world, 3);
    for (int v = 0; v < VIEW_COUNT; v++) {
        const map_point_t *p = &MAPS[v].points[v == VIEW_ROOM ? SPOT_CENTER : SPOT_GRASS];
        s_world.view = l->view = (view_t)v; l->anim = ANIM_IDLE_SIT_BLINK; l->dir = DIR_S; l->anim_t = 0; l->x = p->x; l->y = p->y;
        for (int s = 0; s < 4; s++) for (int d = 0; d < 4; d++) {
            s_world.season_force = (int8_t)s; s_world.daypart_force = (int8_t)d; frame();
            snprintf(lines[n++], sizeof lines[0], "%s_%d_%d %08x", V[v], s, d, fb_hash());
        }
    }
    FILE *f = update ? NULL : fopen(REF, "r");
    if (!f) {
        f = fopen(REF, "w"); if (!f) { printf("FAIL: cannot write %s\n", REF); return 1; }
        for (int i = 0; i < n; i++) fprintf(f, "%s\n", lines[i]);
        fclose(f); printf("render: %d reference hashes written to %s\n", n, REF);
    } else {
        char got[64]; int bad = 0, i = 0;
        while (i < n && fgets(got, sizeof got, f)) { got[strcspn(got, "\r\n")] = 0; if (strcmp(got, lines[i])) { printf("render: %s differs (reference %s)\n", lines[i], got); bad++; } i++; }
        fclose(f);
        if (bad || i != n) { printf("FAIL: %d of %d scenes differ from the reference images: look at ./lilisim --snapshot, then --selftest-render --update\n", bad + (n - i), n); return 1; }
        printf("render: %d scenes match the reference images\n", n);
    }
    printf("selftest-render: ok\n");
    return 0;
}

/* --selftest-gesture (spec 5.3, M4.3): synthetic strokes through gesture.c, each giving its event */
static int g_run(gesture_t *g, const gesture_ctx_t *c, ev_queue_t *q, const int (*pts)[3], int n) {   /* pts: x, y, t; the last one lifts */
    for (int i = 0; i < n; i++) gesture_feed(g, c, i < n - 1, pts[i][0], pts[i][1], (uint32_t)pts[i][2], q);
    return q->count;
}
static bool g_has(ev_queue_t *q, ev_type_t type, ev_t *out) {
    for (int i = 0; i < q->count; i++) { const ev_t *e = &q->q[(q->head + i) % EV_QUEUE_LEN]; if (e->type == type) { if (out) *out = *e; return true; } }
    return false;
}
static int g_count(ev_queue_t *q, ev_type_t type) {
    int n = 0; for (int i = 0; i < q->count; i++) n += q->q[(q->head + i) % EV_QUEUE_LEN].type == type; return n;
}
static int selftest_gesture(void) {
    gesture_t g; gesture_ctx_t c; ev_queue_t q; ev_t e;
    memset(&c, 0, sizeof c);
    c.lili = (g_rect_t){ 200, 200, 260, 260 }; c.door = (g_rect_t){ 196, 30, 278, 166 };
    c.n_toys = 1; c.toy_x[0] = 360; c.toy_y[0] = 360;
    gesture_init(&g);
    { static const int P[][3] = { { 100, 300, 0 }, { 102, 301, 60 }, { 102, 301, 120 } }; ev_init(&q); g_run(&g, &c, &q, P, 3); }
    CHECK(q.count == 1 && g_has(&q, EV_TAP, &e) && e.a == 100 && e.b == 300, "a short still touch is a TAP where it landed");
    { static const int P[][3] = { { 230, 100, 0 }, { 230, 100, 100 } }; ev_init(&q); g_run(&g, &c, &q, P, 2); }
    CHECK(q.count == 1 && g_has(&q, EV_SWITCH_VIEW, NULL), "a tap on the door switches the view");
    { static const int P[][3] = { { 100, 300, 0 }, { 103, 302, 300 }, { 104, 302, 650 }, { 104, 302, 900 }, { 104, 302, 950 } }; ev_init(&q); g_run(&g, &c, &q, P, 5); }
    CHECK(q.count == 1 && g_has(&q, EV_LONG, &e) && e.a == 100, "a long still press is one LONG, and no TAP at its release");
    {   /* the rub: back and forth over her, 25 px every 80 ms */
        static int P[20][3]; for (int i = 0; i < 20; i++) { P[i][0] = 220 + (i % 2) * 25; P[i][1] = 230; P[i][2] = i * 80; }
        ev_init(&q); g_run(&g, &c, &q, (const int (*)[3])P, 20);
    }
    CHECK(g_count(&q, EV_PET_START) == 1 && g_count(&q, EV_PET_TICK) >= 2 && g_count(&q, EV_PET_END) == 1 && !g_has(&q, EV_TAP, NULL), "a rub over her: PET_START, PET_TICKs, PET_END at the lift");
    {   /* the same back and forth away from her: nothing */
        static int P[20][3]; for (int i = 0; i < 20; i++) { P[i][0] = 60 + (i % 2) * 25; P[i][1] = 400; P[i][2] = i * 80; }
        ev_init(&q); g_run(&g, &c, &q, (const int (*)[3])P, 20);
    }
    CHECK(q.count == 0, "rubbing the floor is no pet (and a moved press no tap)");
    {   /* the flick: from the toy, 30 px every 16 ms (1875 px / s) */
        static int P[7][3]; for (int i = 0; i < 7; i++) { P[i][0] = 360 + i * 30; P[i][1] = 360 - i * 10; P[i][2] = i * 16; }
        P[6][0] = P[5][0]; P[6][1] = P[5][1]; P[6][2] = P[5][2];
        ev_init(&q); g_run(&g, &c, &q, (const int (*)[3])P, 7);
    }
    CHECK(g_has(&q, EV_FLICK, &e) && e.a > 600 && e.b < 0 && e.c == 0 && !g_has(&q, EV_DROP, NULL), "a fast stroke from the toy is a FLICK with its velocity and the toy");
    {   /* the drag: from the toy, 2 px every 16 ms */
        static int P[30][3]; for (int i = 0; i < 30; i++) { P[i][0] = 360 - i * 2; P[i][1] = 360; P[i][2] = i * 16; }
        ev_init(&q); g_run(&g, &c, &q, (const int (*)[3])P, 30);
    }
    CHECK(g_count(&q, EV_DRAG) > 5 && g_has(&q, EV_DROP, &e) && e.c == 0 && !g_has(&q, EV_FLICK, NULL), "a slow stroke from the toy drags it, and drops it at the lift");
    {   /* the approach: she bows, the finger comes nearer at ~90 px / s */
        c.bowing = true;
        static int P[40][3]; for (int i = 0; i < 40; i++) { P[i][0] = 230; P[i][1] = 440 - i * 3; P[i][2] = i * 33; }
        ev_init(&q); g_run(&g, &c, &q, (const int (*)[3])P, 40);
        c.bowing = false;
    }
    CHECK(g_count(&q, EV_APPROACH) >= 3 && g_has(&q, EV_APPROACH, &e) && e.a < 240, "nearing her slowly while she bows: APPROACH with the distance");
    {   /* the edge swipe: from the bottom edge, up 140 px */
        static int P[12][3]; for (int i = 0; i < 12; i++) { P[i][0] = 240; P[i][1] = 472 - i * 13; P[i][2] = i * 20; }
        ev_init(&q); g_run(&g, &c, &q, (const int (*)[3])P, 12);
    }
    CHECK(g_count(&q, EV_SWITCH_VIEW) == 1 && q.count == 1, "a swipe up from the bottom edge switches the view, once");
    printf("selftest-gesture: ok\n");
    return 0;
}

/* --selftest-plan (spec 8.7, M4.4): every action of the brain's vocabulary, set up where its precondition
   holds, runs to its end (not failed, within its time limits) with her on the floor all the way; an impossible
   one is skipped and a plan of only impossible ones fails at once; the toys fly, bounce and stop inside the walls */
static void plan_setup(int act) {
    world_init(&s_world, 100 + act); actor_t *l = &s_world.actors[0];
    s_world.season_force = 2; s_world.daypart_force = 1;          /* autumn afternoon: the sun is on the floor */
    const bool yard = act == ACT_ENTER_ROOM || act == ACT_GO_BUSH || act == ACT_GO_TREE || act == ACT_HIDE_BUSH || act == ACT_PEEK_BUSH ||
                      act == ACT_CHASE_LEAF || act == ACT_CHASE_BUTTERFLY || act == ACT_PLAY_SNOW || act == ACT_SNIFF_FLOWERS;
    if (act == ACT_CHASE_BUTTERFLY || act == ACT_SNIFF_FLOWERS) s_world.season_force = 1;
    if (act == ACT_PLAY_SNOW) s_world.season_force = 3;
    if (yard) { const map_point_t *p = &MAPS[VIEW_YARD].points[SPOT_GRASS]; s_world.view = l->view = VIEW_YARD; l->x = p->x; l->y = p->y; }
    if (act == ACT_BRING_TOY || act == ACT_DROP_TOY_FRONT || act == ACT_RUN_WITH_TOY || act == ACT_TOSS_TOY) toy_pick(&s_world, TOY_ROPE, 0);
    if (act == ACT_PEEK_BENCH) s_world.plans[0].hide = 1;
    if (act == ACT_LEAVE_RUG) { const map_point_t *p = &MAPS[VIEW_ROOM].points[SPOT_RUG]; l->x = p->x; l->y = p->y; }
    if (act == ACT_FOLLOW_FINGER) world_finger(&s_world, true, 300, 380);
}
static int selftest_plan(void) {
    nav_init(&s_nav); float longest = 0; int longest_act = 0;
    for (int act = 0; act < ACT_COUNT; act++) {
        if (act == ACT_NONE) continue;
        plan_setup(act);
        if (!plan_possible(&s_world, 0, act)) { printf("FAIL: %s is not possible in its own setup\n", ACTION_NAME(act)); return 1; }
        plan_one(&s_world, 0, act, ST_TROT);
        const actor_t *l = &s_world.actors[0]; float t = 0; int st = PLAN_RUNNING;
        while ((st = plan_tick(&s_world, 0, 1.0f / 30.0f)) == PLAN_RUNNING && t < 3 * PLAN_STEP_LIMIT_S) {
            world_tick(&s_world, 1.0f / 30.0f); t += 1.0f / 30.0f;
            if (!map_walkable(&MAPS[l->view], (int)l->x, (int)l->y)) { printf("FAIL: %s: she is off the floor at (%.0f, %.0f) in the %s\n", ACTION_NAME(act), l->x, l->y, MAPS[l->view].name); return 1; }
        }
        if (st != PLAN_DONE) { printf("FAIL: %s ended %s after %.1f s\n", ACTION_NAME(act), st == PLAN_FAILED ? "failed" : "still running", t); return 1; }
        if (t > longest) { longest = t; longest_act = act; }
        if (act == ACT_ENTER_YARD) CHECK(l->view == VIEW_YARD, "enter_yard: she is in the yard");
        if (act == ACT_ENTER_ROOM) CHECK(l->view == VIEW_ROOM, "enter_room: she is in the room");
        if (act == ACT_PICKUP_ROPE) CHECK(toy_held(&s_world, 0) == TOY_ROPE, "pickup_rope: the rope is in her mouth");
        if (act == ACT_DROP_TOY_FRONT) CHECK(toy_held(&s_world, 0) < 0, "drop_toy_front: her mouth is empty");
        if (act == ACT_EAT) CHECK(s_world.things.food == 2, "eat: one serving less");
        if (act == ACT_USE_PAD) CHECK(s_world.things.pad == 1, "use_pad: the pad is used");
        if (act == ACT_MISCHIEF_FLOOR) CHECK(s_world.things.n_mess == 1 && s_world.things.guilty, "mischief_floor: a spot, and she is guilty");
        if (act == ACT_HIDE_BENCH) CHECK(s_world.plans[0].hide == 1, "hide_bench: she is under the bench");
    }
    printf("plan: all %d actions ran to their end, the longest %s in %.1f s\n", ACT_COUNT - 1, ACTION_NAME(longest_act), longest);
    world_init(&s_world, 5); s_world.daypart_force = 3;                  /* at night, in the room: none of these can run */
    decision_t d = { { ACT_GO_TREE, ACT_NAP_SUNBEAM, ACT_DROP_TOY_FRONT }, ST_WALK, FC_NEUTRAL, BB_NONE, 5, 1 };
    plan_start(&s_world, 0, &d);
    CHECK(plan_tick(&s_world, 0, 0.03f) == PLAN_FAILED, "a plan of three impossible actions fails at once (a new question)");
    d.a[1] = ACT_SIT; plan_start(&s_world, 0, &d);
    int st = PLAN_RUNNING; for (int i = 0; i < 300 && st == PLAN_RUNNING; i++) { st = plan_tick(&s_world, 0, 0.03f); world_tick(&s_world, 0.03f); }
    CHECK(st == PLAN_DONE && (s_world.plans[0].skipped & 1) && (s_world.plans[0].skipped & 4), "the impossible ones are skipped, the possible one runs");
    /* the toys (spec 8.9): a throw flies, bounces, slides and stops on the floor of its own view */
    world_init(&s_world, 6); toy_t *ty = &s_world.things.toys[TOY_PLUSH];
    ty->x = 240; ty->y = 300; toy_throw(&s_world, TOY_PLUSH, 3000, -400);
    CHECK(ty->vz > 0 && ty->vx > 0 && ty->vx <= 220.0f * LILI_K + 1, "a flick's speed is scaled and capped");
    float maxz = 0; for (int i = 0; i < 300; i++) { toys_tick(&s_world, 1.0f / 30.0f); if (ty->z > maxz) maxz = ty->z; CHECK(nav_clear(VIEW_ROOM, (int)ty->x, (int)ty->y, 0), "the toy stays on the room's floor"); }
    printf("toy: highest %.1f px, at rest at (%.0f, %.0f), z %.1f, v (%.2f, %.2f, %.2f)\n", maxz, ty->x, ty->y, ty->z, ty->vx, ty->vy, ty->vz);
    CHECK(maxz > 6 && ty->z == 0 && ty->vx == 0 && ty->vy == 0, "it flies up, comes down and stops");
    printf("selftest-plan: ok\n");
    return 0;
}

/* --selftest-reflex (spec 8.8, M4.5) and the care taps (8.10, M4.7): each reflex fires from its trigger, runs
   to its end with its effect, the priorities hold, and the sleeping, hidden or guilty dog answers only as she should */
static int run_plan(float limit) {
    int st = PLAN_RUNNING; float t = 0;
    while ((st = plan_tick(&s_world, 0, 1.0f / 30.0f)) == PLAN_RUNNING && t < limit) { world_tick(&s_world, 1.0f / 30.0f); t += 1.0f / 30.0f; }
    return st;
}
static void ev(ev_type_t type, int a, int b, int c) { ev_t e = { (uint8_t)type, (int16_t)a, (int16_t)b, (int16_t)c }; world_handle_event(&s_world, &e); }
static void sleep_now(void) { s_world.actors[0].anim = ANIM_SLEEP_CURL; s_world.actors[0].anim_t = 5; s_world.plans[0].status = PLAN_DONE; }
static int selftest_reflex(void) {
    nav_init(&s_nav); actor_t *l = &s_world.actors[0]; plan_t *p = &s_world.plans[0]; char d[64];
    const map_point_t *front = &MAPS[VIEW_ROOM].points[SPOT_FRONT];
    /* R2: a whistle brings her to the front; from the yard through the door; not when guilty */
    world_init(&s_world, 31); ev(EV_WHISTLE, 0, 0, 0);
    CHECK(p->status == PLAN_RUNNING && p->prio == 2, "R2: a whistle starts the call reflex");
    CHECK(run_plan(30) == PLAN_DONE && fabsf(l->x - front->x) < 2 && fabsf(l->y - front->y) < 2, "R2: she ends at the front");
    world_init(&s_world, 32); { const map_point_t *g = &MAPS[VIEW_YARD].points[SPOT_GRASS]; l->view = VIEW_YARD; l->x = g->x; l->y = g->y; }
    ev(EV_CALL_NAME, 0, 0, 0);
    CHECK(run_plan(40) == PLAN_DONE && l->view == VIEW_ROOM && s_world.view == VIEW_ROOM, "R2: called from the yard she comes in through the door; the camera stays");
    world_init(&s_world, 33); s_world.things.guilty = true; ev(EV_WHISTLE, 0, 0, 0);
    CHECK(p->status != PLAN_RUNNING, "R2: a guilty dog does not come");
    /* R3: a pet: she stops and enjoys it while it lasts, 2 s more after it */
    world_init(&s_world, 34); ev(EV_PET_START, (int)l->x, (int)l->y - 20, 0);
    CHECK(p->prio == 3 && run_plan(1.0f) == PLAN_RUNNING && l->anim == ANIM_PET_ENJOY, "R3: petted, she enjoys it");
    ev(EV_PET_END, 0, 0, 0); { int st = run_plan(5); CHECK(st == PLAN_DONE, "R3: and stops 2 s after the hand"); }
    /* R4: a flicked toy: she runs after it and brings it to the front */
    world_init(&s_world, 35); { toy_t *ty = &s_world.things.toys[TOY_PLUSH]; ty->x = 300; ty->y = 300; }
    ev(EV_FLICK, 2000, 600, TOY_PLUSH);
    CHECK(p->prio == 4, "R4: a flick starts the fetch");
    CHECK(run_plan(40) == PLAN_DONE && toy_held(&s_world, 0) < 0, "R4: the fetch ends with the toy dropped");
    { const toy_t *ty = &s_world.things.toys[TOY_PLUSH]; printf("R4: toy at (%.0f, %.0f) view %d held %d, Lili (%.0f, %.0f), front (%d, %d)\n", ty->x, ty->y, ty->view, ty->held_by, l->x, l->y, front->x, front->y); CHECK(fabsf(ty->x - front->x) < 40 && fabsf(ty->y - front->y) < 40, "R4: the toy is at the front"); }
    /* R5: bowing by a toy, the finger near: she grabs it and dashes off; after 4 rounds she is tired */
    world_init(&s_world, 36); toy_pick(&s_world, TOY_ROPE, 0);
    for (int round = 0; round < 5; round++) {
        l->anim = ANIM_PLAY_BOW; p->status = PLAN_DONE;
        ev(EV_APPROACH, 40, 0, 0);
        plan_describe(&s_world, 0, d, sizeof d);
        CHECK(p->prio == 5 && strstr(d, round < 4 ? "R5 tease" : "R5 tired"), round < 4 ? "R5: a tease round" : "R5: the fifth time she is tired");
        if (round < 4) run_plan(30);
    }
    /* R6: the owner back: a toy to chew in front; guilty: under the bench, peeking */
    world_init(&s_world, 37); CHECK(reflex_greet(&s_world) && run_plan(45) == PLAN_DONE && l->anim == ANIM_CHEW_TOY, "R6: she greets with a toy, chewing it in front");
    world_init(&s_world, 38); s_world.things.guilty = true; reflex_greet(&s_world);
    CHECK(run_plan(30) == PLAN_DONE && p->hide == 1, "R6: guilty, she greets from under the bench");
    /* R7: a shake wakes her: startled, one bark; R8: a voice: a head tilt, but not in her sleep */
    world_init(&s_world, 39); sleep_now(); ev(EV_SHAKE, 0, 0, 0);
    CHECK(p->prio == 7 && run_plan(10) == PLAN_DONE, "R7: a shake wakes her (startle, bark)");
    world_init(&s_world, 40); sleep_now(); ev(EV_VOICE_ACTIVITY, 0, 0, 0);
    CHECK(p->status != PLAN_RUNNING, "R8: a voice does not wake her");
    s_world.actors[0].anim = ANIM_IDLE_STAND; ev(EV_VOICE_ACTIVITY, 0, 0, 0);
    CHECK(p->prio == 8 && run_plan(5) == PLAN_DONE && l->anim == ANIM_IDLE_STAND, "R8: awake, she tilts her head");
    /* R9: a tap on the free floor: a look (half the time a walk); a tap on her: a wag */
    world_init(&s_world, 41); ev(EV_TAP, 120, 420, 0);
    CHECK(p->prio == 9 && run_plan(20) == PLAN_DONE, "R9: a tap on the floor gets a look");
    world_init(&s_world, 42); ev(EV_TAP, (int)l->x, (int)l->y - 20, 0); plan_describe(&s_world, 0, d, sizeof d);
    CHECK(strstr(d, "tap on her") != NULL, "a tap on her: a happy wag");
    /* the priorities: a running call (R2) is not cut by a tap (R9) or a shake (R7); a call cuts a tap */
    world_init(&s_world, 43); ev(EV_WHISTLE, 0, 0, 0); ev(EV_TAP, 120, 420, 0); ev(EV_SHAKE, 0, 0, 0);
    CHECK(p->prio == 2, "a call is not cut by a tap or a shake");
    world_init(&s_world, 44); ev(EV_TAP, 120, 420, 0); ev(EV_WHISTLE, 0, 0, 0);
    CHECK(p->prio == 2, "a call cuts a tap's look");
    /* M4.7 and R1: the bowls, the pad, the spots; the last spot cleaned forgives her and she runs out */
    world_init(&s_world, 45);
    pak_image_t im; int bx = 0, by = 0;
    if (pak_image(&s_pak, IMG_BOWL_FOOD, &im)) { bx = im.x + im.w / 2; by = im.y + im.h / 2; }
    else { const map_point_t *bf = &MAPS[VIEW_ROOM].points[SPOT_BOWL_FOOD]; bx = bf->x; by = bf->y - 4; }
    s_world.things.food = 0; ev(EV_TAP, bx, by, 0);
    CHECK(s_world.things.food == 3 && s_world.sparkle_t > 0 && p->status != PLAN_RUNNING, "a tap on the empty bowl fills it (3), with a sparkle, and is no R9");
    if (pak_image(&s_pak, IMG_PAD, &im)) { s_world.things.pad = 2; ev(EV_TAP, im.x + im.w / 2, im.y + im.h / 2, 0); CHECK(s_world.things.pad == 0, "a tap on the full pad changes it"); }
    plan_one(&s_world, 0, ACT_MISCHIEF_FLOOR, ST_WALK); run_plan(30);
    plan_one(&s_world, 0, ACT_HIDE_BENCH, ST_WALK); run_plan(30);
    CHECK(s_world.things.guilty && s_world.things.n_mess == 1 && p->hide == 1, "a spot, guilty, under the bench");
    ev(EV_WHISTLE, 0, 0, 0); CHECK(p->status != PLAN_RUNNING || p->prio != 2, "hidden and guilty she does not come to a whistle");
    ev(EV_TAP, s_world.things.mess[0][0], s_world.things.mess[0][1], 0);
    CHECK(s_world.things.n_mess == 0 && p->prio == 1, "R1: the last spot cleaned starts the release");
    CHECK(run_plan(30) == PLAN_DONE && !s_world.things.guilty && p->hide == 0 && fabsf(l->x - front->x) < 2, "R1: forgiven, she runs out to the front");
    printf("selftest-reflex: ok\n");
    return 0;
}

/* --selftest-toys (spec 8.9, M4.6): a throw flies and bounces, the walls send it back, it never leaves its view
   on its own, in her mouth it follows her head and goes with her through the door */
static int selftest_toys(void) {
    nav_init(&s_nav); world_init(&s_world, 51); toy_t *ty = &s_world.things.toys[TOY_PLUSH];
    ty->x = 240; ty->y = 300; toy_throw(&s_world, TOY_PLUSH, 900, 0);
    int landings = 0; float lastz = 0;
    for (int i = 0; i < 300; i++) {
        toys_tick(&s_world, 1.0f / 30.0f);
        if (lastz > 0 && ty->z == 0) landings++;
        lastz = ty->z;
        CHECK(ty->view == VIEW_ROOM && nav_clear(VIEW_ROOM, (int)ty->x, (int)ty->y, 0), "a thrown toy stays on its view's floor");
    }
    CHECK(landings >= 2, "it bounces (lands more than once)");
    ty->x = 400; ty->y = 300; ty->z = 0; toy_throw(&s_world, TOY_PLUSH, 6000, 0);     /* hard at the right wall */
    bool back = false; for (int i = 0; i < 200; i++) { toys_tick(&s_world, 1.0f / 30.0f); if (ty->vx < 0) back = true; }
    CHECK(back && nav_clear(VIEW_ROOM, (int)ty->x, (int)ty->y, 0), "the wall sends it back");
    actor_t *l = &s_world.actors[0]; l->dir = DIR_E;
    toy_pick(&s_world, TOY_PLUSH, 0); toys_tick(&s_world, 0.03f);
    float dx, h; toy_mouth(l, &dx, &h);
    CHECK(ty->held_by == 0 && fabsf(ty->x - (l->x + dx)) < 0.5f && ty->z > 20, "in her mouth it is at her head");
    plan_one(&s_world, 0, ACT_ENTER_YARD, ST_WALK); run_plan(40); toys_tick(&s_world, 0.03f);
    CHECK(l->view == VIEW_YARD && ty->view == VIEW_YARD, "it goes out with her, in her mouth");
    toy_drop(&s_world, TOY_PLUSH);
    CHECK(ty->held_by < 0 && ty->view == VIEW_YARD && nav_clear(VIEW_YARD, (int)ty->x, (int)ty->y, 0), "dropped, it lies on the yard's floor");
    printf("selftest-toys: ok\n");
    return 0;
}

/* ---- M5: the needs, the pad and the guilt, the calendar, the time away, the save ---- */
#include "life.h"
#include "save.h"
/* --selftest-progress (spec 9.1-9.2, M5.1-M5.2) */
static int selftest_progress(void) {
    nav_init(&s_nav); world_init(&s_world, 61); actor_t *l = &s_world.actors[0];
    const float h0 = s_world.hunger, b0 = s_world.bladder, lo0 = s_world.lonely;
    l->anim = ANIM_IDLE_STAND;
    for (int i = 0; i < 3600; i++) life_tick(&s_world, 1.0f);       /* an hour awake, alone */
    CHECK(fabsf(s_world.hunger - h0 - 0.35f) < 0.01f && fabsf(s_world.bladder - b0 - 0.45f) < 0.01f && fabsf(s_world.lonely - lo0 - 0.6f) < 0.01f,
          "an hour alone: hunger +0.35, bladder +0.45, lonely +0.6 (spec 9.1)");
    const float e0 = s_world.energy; l->anim = ANIM_SLEEP_CURL;
    for (int i = 0; i < 3600; i++) life_tick(&s_world, 1.0f);
    CHECK(fabsf(s_world.energy - (e0 + 2.5f > 10 ? 10 : e0 + 2.5f)) < 0.01f, "an hour asleep: energy +2.5");
    l->anim = ANIM_IDLE_STAND; s_world.lonely = 6;
    for (int i = 0; i < 3600; i++) { life_owner(&s_world); life_tick(&s_world, 1.0f); }
    CHECK(fabsf(s_world.lonely - 4.0f) < 0.01f, "an hour with the owner: lonely -2");
    s_world.hunger = 7; s_world.bladder = 3; plan_one(&s_world, 0, ACT_EAT, ST_WALK); run_plan(30);
    CHECK(s_world.hunger < 0.01f && fabsf(s_world.bladder - 5.0f) < 0.05f && s_world.things.food == 2, "eating: one serving, hunger 0, bladder +2");
    s_world.bored = 8; plan_one(&s_world, 0, ACT_PLAY_BOW, ST_WALK); run_plan(10);
    CHECK(fabsf(s_world.bored - 5.0f) < 0.05f, "playing: bored -3");
    /* the pad (spec 9.2): used after one visit, full after the second; the floor: a spot and guilt */
    s_world.bladder = 9; plan_one(&s_world, 0, ACT_USE_PAD, ST_WALK); run_plan(30);
    CHECK(s_world.things.pad == 1 && s_world.bladder < 0.01f && !s_world.things.guilty, "the pad once: used, no guilt, bladder 0");
    s_world.bladder = 9; plan_one(&s_world, 0, ACT_USE_PAD, ST_WALK); run_plan(30);
    CHECK(s_world.things.pad == 2, "the pad twice: full");
    s_world.bladder = 9; plan_one(&s_world, 0, ACT_MISCHIEF_FLOOR, ST_WALK); run_plan(30);
    CHECK(s_world.things.n_mess == 1 && s_world.things.guilty && s_world.bladder < 0.01f, "the floor: a spot, guilty");
    /* H9's chance (spec 9.2) */
    s_world.bored = 5; s_world.lonely = 5; s_world.things.pad = 0;
    CHECK(fabsf(life_mischief_p(&s_world, DAY_NORMAL) - 0.05f) < 0.001f, "content, a clean pad: p = 0.05");
    s_world.bored = 10; s_world.lonely = 10; s_world.things.pad = 2;
    CHECK(fabsf(life_mischief_p(&s_world, DAY_NORMAL) - 0.85f) < 0.001f, "bored, lonely, the pad full: p capped at 0.85");
    CHECK(life_mischief_p(&s_world, DAY_MEMORIAL) == 0, "on September 12th: p = 0");
    CHECK(life_bucket(0) == 0 && life_bucket(5.5f) == 5 && life_bucket(10) == 9, "the brain's buckets 0..9");
    printf("selftest-progress: ok\n");
    return 0;
}
/* --selftest-calendar (spec 9.3-9.5, M5.3) */
static int selftest_calendar(void) {
    lili_date_t d;
    life_date(life_local(2026, 10, 27, 13, 5, 9), &d);
    CHECK(d.year == 2026 && d.month == 10 && d.day == 27 && d.hour == 13 && d.minute == 5 && d.second == 9, "a local time round trip");
    life_date(life_local(2028, 2, 29, 23, 59, 59) + 1, &d);
    CHECK(d.year == 2028 && d.month == 3 && d.day == 1 && d.hour == 0, "a leap day rolls into March");
    life_date(0, &d); CHECK(d.year == 1970 && d.month == 1 && d.day == 1, "the epoch");
    CHECK(life_daypart_of(7) == 0 && life_daypart_of(12) == 0 && life_daypart_of(13) == 1 && life_daypart_of(17) == 1 && life_daypart_of(18) == 2 &&
          life_daypart_of(21) == 2 && life_daypart_of(22) == 3 && life_daypart_of(6) == 3, "the dayparts' borders (spec 9.3)");
    CHECK(life_season_of(3) == 0 && life_season_of(5) == 0 && life_season_of(6) == 1 && life_season_of(9) == 2 && life_season_of(12) == 3 && life_season_of(2) == 3, "the seasons by month (spec 9.4)");
    CHECK(life_day_of(1, 15) == DAY_ADOPT && life_day_of(6, 27) == DAY_BDAY && life_day_of(9, 12) == DAY_MEMORIAL && life_day_of(9, 13) == DAY_NORMAL, "the special days (spec 9.5, D-04)");
    world_init(&s_world, 71); s_world.season_force = -1; s_world.daypart_force = -1;
    life_init(&s_world, life_local(2026, 7, 3, 15, 0, 0));
    CHECK(world_season(&s_world) == 1 && world_daypart(&s_world) == 1 && world_day(&s_world) == DAY_NORMAL, "the world from its clock: summer afternoon");
    s_world.clock_s += 8 * 3600;                                     /* 23:00 */
    CHECK(world_daypart(&s_world) == 3, "and the clock moves the daypart");
    life_init(&s_world, life_local(2026, 9, 12, 10, 0, 0));
    CHECK(world_day(&s_world) == DAY_MEMORIAL && !plan_possible(&s_world, 0, ACT_MISCHIEF_FLOOR), "September 12th: no mischief at all (H15, spec 10.5)");
    life_init(&s_world, life_local(2026, 9, 13, 10, 0, 0));
    CHECK(plan_possible(&s_world, 0, ACT_MISCHIEF_FLOOR), "the next day it is possible again");
    printf("selftest-calendar: ok\n");
    return 0;
}
/* --selftest-offline (spec 9.6, M5.4): the time away, deterministic, its events and scenes in their bands */
static int selftest_offline(void) {
    nav_init(&s_nav);
    int scenes[5] = { 0 }, floors = 0, pads = 0; const int N = 400;
    for (int k = 0; k < N; k++) {                                    /* away 1..36 h, leaving at every hour of the day, many seeds */
        world_init(&s_world, 1000 + k); s_world.season_force = -1; s_world.daypart_force = -1;
        life_init(&s_world, life_local(2026, 10, 5, (k * 5) % 24, 0, 0));
        offline_log_t lg; const int sc = life_offline(&s_world, (1 + k % 36) * 3600, &lg);
        scenes[sc]++; floors += lg.pees_floor; pads += lg.pees_pad;
    }
    printf("offline: %d absences: greet %d, guilty %d, caught %d, outside %d, asleep %d; pees: pad %d, floor %d\n",
           N, scenes[0], scenes[1], scenes[2], scenes[3], scenes[4], pads, floors);
    for (int i = 0; i < 5; i++) CHECK(scenes[i] > 0, "every wake-up scene happens");
    CHECK(pads > 0 && floors > 0, "both the pad and the floor happen");
    int short_floor = 0, short_n = 0;                                /* a contented dog, a clean pad, 3 hours: mostly no spot */
    for (int k = 0; k < 200; k++) {
        world_init(&s_world, 3000 + k); life_init(&s_world, life_local(2026, 10, 5, 10, 0, 0)); s_world.bladder = 7;
        offline_log_t lg; life_offline(&s_world, 3 * 3600, &lg); short_floor += lg.pees_floor; short_n += lg.pees_floor + lg.pees_pad;
    }
    printf("offline: 3 h away, content: %d pees, %d on the floor\n", short_n, short_floor);
    CHECK(short_n > 150 && short_floor * 5 < short_n, "short and content: the pad, the floor under one time in five (p = 0.05 + a little)");
    world_t a, b; world_init(&a, 77); world_init(&b, 77);
    life_init(&a, life_local(2026, 10, 5, 9, 0, 0)); life_init(&b, life_local(2026, 10, 5, 9, 0, 0));
    s_world = a; int sa = life_offline(&s_world, 20 * 3600, NULL); a = s_world;
    s_world = b; int sb = life_offline(&s_world, 20 * 3600, NULL); b = s_world;
    CHECK(sa == sb && a.hunger == b.hunger && a.things.n_mess == b.things.n_mess && a.actors[0].x == b.actors[0].x, "the same seed and absence: the same story");
    world_init(&s_world, 78); life_init(&s_world, life_local(2026, 10, 5, 9, 0, 0));
    life_offline(&s_world, 400LL * 86400, NULL);
    CHECK(s_world.lonely == 10 && s_world.bored == 10, "a year away is cut to 30 days: everything saturated");
    int mem_floor = 0;
    for (int k = 0; k < 100; k++) {                                  /* a whole September 12th away: never the floor */
        world_init(&s_world, 2000 + k); life_init(&s_world, life_local(2026, 9, 12, 0, 10, 0)); s_world.bored = s_world.lonely = 9; s_world.things.pad = 2;
        offline_log_t lg; life_offline(&s_world, 23 * 3600, &lg); mem_floor += lg.pees_floor;
    }
    CHECK(mem_floor == 0, "away on September 12th: no spot on the floor");
    printf("selftest-offline: ok\n");
    return 0;
}
/* --selftest-save (spec 9.7, M5.5): a round trip, a broken byte refused, an older (shorter) save read with defaults */
static int selftest_save(void) {
    nav_init(&s_nav); world_init(&s_world, 81);
    s_world.hunger = 4.25f; s_world.lonely = 6.5f; s_world.things.n_mess = 2; s_world.things.mess[1][0] = 123; s_world.things.guilty = true;
    s_world.things.pad = 1; s_world.things.food = 1; s_world.actors[0].view = VIEW_YARD; s_world.actors[0].x = 300.5f; s_world.pets = 42;
    toy_pick(&s_world, TOY_ROPE, 0);
    uint8_t buf[SAVE_MAX]; const int n = save_write(&s_world, 1234567, buf, sizeof buf);
    CHECK(n > 40 && n <= SAVE_MAX, "the save fits its buffer");
    world_t keep = s_world; world_init(&s_world, 82); int64_t at = 0, first = 0;
    CHECK(save_read(&s_world, buf, n, &at, &first), "a save reads back");
    CHECK(s_world.hunger == keep.hunger && s_world.lonely == keep.lonely && s_world.things.n_mess == 2 && s_world.things.mess[1][0] == 123 &&
          s_world.things.guilty && s_world.things.pad == 1 && s_world.things.food == 1 && s_world.actors[0].view == VIEW_YARD &&
          s_world.actors[0].x == 300.5f && s_world.pets == 42 && s_world.things.toys[TOY_ROPE].held_by == 0 && first == 1234567 && at == world_now(&keep),
          "every field comes back");
    for (int i = 0; i < n; i += 7) { buf[i] ^= 0x10; CHECK(!save_read(&s_world, buf, n, NULL, NULL), "a flipped bit is refused (CRC, magic, length)"); buf[i] ^= 0x10; }
    /* an older save: the same layout cut after the needs (a v0 that knew less), its CRC made right */
    uint8_t old[SAVE_MAX]; memcpy(old, buf, (size_t)n);
    const int plen = 8 + 8 + 6 * 4; old[6] = (uint8_t)plen; old[7] = 0;
    uint32_t c = 0xFFFFFFFFu; for (int i = 0; i < 8 + plen; i++) { c ^= old[i]; for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1))); } c = ~c;
    old[8 + plen] = (uint8_t)c; old[9 + plen] = (uint8_t)(c >> 8); old[10 + plen] = (uint8_t)(c >> 16); old[11 + plen] = (uint8_t)(c >> 24);
    world_init(&s_world, 83);
    CHECK(save_read(&s_world, old, 12 + plen, NULL, NULL) && s_world.hunger == keep.hunger && s_world.things.food == 3 && s_world.actors[0].view == VIEW_ROOM,
          "an older, shorter save reads: its fields, and the defaults after them");
    CHECK(!save_read(&s_world, (const uint8_t *)"LSAVxxxx", 8, NULL, NULL), "garbage is refused");
    printf("selftest-save: ok (%d B)\n", n);
    return 0;
}

static double now_us(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3; }
static int bench(void) {
    canvas_init(); world_init(&s_world, 1);
    const int N = 2000; double t_render = 0, t_flush = 0, t_mask = 0;
    for (int i = 0; i < N; i++) {
        double t0 = now_us(); world_render_cached(&s_world, &s_r8, &s_bg);
        double t1 = now_us(); r8_flush_rows(&s_r8, s_fb, 0, LILI_PHYS_H);
        double t2 = now_us(); mask_apply_rows(&s_mask, s_fb, 0, LILI_PHYS_H);
        double t3 = now_us(); t_render += t1 - t0; t_flush += t2 - t1; t_mask += t3 - t2;
    }
    printf("bench (PC, %d frames, static layer cached): render %.1f us, flush %.1f us, mask %.1f us per frame - the panel's numbers come from M2.6\n", N, t_render / N, t_flush / N, t_mask / N);
    return 0;
}

/* ---------- the window: the picture on the left, the inspector on the right ---------- */
#include "inspector.h"
static ev_queue_t s_q;
static lv_draw_buf_t s_draw_buf;
static lv_obj_t *s_canvas;
static uint32_t s_last_ms;
static float s_fps, s_frame_ms;
static uint32_t tick_cb(void) { return SDL_GetTicks(); }

/* the one consumer: every event is shown (console + terminal), then handed to the world */
static void drain_events(void) {
    ev_t e;
    while (ev_pop(&s_q, &e)) {
        char line[72];
        snprintf(line, sizeof line, e.c ? "%7.1f  %-14s %d %d %d" : "%7.1f  %-14s %d %d", s_world.clock_s, ev_name((ev_type_t)e.type), e.a, e.b, e.c);
        inspector_log(line); puts(line);
        world_handle_event(&s_world, &e);
    }
}

/* ---------- --anim-viewer (spec 7.5 B8): one animation from all 8 directions at once ----------
 * The compass on a 3 x 3 grid (N at the top, S at the bottom, the middle cell shows the frame count), at the
 * game's size on the master palette's floor (Lili's colours are the same slots in every palette), drawn through
 * the game's own world_actor_frame, so a reversed animation (stand_up) plays as the game plays it.
 * Window: A / Right next, Left previous, Tab the yard's size, Space pause, . one frame on (paused), Q quit.
 * Headless: --anim-viewer-shot DIR [frame] writes DIR/<anim>.ppm for every animation (default frame 2). */
static struct { int k; bool on, yard, pause; float t; } s_av;
static void viewer_draw(void) {
    static const int8_t GX[DIR_COUNT] = { 1, 0, 0, 0, 1, 2, 2, 2 }, GY[DIR_COUNT] = { 2, 2, 1, 0, 0, 0, 1, 2 };   /* S SW W NW N NE E SE */
    enum { CELL = LILI_LOG_W / 3 };
    uint8_t pal[256][3];
    if (pak_palette(&s_pak, pal)) r8_set_palette(&s_r8, pal);
    r8_clear(&s_r8, PAL_FLOOR + 5);
    for (int i = 1; i < 3; i++) { r8_fill(&s_r8, i * CELL, 0, 1, LILI_LOG_H, PAL_FLOOR + 3); r8_fill(&s_r8, 0, i * CELL, LILI_LOG_W, 1, PAL_FLOOR + 3); }
    sprite_set_t set; sprite_frame_t fr; int n = 0, shown = 0;
    for (int d = 0; d < DIR_COUNT; d++) {
        actor_t a = { 0 }; a.anim = ANIMS[s_av.k].id; a.dir = (dir8_t)d; a.anim_t = s_av.t;
        const int px = GX[d] * CELL + CELL / 2, py = GY[d] * CELL + CELL * 4 / 5;
        r8_fill(&s_r8, px - 30, py, 61, 1, PAL_FLOOR + 2);                          /* the ground line under the paws */
        if (world_actor_frame(&a, s_av.yard ? ANIM_YARD_OFFSET : 0, &set, &fr)) { pak_blit(&s_r8, &set, &fr, px, py); n = set.n_frames; shown++; }
    }
    if (n) {                                                                         /* the middle cell: one tick per frame, the current one lit */
        int f = (int)(s_av.t * set.fps); if (!set.loop && f >= n) f = n - 1; else f %= n;
        if (ANIM_REVERSE_OF(ANIMS[s_av.k].id)) f = n - 1 - f;
        for (int i = 0; i < n; i++) r8_fill(&s_r8, CELL + (CELL - n * 12) / 2 + i * 12, CELL + CELL / 2 - 4, 9, 9, i == f ? PAL_WHITE : PAL_FLOOR + 2);
    }
    if (!shown) r8_fill(&s_r8, CELL + 20, CELL + 20, CELL - 40, CELL - 40, PAL_DEBUG);   /* no such set in the pak */
    r8_flush_rows(&s_r8, s_fb, 0, LILI_PHYS_H);
    mask_apply_rows(&s_mask, s_fb, 0, LILI_PHYS_H);
}
static void viewer_announce(void) {
    sprite_set_t set; char line[72]; const uint16_t id = ANIMS[s_av.k].id, src = ANIM_REVERSE_OF(id) ? ANIM_REVERSE_OF(id) : id;
    if (pak_sprite_set(&s_pak, (uint16_t)(src + (s_av.yard ? ANIM_YARD_OFFSET : 0)), &set))
        snprintf(line, sizeof line, "viewer: %s (%d)%s, %d fr, %d fps, %s, %dx%d%s", ANIMS[s_av.k].name, id, s_av.yard ? " yard" : "",
                 set.n_frames, set.fps, set.loop ? "loop" : "once", set.cell_w, set.cell_h, ANIM_REVERSE_OF(id) ? ", reversed" : "");
    else snprintf(line, sizeof line, "viewer: %s (%d): not in the pak", ANIMS[s_av.k].name, id);
    inspector_log(line); puts(line);
}
static int viewer_shot(const char *dir, int frame_no) {
    canvas_init(); char path[512];
    for (s_av.k = 0; s_av.k < N_ANIMS; s_av.k++) {
        sprite_set_t set; const uint16_t id = ANIMS[s_av.k].id;
        if (!pak_sprite_set(&s_pak, ANIM_REVERSE_OF(id) ? ANIM_REVERSE_OF(id) : id, &set)) continue;
        s_av.t = (frame_no + 0.5f) / set.fps; viewer_draw();
        snprintf(path, sizeof path, "%s/%s.ppm", dir, ANIMS[s_av.k].name); write_ppm(path, s_fb);
    }
    printf("viewer: %d animations, frame %d -> %s\n", N_ANIMS, frame_no, dir);
    return 0;
}

/* the save (spec 9.7): this folder's lili_save.bin; the PC's local time is the clock */
#define SIM_SAVE "lili_save.bin"
static int64_t s_first_start;
static int64_t pc_local_now(void) {                 /* the PC's wall clock as local seconds (life.c's own calendar: no zone) */
    time_t t = time(NULL); const struct tm *lt = localtime(&t);
    return life_local(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
}
static void sim_save(void) {
    uint8_t buf[SAVE_MAX]; int n = save_write(&s_world, s_first_start, buf, sizeof buf);
    FILE *f = n ? fopen(SIM_SAVE, "wb") : NULL;
    if (f) { fwrite(buf, 1, (size_t)n, f); fclose(f); }
}
static void sim_load(bool fresh) {
    const int64_t now = pc_local_now();
    life_init(&s_world, now); s_first_start = now;
    if (fresh) { printf("save: a fresh start\n"); return; }
    uint8_t buf[SAVE_MAX]; FILE *f = fopen(SIM_SAVE, "rb"); int n = f ? (int)fread(buf, 1, sizeof buf, f) : 0; if (f) fclose(f);
    int64_t at = 0;
    if (n <= 0 || !save_read(&s_world, buf, n, &at, &s_first_start)) { printf("save: none (%s), a new start\n", SIM_SAVE); return; }
    s_world.clock_s = 0; s_world.epoch0 = at;                       /* back to the save's moment, then the time away up to now */
    offline_log_t lg; const int sc = life_offline(&s_world, now - at, &lg);
    s_world.epoch0 = now - (int64_t)s_world.clock_s;
    char line[96]; snprintf(line, sizeof line, "away %.1f h: %s (pad %d, floor %d, meals %d)", (now - at) / 3600.0, life_scene_name(sc), lg.pees_pad, lg.pees_floor, lg.meals);
    puts(line); inspector_log(line);
}

static void frame_cb(lv_timer_t *t) {
    (void)t;
    double t0 = now_us();
    uint32_t now = SDL_GetTicks();
    float dt = (now - s_last_ms) / 1000.0f; s_last_ms = now;
    if (dt > 0.1f) dt = 0.1f;                           /* a dragged window pauses time, not jumps it */
    if (!s_av.on) {                                     /* the mouse on the picture is the finger (M4.3) */
        static gesture_t g; gesture_ctx_t gc; int fx, fy;
        world_gesture_ctx(&s_world, &gc);
        const bool fd = inspector_finger(&fx, &fy);
        gesture_feed(&g, &gc, fd, fx, fy, now, &s_q);
        world_finger(&s_world, fd, fx, fy);
    }
    drain_events();
    if (s_av.on) { if (!s_av.pause) s_av.t += dt; viewer_draw(); }
    else { if (demo_active()) demo_tick(&s_world, dt); else plan_tick(&s_world, 0, dt); world_tick(&s_world, dt); frame(); }
    static float save_t; save_t += dt;
    if (save_t > 300.0f && !demo_active()) { save_t = 0; sim_save(); }                /* every 5 minutes (spec 9.7) */
    lv_obj_invalidate(s_canvas);
    if (dt > 0) s_fps = s_fps > 0 ? 0.9f * s_fps + 0.1f / dt : 1.0f / dt;
    s_frame_ms = (float)((now_us() - t0) / 1000.0);
    static float since; since += dt;
    if (since > 0.2f) { since = 0; inspector_update(s_fps, s_frame_ms); }
}

static void ui_build(void) {
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_sdl_window_create(LILI_PHYS_W + INSPECTOR_W, LILI_PHYS_H);
    lv_sdl_window_set_title(disp, "Pixel Lili sim  (Tab szoba/udvar, W futty, N nev, Enter felvetel, Space razas, T +1 ora, 1-4 evszak, D napszak, A animacio, R irany, M debug, Q kilepes)");
    lv_sdl_mouse_create();
    lv_draw_buf_init(&s_draw_buf, LILI_PHYS_W, LILI_PHYS_H, LV_COLOR_FORMAT_RGB565, LILI_PHYS_W * 2, s_fb, sizeof s_fb);
    s_canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(s_canvas, &s_draw_buf);
    lv_obj_set_pos(s_canvas, 0, 0);
    inspector_init(&s_world, &s_q, &s_debug);
    inspector_bind_canvas(s_canvas);
    s_last_ms = SDL_GetTicks();
    lv_timer_create(frame_cb, 16, NULL);
}

static bool key_edge(int sc) {                          /* true once per press */
    static bool prev[SDL_NUM_SCANCODES];
    bool down = SDL_GetKeyboardState(NULL)[sc], rise = down && !prev[sc];
    prev[sc] = down; return rise;
}

/* --inspector-shot out.ppm: the whole window (picture + inspector) to a PPM, no display needed
   (run it with SDL_VIDEODRIVER=dummy); a few events are pushed so the console shows something */
static int inspector_shot(const char *path) {
    canvas_init(); world_init(&s_world, 1234); ev_init(&s_q);
    ui_build();
    ev_push(&s_q, EV_TAP, 120, 140); ev_push(&s_q, EV_WHISTLE, 0, 0); ev_push(&s_q, EV_SET_SEASON, 3, 0); ev_push(&s_q, EV_TIME_SKIP, 3600, 0);
    drain_events(); frame(); inspector_update(30.0f, 1.2f);
    for (int i = 0; i < 5; i++) { lv_obj_invalidate(lv_screen_active()); lv_timer_handler(); }
    lv_draw_buf_t *snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_XRGB8888);
    if (!snap) { printf("snapshot failed\n"); return 1; }
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return 1; }
    int w = snap->header.w, h = snap->header.h; fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        const uint8_t *p = (const uint8_t *)snap->data + (size_t)y * snap->header.stride + (size_t)x * 4;
        unsigned char rgb[3] = { p[2], p[1], p[0] }; fwrite(rgb, 1, 3, f);        /* BGRX -> RGB */
    }
    fclose(f); lv_draw_buf_destroy(snap);
    printf("wrote %s (%d x %d)\n", path, w, h); return 0;
}

int main(int argc, char **argv) {
    pak_load("../assets/build/assets.pak");
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();
    if (argc > 1 && !strcmp(argv[1], "--selftest-nav")) return selftest_nav();
    if (argc > 1 && !strcmp(argv[1], "--selftest-gesture")) return selftest_gesture();
    if (argc > 1 && !strcmp(argv[1], "--selftest-plan")) return selftest_plan();
    if (argc > 1 && !strcmp(argv[1], "--selftest-reflex")) return selftest_reflex();
    if (argc > 1 && !strcmp(argv[1], "--selftest-toys")) return selftest_toys();
    if (argc > 1 && !strcmp(argv[1], "--selftest-progress")) return selftest_progress();
    if (argc > 1 && !strcmp(argv[1], "--selftest-calendar")) return selftest_calendar();
    if (argc > 1 && !strcmp(argv[1], "--selftest-offline")) return selftest_offline();
    if (argc > 1 && !strcmp(argv[1], "--selftest-save")) return selftest_save();
    if (argc > 1 && !strcmp(argv[1], "--selftest-render")) return selftest_render(argc > 2 && !strcmp(argv[2], "--update"));
    if (argc > 1 && !strcmp(argv[1], "--bench")) return bench();
    if (argc > 2 && !strcmp(argv[1], "--inspector-shot")) return inspector_shot(argv[2]);
    if (argc > 2 && !strcmp(argv[1], "--demo-record")) return demo_record(argv[2], argc > 3 ? atoi(argv[3]) : 12);
    if (argc > 2 && !strcmp(argv[1], "--anim-viewer-shot")) return viewer_shot(argv[2], argc > 3 ? atoi(argv[3]) : 2);
    if (argc > 2 && !strcmp(argv[1], "--snapshot")) return snapshot(argv[2]);
    if (argc > 2 && !strcmp(argv[1], "--screenshot")) {
        s_debug = argc > 3 && !strcmp(argv[3], "--debug");
        canvas_init(); world_init(&s_world, 1234);
        if (getenv("LILI_VIEW")) s_world.view = (view_t)(atoi(getenv("LILI_VIEW")) % VIEW_COUNT);   /* 1 = the yard */
        frame(); write_ppm(argv[2], s_fb);
        printf("wrote %s\n", argv[2]); return 0;
    }
    canvas_init();
    world_init(&s_world, (uint32_t)time(NULL));
    ev_init(&s_q);
    ui_build();
    nav_init(&s_nav);
    sim_load(argc > 1 && !strcmp(argv[1], "--fresh"));
    if (argc > 1 && !strcmp(argv[1], "--demo")) demo_start();      /* the window with the tour already running (G stops it) */
    if (argc > 1 && !strcmp(argv[1], "--anim-viewer")) {           /* optional: the animation to start at, by name */
        s_av.on = true;
        for (int k = 0; argc > 2 && k < N_ANIMS; k++) if (!strcmp(argv[2], ANIMS[k].name)) s_av.k = k;
        viewer_announce();
    }
    for (;;) {
        uint32_t wait = lv_timer_handler();
        if (key_edge(SDL_SCANCODE_Q)) { if (!s_av.on && !demo_active()) sim_save(); break; }
        if (s_av.on) {
            int step = key_edge(SDL_SCANCODE_A) || key_edge(SDL_SCANCODE_RIGHT) ? 1 : key_edge(SDL_SCANCODE_LEFT) ? -1 : 0;
            if (step) { s_av.k = (s_av.k + step + N_ANIMS) % N_ANIMS; s_av.t = 0; viewer_announce(); }
            if (key_edge(SDL_SCANCODE_TAB)) { s_av.yard = !s_av.yard; viewer_announce(); }
            if (key_edge(SDL_SCANCODE_SPACE)) s_av.pause = !s_av.pause;
            if (key_edge(SDL_SCANCODE_PERIOD) && s_av.pause) {
                sprite_set_t set; const uint16_t id = ANIMS[s_av.k].id;
                if (pak_sprite_set(&s_pak, ANIM_REVERSE_OF(id) ? ANIM_REVERSE_OF(id) : id, &set)) s_av.t += 1.0f / set.fps;
            }
            SDL_Delay(wait < 5 ? wait : 5);
            continue;
        }
        if (key_edge(SDL_SCANCODE_TAB))    ev_push(&s_q, EV_SWITCH_VIEW, 0, 0);
        if (key_edge(SDL_SCANCODE_W))      ev_push(&s_q, EV_WHISTLE, 0, 0);
        if (key_edge(SDL_SCANCODE_N))      ev_push(&s_q, EV_CALL_NAME, 0, 0);
        if (key_edge(SDL_SCANCODE_RETURN)) ev_push(&s_q, EV_PICKUP, 0, 0);
        if (key_edge(SDL_SCANCODE_SPACE))  ev_push(&s_q, EV_SHAKE, 0, 0);
        if (key_edge(SDL_SCANCODE_T))      ev_push(&s_q, EV_TIME_SKIP, 3600, 0);
        for (int k = 0; k < 4; k++) if (key_edge(SDL_SCANCODE_1 + k)) ev_push(&s_q, EV_SET_SEASON, k, 0);
        if (key_edge(SDL_SCANCODE_D)) { int c = s_world.daypart_force; ev_push(&s_q, EV_SET_DAYPART, c >= 3 ? -1 : c + 1, 0); }
        if (key_edge(SDL_SCANCODE_M)) s_debug = !s_debug;
        if (key_edge(SDL_SCANCODE_A)) {
            actor_t *l = &s_world.actors[0];
            int k = 0;
            while (k < N_ANIMS && ANIMS[k].id != l->anim) k++;
            k = (k + 1) % N_ANIMS; l->anim = ANIMS[k].id; l->anim_t = 0;
            char line[72]; snprintf(line, sizeof line, "%7.1f  A: %s (%d)", s_world.clock_s, ANIMS[k].name, ANIMS[k].id);
            inspector_log(line); puts(line);
        }
        if (key_edge(SDL_SCANCODE_G)) { if (demo_active()) demo_stop(); else demo_start(); }
        if (key_edge(SDL_SCANCODE_R)) { actor_t *l = &s_world.actors[0]; l->dir = (dir8_t)((l->dir + 1) % DIR_COUNT); }
        SDL_Delay(wait < 5 ? wait : 5);
    }
    return 0;
}
