/* lili_main.c - the Pixel Lili simulator. The same common/lili code the firmware
 * runs, in an SDL window through LVGL (the pocket-tank's sim recipe).
 *
 *   ./lilisim                         the window: Tab = room <-> yard, Q = quit
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
#include "lvgl/lvgl.h"
#include <SDL2/SDL.h>

static world_t s_world;
static render8_t s_r8;
static uint16_t s_fb[LILI_PHYS_W * LILI_PHYS_H];

static void canvas_init(void) {
    uint8_t pal[256][3]; pal_placeholder(pal); r8_set_palette(&s_r8, pal);
}
static void frame(void) {
    world_render(&s_world, &s_r8);
    r8_flush_rows(&s_r8, s_fb, 0, LILI_PHYS_H);        /* the sim takes the whole frame; the panel takes stripes */
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
    CHECK(s_r8.lut[PAL_OUTLINE] == (uint16_t)(((0x3B & 0xF8) << 8) | ((0x24 & 0xFC) << 3) | (0x18 >> 3)), "the outline's RGB565 is exact");
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

    /* --- the world --- */
    world_init(&s_world, 1234);
    for (int i = 0; i < 600; i++) world_tick(&s_world, 1.0f / 60.0f);
    CHECK(s_world.clock_s > 9.9f && s_world.clock_s < 10.1f, "10 s of ticks");
    frame();
    const actor_t *a = &s_world.actors[0];
    CHECK(px((int)a->x, (int)a->y - 6) != px(120, 220), "the placeholder dog is drawn in the room");
    world_switch_view(&s_world); frame();
    CHECK(px((int)a->x, (int)a->y - 6) == px(120, 220), "the dog stays in the room when the yard is shown");
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
    const int N = 2000; double t_render = 0, t_flush = 0;
    for (int i = 0; i < N; i++) {
        double t0 = now_us(); world_render(&s_world, &s_r8);
        double t1 = now_us(); r8_flush_rows(&s_r8, s_fb, 0, LILI_PHYS_H);
        double t2 = now_us(); t_render += t1 - t0; t_flush += t2 - t1;
    }
    printf("bench (PC, %d frames): render %.1f us, flush %.1f us per frame - the panel's numbers come from M2.6\n", N, t_render / N, t_flush / N);
    return 0;
}

/* ---------- the window ---------- */
static lv_draw_buf_t s_draw_buf;
static lv_obj_t *s_canvas;
static uint32_t s_last_ms;
static uint32_t tick_cb(void) { return SDL_GetTicks(); }

static void frame_cb(lv_timer_t *t) {
    (void)t;
    uint32_t now = SDL_GetTicks();
    float dt = (now - s_last_ms) / 1000.0f; s_last_ms = now;
    if (dt > 0.1f) dt = 0.1f;                           /* a dragged window pauses time, not jumps it */
    world_tick(&s_world, dt);
    frame();
    lv_obj_invalidate(s_canvas);
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();
    if (argc > 1 && !strcmp(argv[1], "--bench")) return bench();
    if (argc > 2 && !strcmp(argv[1], "--screenshot")) {
        canvas_init(); world_init(&s_world, 1234); frame(); write_ppm(argv[2], s_fb);
        printf("wrote %s\n", argv[2]); return 0;
    }
    canvas_init();
    world_init(&s_world, (uint32_t)time(NULL));
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_sdl_window_create(LILI_PHYS_W, LILI_PHYS_H);
    lv_sdl_window_set_title(disp, "Pixel Lili sim 480x480  (Tab: szoba/udvar, Q: kilépés)");
    lv_draw_buf_init(&s_draw_buf, LILI_PHYS_W, LILI_PHYS_H, LV_COLOR_FORMAT_RGB565, LILI_PHYS_W * 2, s_fb, sizeof s_fb);
    s_canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(s_canvas, &s_draw_buf);
    lv_obj_center(s_canvas);
    s_last_ms = SDL_GetTicks();
    lv_timer_create(frame_cb, 16, NULL);
    bool tab_down = false;
    for (;;) {
        uint32_t wait = lv_timer_handler();
        const Uint8 *k = SDL_GetKeyboardState(NULL);
        if (k[SDL_SCANCODE_Q]) break;
        if (k[SDL_SCANCODE_TAB] && !tab_down) world_switch_view(&s_world);
        tab_down = k[SDL_SCANCODE_TAB];
        SDL_Delay(wait < 5 ? wait : 5);
    }
    return 0;
}
