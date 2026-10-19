/* lili_main.c - the Pixel Lili simulator (M2.1). The same common/lili code the
 * firmware runs, in an SDL window through LVGL (the pocket-tank's sim recipe).
 *
 *   ./lilisim                         the window: Tab = room <-> yard, Q = quit
 *   ./lilisim --selftest              headless checks, exit 0 = green
 *   ./lilisim --screenshot out.ppm    headless, one frame to a PPM
 *
 * The fish simulator (main.c) is untouched and still builds with make SQUARE=1. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lili_world.h"
#include "lvgl/lvgl.h"
#include <SDL2/SDL.h>

static world_t s_world;
static uint16_t s_fb[LILI_PHYS_W * LILI_PHYS_H];

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
    world_init(&s_world, 1234);
    for (int i = 0; i < 600; i++) world_tick(&s_world, 1.0f / 60.0f);
    CHECK(s_world.clock_s > 9.9f && s_world.clock_s < 10.1f, "10 s of ticks");
    world_render_rgb565(&s_world, s_fb);
    const actor_t *a = &s_world.actors[0];
    uint16_t dog = px((int)a->x, (int)a->y - 6), floor = px(120, 220);
    CHECK(dog != floor, "the placeholder dog is drawn in the room");
    world_switch_view(&s_world);
    world_render_rgb565(&s_world, s_fb);
    CHECK(px((int)a->x, (int)a->y - 6) == px(120, 220), "the dog stays in the room when the yard is shown");
    CHECK(s_world.view == VIEW_YARD, "Tab goes to the yard");
    world_switch_view(&s_world);
    CHECK(s_world.view == VIEW_ROOM, "and back to the room");
    /* spec 4.6: the same seed, the same random numbers, on every platform */
    world_t a1, a2; world_init(&a1, 7); world_init(&a2, 7);
    for (int i = 0; i < 1000; i++) CHECK(world_rand(&a1) == world_rand(&a2), "seeded PRNG is deterministic");
    world_init(&a1, 0); CHECK(world_rand(&a1) != 0, "seed 0 does not stall the PRNG");
    printf("selftest: ok (world %zu bytes, %d actor(s), logical %dx%d, corner %d px)\n",
           sizeof(world_t), MAX_ACTORS, LILI_LOG_W, LILI_LOG_H, LILI_CORNER_R_PHYS);
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
    world_render_rgb565(&s_world, s_fb);
    lv_obj_invalidate(s_canvas);
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();
    if (argc > 2 && !strcmp(argv[1], "--screenshot")) {
        world_init(&s_world, 1234); world_render_rgb565(&s_world, s_fb); write_ppm(argv[2], s_fb);
        printf("wrote %s\n", argv[2]); return 0;
    }
    world_init(&s_world, (uint32_t)SDL_GetTicks() ^ 0x9E3779B9u);
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
