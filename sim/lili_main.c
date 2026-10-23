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
/* the placeholder shapes (spec 7.7) are still the picture without a pak: their tests switch the pak off */
static void placeholder_mode(bool on) {
    uint8_t pal[256][3];
    world_set_pak(on || !s_pak.data ? NULL : &s_pak);
    if (on) { pal_master(pal); r8_set_palette(&s_r8, pal); }
}
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
    CHECK(room->points[SPOT_PAD].x < LILI_LOG_W / 3 && room->points[SPOT_BOWL_FOOD].x >= 2 * LILI_LOG_W / 3 && room->points[SPOT_BOWL_WATER].x >= 2 * LILI_LOG_W / 3,
          "the pad in the left third, food and water in the right third (owner, 2026-10-19)");
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
            CHECK(pak_sprite_set(&s_pak, ANIM_IDLE_STAND, &set) && set.cell_w == 48 * K && set.pivot_x == 24 * K && set.pivot_y == 90 && set.n_frames == 8, "idle_stand: a 96 px cell, pivot (48, 90) (the 2D line, 2026-10-23), 8 frames");
            sprite_set_t ws; CHECK(pak_sprite_set(&s_pak, ANIM_WALK, &ws) && ws.dir_mask == 0xB7, "the walk has S, SW, W, N, NE and SE (PixelLab, 2026-10-23)");
            CHECK(pak_frame(&set, DIR_S, 0, &fr) && fr.rle_len > 0, "frame S/0 exists");
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
            sprite_set_t set; sprite_frame_t fr; static uint8_t cell[96 * 96];
            CHECK(pak_sprite_set(&s_pak, d->anim, &set) && pak_frame(&set, d->dir, 0, &fr), "the sitting frame exists");
            pak_frame_decode(&set, &fr, cell);
            for (int y = 0; y < LILI_LOG_H; y++) for (int x = 0; x < LILI_LOG_W; x++) {
                int cx = x - (240 - set.pivot_x), cy = y - (300 - set.pivot_y);
                bool body = cx >= 0 && cy >= 0 && cx < set.cell_w && cy < set.cell_h && cell[cy * set.cell_w + cx];
                if (!body && bg.px[y * LILI_LOG_W + x] != sc.px[y * LILI_LOG_W + x]) shadow[dp]++;
            }
        }
        CHECK(shadow[3] > 50 && shadow[1] > shadow[3] + 300, "Lili throws a shadow; by day the door's light makes it a long one (the owner, 2026-10-23)");
        s_world.daypart_force = 1; world_render(&s_world, &bg); uint16_t day = bg.lut[150 + 50];
        s_world.daypart_force = 3; world_render(&s_world, &bg); uint16_t night = bg.lut[150 + 50];
        CHECK(((night >> 11) + ((night >> 5) & 63) / 2) < ((day >> 11) + ((day >> 5) & 63) / 2), "the night palette is darker than the afternoon's");
        pak_image_t bowl; CHECK(pak_image(&s_pak, IMG_BOWL_FOOD, &bowl), "the food bowl is a prop");
        const int bx = bowl.x + bowl.w / 2, by = bowl.y + bowl.h / 2;
        s_world.daypart_force = 3; d->x = 100; d->y = 300; d->anim = ANIM_IDLE_STAND; world_render(&s_world, &sc);
        uint8_t with_bowl = sc.px[by * LILI_LOG_W + bx];
        d->anim = ANIM_EAT_DRINK; world_render(&s_world, &sc);
        CHECK(with_bowl != sc.px[by * LILI_LOG_W + bx], "while she eats (her frames hold a bowl), the room's own bowl steps aside");
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

/* ---------- the demo tour (2026-10-23): the simulator's own director, not the world's behaviour ----------
 * Lili walks a fixed route through the painted room and yard so the owner can see the animations, the
 * light and the shadows moving (G in the window, --demo-record headless). The world's own plans come in M5;
 * until then this is a script: walk / trot to a point (the direction from the way she goes), play an
 * animation for a while, or set the view, season and daypart. Coordinates are logical px (480). */
#include <math.h>
enum { D_WALK, D_TROT, D_DO, D_SET };
typedef struct { uint8_t kind; int16_t x, y; uint16_t anim; int8_t dir; float t; int8_t view, season, daypart; } demo_step_t;
static const demo_step_t DEMO[] = {
    { D_SET, 240, 290, 0, 0, 0, VIEW_ROOM, 2, 1 },                                     /* the room, autumn, afternoon */
    { D_DO, 0, 0, ANIM_IDLE_STAND, DIR_SW, 2.5f, -1, -1, -1 },
    { D_WALK, 130, 272, 0, 0, 0, -1, -1, -1 },                                          /* to the rug */
    { D_DO, 0, 0, ANIM_SLEEP_CURL, DIR_SW, 4.0f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.5f, -1, -1, -1 },
    { D_WALK, 250, 330, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PANT_SMILE, DIR_S, 3.0f, -1, -1, -1 },
    { D_WALK, 394, 443, 0, 0, 0, -1, -1, -1 },                                          /* her frames' bowl onto the food bowl */
    { D_DO, 0, 0, ANIM_EAT_DRINK, DIR_W, 3.5f, -1, -1, -1 },
    { D_WALK, 437, 448, 0, 0, 0, -1, -1, -1 },                                          /* and onto the water bowl */
    { D_DO, 0, 0, ANIM_DRINK, DIR_W, 3.0f, -1, -1, -1 },
    { D_WALK, 300, 400, 0, 0, 0, -1, -1, -1 },
    { D_WALK, 250, 250, 0, 0, 0, -1, -1, -1 },                                          /* towards the door */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.2f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_BARK_HEAD_FLIP, DIR_S, 1.8f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PLAY_BOW, DIR_SW, 2.5f, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, -1, 2 },                                               /* evening: the door's light fades */
    { D_DO, 0, 0, ANIM_IDLE_STAND, DIR_SW, 3.0f, -1, -1, -1 },
    { D_WALK, 135, 262, 0, 0, 0, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, -1, 3 },                                               /* night: only the lamp's short shadows */
    { D_DO, 0, 0, ANIM_SLEEP_CURL, DIR_SW, 4.0f, -1, -1, -1 },
    { D_SET, 236, 456, 0, 0, 0, VIEW_YARD, 0, 1 },                                      /* out to the yard: spring, afternoon */
    { D_WALK, 260, 300, 0, 0, 0, -1, -1, -1 },
    { D_WALK, 404, 246, 0, 0, 0, -1, -1, -1 },                                          /* half into the right bush (spot_bush_2) */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.6f, -1, -1, -1 },                          /* peeking out of it */
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.0f, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, 1, -1 },                                              /* summer */
    { D_WALK, 250, 250, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PLAY_BOW, DIR_SW, 2.0f, -1, -1, -1 },
    { D_WALK, 130, 210, 0, 0, 0, -1, -1, -1 },
    { D_TROT, 286, 340, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PANT_SMILE, DIR_S, 2.5f, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, 2, -1 },                                              /* autumn */
    { D_WALK, 186, 122, 0, 0, 0, -1, -1, -1 },                                          /* half behind the top bush (spot_bush_3) */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.2f, -1, -1, -1 },
    { D_WALK, 250, 330, 0, 0, 0, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, 3, -1 },                                              /* winter */
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.5f, -1, -1, -1 },
    { D_WALK, 236, 456, 0, 0, 0, -1, -1, -1 },
};

/* the way there: a breadth-first search on the map's 4 px walkable grid, keeping clear of the blocked shapes
   (the fixed things' invisible walls) by a margin, then the straight stretches merged (line of sight) */
enum { DEMO_PATH_MAX = 256 };
static int16_t s_path[DEMO_PATH_MAX][2]; static int s_path_n, s_path_i;
static bool clear_at(const map_t *m, int x, int y, int r) {
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++)
        if (!map_walkable(m, x + dx * MAP_CELL, y + dy * MAP_CELL)) return false;
    return true;
}
static bool line_clear(const map_t *m, float x0, float y0, float x1, float y1, int r) {
    float dx = x1 - x0, dy = y1 - y0; int n = (int)(sqrtf(dx * dx + dy * dy) / 2) + 1;
    for (int i = 0; i <= n; i++) if (!clear_at(m, (int)(x0 + dx * i / n), (int)(y0 + dy * i / n), r)) return false;
    return true;
}
static void demo_plan(const map_t *m, float x0, float y0, int x1, int y1, int r) {
    static int16_t prev[MAP_GW * MAP_GH]; static int16_t queue[MAP_GW * MAP_GH];
    const int start = (int)(y0 / MAP_CELL) * MAP_GW + (int)(x0 / MAP_CELL);
    int goal = (y1 / MAP_CELL) * MAP_GW + x1 / MAP_CELL;
    for (int i = 0; i < MAP_GW * MAP_GH; i++) prev[i] = -1;
    int qh = 0, qt = 0, found = -1, best = -1, best_d = 1 << 30;
    prev[start] = (int16_t)start; queue[qt++] = (int16_t)start;
    while (qh < qt) {
        int c = queue[qh++], cx = c % MAP_GW, cy = c / MAP_GW;
        int d = (cx - goal % MAP_GW) * (cx - goal % MAP_GW) + (cy - goal / MAP_GW) * (cy - goal / MAP_GW);
        if (d < best_d) { best_d = d; best = c; }                   /* the goal itself may be unreachable: the nearest then */
        if (c == goal) { found = c; break; }
        for (int k = 0; k < 8; k++) {
            static const int8_t DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
            int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 0 || ny < 0 || nx >= MAP_GW || ny >= MAP_GH) continue;
            int n = ny * MAP_GW + nx;
            if (prev[n] >= 0 || !clear_at(m, nx * MAP_CELL + 2, ny * MAP_CELL + 2, r)) continue;
            prev[n] = (int16_t)c; queue[qt++] = (int16_t)n;
        }
    }
    if (found < 0) found = best;
    static int16_t cells[MAP_GW * MAP_GH]; int nc = 0;
    for (int c = found; nc < MAP_GW * MAP_GH; c = prev[c]) { cells[nc++] = (int16_t)c; if (c == start) break; }
    float ax = x0, ay = y0; s_path_n = 0; s_path_i = 0;
    int i = nc - 1;
    while (i > 0 && s_path_n < DEMO_PATH_MAX - 1) {                     /* the farthest cell still in sight, one after the other */
        int j = 0;
        while (j < i && !line_clear(m, ax, ay, cells[j] % MAP_GW * MAP_CELL + 2, cells[j] / MAP_GW * MAP_CELL + 2, r)) j++;
        if (j == i) j = i - 1 > 0 ? i - 1 : 0;
        ax = cells[j] % MAP_GW * MAP_CELL + 2; ay = cells[j] / MAP_GW * MAP_CELL + 2;
        s_path[s_path_n][0] = (int16_t)ax; s_path[s_path_n][1] = (int16_t)ay; s_path_n++;
        i = j;
    }
    if (found == goal) { s_path[s_path_n][0] = (int16_t)x1; s_path[s_path_n][1] = (int16_t)y1; s_path_n++; }
}
enum { DEMO_N = (int)(sizeof DEMO / sizeof DEMO[0]) };
static int s_demo_i = -1, s_demo_rounds; static float s_demo_t;

static dir8_t dir_of(float dx, float dy) {             /* screen vector -> dir8 (S = down, clockwise) */
    float a = atan2f(-dx, dy) * 57.29578f; if (a < 0) a += 360.0f;
    return (dir8_t)((int)(a / 45.0f + 0.5f) & 7);
}
static void demo_start(void) { s_demo_i = 0; s_demo_t = 0; }
static void demo_tick(float dt) {
    if (s_demo_i < 0) return;
    actor_t *l = &s_world.actors[0];
    const demo_step_t *s = &DEMO[s_demo_i];
    bool done = false;
    if (s->kind == D_SET) {
        s_path_n = s_path_i = 0;
        if (s->view >= 0) { s_world.view = (view_t)s->view; l->view = (view_t)s->view; }
        if (s->season >= 0) s_world.season_force = s->season;
        if (s->daypart >= 0) s_world.daypart_force = s->daypart;
        if (s->x >= 0) { l->x = s->x; l->y = s->y; }
        done = true;
    } else if (s->kind == D_DO) {
        if (s_demo_t == 0) { l->anim = s->anim; l->dir = (dir8_t)s->dir; l->anim_t = 0; }
        s_demo_t += dt; done = s_demo_t >= s->t;
    } else {
        const uint16_t anim = s->kind == D_TROT ? ANIM_TROT : ANIM_WALK;
        const bool yard = s_world.view == VIEW_YARD;                        /* outdoors she is 0.625 the size: slower in px */
        const float speed = (s->kind == D_TROT ? 95.0f : 48.0f) * (yard ? 0.7f : 1.0f);   /* logical px per second */
        if (s_demo_t == 0) { demo_plan(world_map(&s_world, s_world.view), l->x, l->y, s->x, s->y, yard ? 2 : 3); s_demo_t = 1e-6f; }
        if (l->anim != anim) { l->anim = anim; l->anim_t = 0; }
        float step = speed * dt;
        while (step > 0 && s_path_i < s_path_n) {
            float dx = s_path[s_path_i][0] - l->x, dy = s_path[s_path_i][1] - l->y, dist = sqrtf(dx * dx + dy * dy);
            if (dist > 0.5f) l->dir = dir_of(dx, dy);
            if (dist <= step) { l->x = s_path[s_path_i][0]; l->y = s_path[s_path_i][1]; step -= dist; s_path_i++; }
            else { l->x += dx / dist * step; l->y += dy / dist * step; step = 0; }
        }
        done = s_path_i >= s_path_n;
    }
    if (done) { s_demo_i = (s_demo_i + 1) % DEMO_N; s_demo_t = 0; if (!s_demo_i) s_demo_rounds++; }
}
/* --demo-record DIR [fps]: one round of the tour, headless, as DIR/f00000.ppm ... (tools: ffmpeg -> mp4) */
static int demo_record(const char *dir, int fps) {
    canvas_init(); world_init(&s_world, 42); demo_start(); s_demo_rounds = 0;
    const float dt = 1.0f / (float)fps;
    char path[512]; int n = 0;
    while (!s_demo_rounds && n < 400 * fps) {                    /* until the tour comes round to its start */
        demo_tick(dt); world_tick(&s_world, dt); frame();
        snprintf(path, sizeof path, "%s/f%05d.ppm", dir, n++); write_ppm(path, s_fb);
    }
    printf("demo: %d frames at %d fps (%.1f s) -> %s\n", n, fps, n / (float)fps, dir);
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
    demo_tick(dt);
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
    if (argc > 2 && !strcmp(argv[1], "--demo-record")) return demo_record(argv[2], argc > 3 ? atoi(argv[3]) : 12);
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
    if (argc > 1 && !strcmp(argv[1], "--demo")) demo_start();      /* the window with the tour already running (G stops it) */
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
        if (key_edge(SDL_SCANCODE_A)) {
            actor_t *l = &s_world.actors[0];
            static const uint16_t cyc[] = { ANIM_IDLE_STAND, ANIM_IDLE_SIT_BLINK, ANIM_WALK, ANIM_TROT, ANIM_PANT_SMILE, ANIM_PLAY_BOW, ANIM_BARK_HEAD_FLIP,
                                            ANIM_EAT_DRINK, ANIM_DRINK, ANIM_SLEEP_CURL, ANIM_LOOK_AROUND };
            enum { NC = (int)(sizeof cyc / sizeof cyc[0]) }; int k = 0;
            while (k < NC && cyc[k] != l->anim) k++;
            l->anim = cyc[(k + 1) % NC]; l->anim_t = 0;
        }
        if (key_edge(SDL_SCANCODE_G)) { if (s_demo_i < 0) demo_start(); else s_demo_i = -1; }
        if (key_edge(SDL_SCANCODE_R)) { actor_t *l = &s_world.actors[0]; l->dir = (dir8_t)((l->dir + 1) % DIR_COUNT); }
        SDL_Delay(wait < 5 ? wait : 5);
    }
    return 0;
}
