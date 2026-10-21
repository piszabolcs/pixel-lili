/* lili_main.c - the Pixel Lili simulator. The same common/lili code the firmware
 * runs, in an SDL window through LVGL (the pocket-tank's sim recipe).
 *
 *   ./lilisim                         the window: the picture + the inspector (buttons, state, event console)
 *   ./lilisim --inspector-shot o.ppm  the whole window to a PPM (SDL_VIDEODRIVER=dummy: no display needed)
 *   ./lilisim --selftest              headless checks, exit 0 = green
 *   ./lilisim --bench                 headless: render + flush cost per frame (spec 6.4)
 *   ./lilisim --screenshot out.ppm    headless, one frame to a PPM
 *
 * The fish simulator (main.c) is untouched and still builds with make SQUARE=1. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
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

static int selftest(void) {
    canvas_init();
    /* --- render8: the LUT, the 2x, the stripes, the transparency --- */
    CHECK(s_r8.lut[PAL_WHITE] == 0xFFFF || s_r8.lut[PAL_WHITE] == 0xFFDE, "white maps to RGB565 white");
    CHECK(s_r8.lut[PAL_OUTLINE] == (uint16_t)(((PAL_MASTER[1][0] & 0xF8) << 8) | ((PAL_MASTER[1][1] & 0xFC) << 3) | (PAL_MASTER[1][2] >> 3)), "the outline's RGB565 is exact (the master palette's slot 1)");
    r8_clear(&s_r8, PAL_FLOOR + 5); r8_fill(&s_r8, 10, 20, 1, 1, PAL_DEBUG);
    r8_flush_rows(&s_r8, s_fb, 0, LILI_PHYS_H);
    for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++)
        CHECK(s_fb[(40 + dy) * LILI_PHYS_W + 20 + dx] == s_r8.lut[PAL_DEBUG], "one logical pixel = 2 x 2 physical ones");
    CHECK(s_fb[42 * LILI_PHYS_W + 20] == s_r8.lut[PAL_FLOOR + 5] && s_fb[40 * LILI_PHYS_W + 22] == s_r8.lut[PAL_FLOOR + 5], "and no more");
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
    r8_fill(&s_r8, -5, -5, 10, 10, PAL_WHITE); r8_fill(&s_r8, 235, 235, 20, 20, PAL_WHITE);
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
    CHECK(map_walkable(room, 52, 126) && room->points[SPOT_RUG].z == 0, "the rug is walkable: she plays and sleeps on it (owner, 2026-10-19)");
    CHECK(room->points[SPOT_PAD].x < LILI_LOG_W / 3 && room->points[SPOT_BOWL_FOOD].x >= 2 * LILI_LOG_W / 3 && room->points[SPOT_BOWL_WATER].x >= 2 * LILI_LOG_W / 3,
          "the pad in the left third, food and water in the right third (owner, 2026-10-19)");
    CHECK(room->points[SPOT_DECO].x >= LILI_LOG_W / 3 && room->points[SPOT_DECO].x < 2 * LILI_LOG_W / 3, "the season's decoration in the middle third");
    CHECK(map_walkable(room, 208, 150), "the nook under the bench is walkable (H10, H11)");
    CHECK(!map_walkable(yard, 50, 62) && !map_walkable(yard, 120, 10), "the yard: a tree trunk and the hedge block");
    for (int i = 0; i < room->n_props; i++) {                /* every interactive thing is wholly on the glass, 2 px in */
        const map_prop_t *p = &room->props[i];
        if (strcmp(p->name, "pad") && strcmp(p->name, "bowl_food") && strcmp(p->name, "bowl_water")) continue;
        if (!mask_rect_visible(p->x, p->y - p->height, p->w, p->h + p->height, 2)) { printf("FAIL: %s is not wholly on the glass\n", p->name); return 1; }
    }
    CHECK(room->props[0].x >= 0, "props exist");
    const map_prop_t *side = NULL; for (int i = 0; i < room->n_props; i++) if (!strcmp(room->props[i].name, "bench_side")) side = &room->props[i];
    CHECK(side && LILI_LOG_W - (side->x + side->w) <= 5, "the bench stands at the right wall (owner, 2026-10-19)");
    CHECK(side->y + side->h <= 180 && mask_rect_visible(side->x, side->y - side->height, side->w, side->h + side->height, 2),
          "the bench keeps its length and stays clear of the bottom-right corner arc");
    const map_prop_t *rug = NULL; for (int i = 0; i < room->n_props; i++) if (!strcmp(room->props[i].name, "rug")) rug = &room->props[i];
    CHECK(rug && rug->h * 2 == rug->w * 3, "the rug is 3:2 like the real 300 x 200 cm one, lengthwise");
    /* the Y-sort, in the yard (H13): behind a bush Lili is covered by it, in front of it she covers it */
    world_init(&s_world, 1); actor_t *l = &s_world.actors[0];
    s_world.view = VIEW_YARD; l->view = VIEW_YARD;
    l->x = 40; l->y = 112; frame();                    /* bush_1: footprint y 120-134, raised 18: its top face hides her */
    uint16_t behind = px(40, 106);
    l->y = 170; frame();
    int front_dog = 0;                                  /* the sprite (or the capsule) above the pivot: anything that is not grass */
    for (int yy = 150; yy < 170; yy++) for (int xx = 36; xx < 44; xx++) {
        uint16_t c = px(xx, yy); int grass = 0;
        for (int sh = 0; sh < 8; sh++) grass |= c == s_r8.lut[pal_ramp(RAMP_ID_GRASS, sh)];
        front_dog += !grass;
    }
    CHECK(behind == s_r8.lut[pal_ramp(RAMP_ID_HEDGE, 6)], "behind a bush, the bush covers Lili (H13)");
    CHECK(front_dog > 60, "in front of a bush, Lili is drawn over the grass");
    world_init(&s_world, 1); frame();
    uint16_t rugpx = px(30, 100);
    int on_rug = 0; for (int sh = 0; sh < 8; sh++) on_rug |= rugpx == s_r8.lut[pal_ramp(RAMP_ID_RUG_GRAY, sh)] || rugpx == s_r8.lut[pal_ramp(RAMP_ID_RUG_GREEN, sh)];
    CHECK(on_rug, "the rug is drawn where the bed was");

    /* --- the event queue (spec 6.3: the one door into the world) --- */
    ev_queue_t q; ev_init(&q); ev_t e;
    CHECK(!ev_pop(&q, &e), "an empty queue pops nothing");
    ev_push(&q, EV_TAP, 10, 20); ev_push(&q, EV_WHISTLE, 0, 0);
    CHECK(ev_pop(&q, &e) && e.type == EV_TAP && e.a == 10 && e.b == 20, "first in, first out");
    CHECK(ev_pop(&q, &e) && e.type == EV_WHISTLE && !ev_pop(&q, &e), "and then it is empty again");
    for (int i = 0; i < EV_QUEUE_LEN + 8; i++) ev_push(&q, EV_TIME_SKIP, i, 0);          /* 8 more than it holds: wraps, drops the oldest */
    CHECK(q.dropped == 8 && q.count == EV_QUEUE_LEN, "a full queue drops the oldest 8");
    CHECK(ev_pop(&q, &e) && e.a == 8, "the survivors start at the 9th event");
    CHECK(!strcmp(ev_name(EV_SHAKE), "SHAKE") && EV_COUNT == 17 && (int)(sizeof s_world.events_seen / sizeof s_world.events_seen[0]) == EV_COUNT,
          "event names, and the world's counters cover every type");
    world_init(&s_world, 1);
    ev_t sv = { EV_SWITCH_VIEW, 0, 0 }, ts = { EV_TIME_SKIP, 3600, 0 }, ss = { EV_SET_SEASON, 3, 0 }, dp = { EV_SET_DAYPART, 9, 0 }, wh = { EV_WHISTLE, 0, 0 };
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
            CHECK(pak_sprite_set(&s_pak, ANIM_IDLE_STAND, &set) && set.cell_w == 48 && set.pivot_x == 24 && set.pivot_y == 40 && set.n_frames == 6, "idle_stand: 48 px cell, pivot (24, 40), 6 frames");
            CHECK(pak_frame(&set, DIR_S, 0, &fr) && fr.rle_len > 0, "frame S/0 exists");
            r8_clear(&s_r8, PAL_FLOOR + 5); pak_blit(&s_r8, &set, &fr, 120, 140);
            int painted = 0, out_of_cell = 0;
            for (int y = 0; y < LILI_LOG_H; y++) for (int x = 0; x < LILI_LOG_W; x++) if (s_r8.px[y * LILI_LOG_W + x] != PAL_FLOOR + 5) {
                painted++; if (x < 120 - 24 || x >= 120 + 24 || y < 140 - 40 || y >= 140 + 8) out_of_cell++;
            }
            CHECK(painted > 200 && !out_of_cell, "the sprite paints inside its cell around the pivot");
            CHECK(s_r8.px[(140 - 1) * LILI_LOG_W + 120] != PAL_FLOOR + 5 || s_r8.px[(140 - 2) * LILI_LOG_W + 120] != PAL_FLOOR + 5, "the paws stand on the pivot row");
            r8_clear(&s_r8, 0); pak_blit(&s_r8, &set, &fr, 2, 2); pak_blit(&s_r8, &set, &fr, 238, 238);   /* clipped at both corners: no crash */
            CHECK(pak_frame(&set, DIR_NE, 7, &fr), "a frame index wraps, a missing direction falls back");
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

    /* --- the world --- */
    world_init(&s_world, 1234);
    for (int i = 0; i < 600; i++) world_tick(&s_world, 1.0f / 60.0f);
    CHECK(s_world.clock_s > 9.9f && s_world.clock_s < 10.1f, "10 s of ticks");
    frame();
    const actor_t *a = &s_world.actors[0];
    CHECK(px((int)a->x, (int)a->y - 6) != px(120, 220), "the placeholder dog is drawn in the room");
    world_switch_view(&s_world); frame();
    CHECK(px((int)a->x, (int)a->y - 6) == s_r8.lut[pal_ramp(RAMP_ID_GRASS, 5)], "the dog stays in the room when the yard is shown (bare grass there)");
    CHECK(s_world.view == VIEW_YARD, "Tab goes to the yard");
    world_switch_view(&s_world);
    CHECK(s_world.view == VIEW_ROOM, "and back to the room");
    world_t a1, a2; world_init(&a1, 7); world_init(&a2, 7);
    for (int i = 0; i < 1000; i++) CHECK(world_rand(&a1) == world_rand(&a2), "seeded PRNG is deterministic");
    world_init(&a1, 0); CHECK(world_rand(&a1) != 0, "seed 0 does not stall the PRNG");
    printf("selftest: ok (world %zu B, canvas %zu B vs %zu B as RGB565, %d actor(s), logical %dx%d, corner %d px)\n",
           sizeof(world_t), sizeof s_r8.px, sizeof s_fb, MAX_ACTORS, LILI_LOG_W, LILI_LOG_H, LILI_CORNER_R_PHYS);
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
        snprintf(line, sizeof line, "%7.1f  %-14s %d %d", s_world.clock_s, ev_name((ev_type_t)e.type), e.a, e.b);
        inspector_log(line); puts(line);
        world_handle_event(&s_world, &e);
    }
}

static void frame_cb(lv_timer_t *t) {
    (void)t;
    double t0 = now_us();
    uint32_t now = SDL_GetTicks();
    float dt = (now - s_last_ms) / 1000.0f; s_last_ms = now;
    if (dt > 0.1f) dt = 0.1f;                           /* a dragged window pauses time, not jumps it */
    drain_events();
    world_tick(&s_world, dt);
    frame();
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
    if (argc > 1 && !strcmp(argv[1], "--bench")) return bench();
    if (argc > 2 && !strcmp(argv[1], "--inspector-shot")) return inspector_shot(argv[2]);
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
    for (;;) {
        uint32_t wait = lv_timer_handler();
        if (key_edge(SDL_SCANCODE_Q)) break;
        if (key_edge(SDL_SCANCODE_TAB))    ev_push(&s_q, EV_SWITCH_VIEW, 0, 0);
        if (key_edge(SDL_SCANCODE_W))      ev_push(&s_q, EV_WHISTLE, 0, 0);
        if (key_edge(SDL_SCANCODE_N))      ev_push(&s_q, EV_CALL_NAME, 0, 0);
        if (key_edge(SDL_SCANCODE_RETURN)) ev_push(&s_q, EV_PICKUP, 0, 0);
        if (key_edge(SDL_SCANCODE_SPACE))  ev_push(&s_q, EV_SHAKE, 0, 0);
        if (key_edge(SDL_SCANCODE_T))      ev_push(&s_q, EV_TIME_SKIP, 3600, 0);
        for (int k = 0; k < 4; k++) if (key_edge(SDL_SCANCODE_1 + k)) ev_push(&s_q, EV_SET_SEASON, k, 0);
        if (key_edge(SDL_SCANCODE_D)) { int c = s_world.daypart_force; ev_push(&s_q, EV_SET_DAYPART, c >= 3 ? -1 : c + 1, 0); }
        if (key_edge(SDL_SCANCODE_M)) s_debug = !s_debug;
        if (key_edge(SDL_SCANCODE_A)) { actor_t *l = &s_world.actors[0]; l->anim = l->anim == ANIM_IDLE_STAND ? ANIM_WALK : ANIM_IDLE_STAND; l->anim_t = 0; }
        if (key_edge(SDL_SCANCODE_R)) { actor_t *l = &s_world.actors[0]; l->dir = (dir8_t)((l->dir + 1) % DIR_COUNT); }
        SDL_Delay(wait < 5 ? wait : 5);
    }
    return 0;
}
