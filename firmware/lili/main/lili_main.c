/* lili_main.c - Pixel Lili's world on the 2.16 panel (M2.6).
 *
 * The same common/lili core the simulator runs: world_tick, world_render into the 480 x 480 (native since M3.4)
 * byte canvas (PSRAM), then the panel's stripes are made straight into the DMA buffer:
 * 2x LUT flush, the glass mask, the byte swap. No RGB565 frame buffer exists.
 * Keys (the stand-in for the finger until M4): KEY3 = room <-> yard, BOOT = next season. */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "board_pins.h"
#include "display_port.h"
#include "lili_world.h"
#include "render8.h"
#include "palette.h"
#include "mask.h"
#include "events.h"

static const char *TAG = "lili";
#define PIN_BOOT 0
#define PIN_KEY3 S_PIN_KEY3                    /* 18, pull-up on the board, low when pressed */

static world_t   s_world;
static render8_t *s_r8;                        /* PSRAM: 57 600 B canvas + the LUT */
static mask_t    *s_mask;                      /* PSRAM: the glass's quadrant table */
static bgcache_t *s_bg;                        /* PSRAM: the static layer (floor, wall, flat props), drawn once per view */
static ev_queue_t s_q;
static int64_t   s_fill_us;                    /* time spent making stripes (this window) */

static void *psram(size_t n) { return heap_caps_aligned_alloc(64, (n + 63) & ~(size_t)63, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

/* one stripe: indices -> RGB565 at 2x, the glass mask, then the wire's byte order */
static void fill_stripe(uint16_t *dst, int y0, int n, void *ctx) {
    (void)ctx;
    int64_t t = esp_timer_get_time();
    r8_flush_rows(s_r8, dst, y0, n);
    mask_apply_rows(s_mask, dst, y0, n);
    uint32_t *p = (uint32_t *)dst;
    for (int i = 0, m = n * LILI_PHYS_W / 2; i < m; i++) { uint32_t v = p[i]; p[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu); }
    s_fill_us += esp_timer_get_time() - t;
}

static void keys_init(void) {
    gpio_config_t pu = { .pin_bit_mask = (1ULL << PIN_BOOT) | (1ULL << PIN_KEY3), .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&pu);
}
static void keys_poll(void) {                  /* a press = one event on the falling edge */
    static int prev_boot = 1, prev_key3 = 1;
    int boot = gpio_get_level(PIN_BOOT), key3 = gpio_get_level(PIN_KEY3);
    if (prev_key3 && !key3) ev_push(&s_q, EV_SWITCH_VIEW, 0, 0);
    if (prev_boot && !boot) ev_push(&s_q, EV_SET_SEASON, (s_world.season_force + 1) & 3, 0);
    prev_boot = boot; prev_key3 = key3;
}

void app_main(void) {
    ESP_LOGI(TAG, "Pixel Lili world | internal heap %u KB, PSRAM %u KB free", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }
    keys_init();
    s_r8 = psram(sizeof *s_r8); s_mask = psram(sizeof *s_mask); s_bg = psram(sizeof *s_bg);
    if (!s_r8 || !s_mask || !s_bg) { ESP_LOGE(TAG, "no PSRAM for the canvas"); return; }
    if (!display_port_init()) ESP_LOGE(TAG, "no display: the board was not told");
    uint8_t pal[256][3]; pal_master(pal); r8_set_palette(s_r8, pal);      /* M3.2: the approved master palette */
    mask_init(s_mask);
    world_init(&s_world, esp_random());
    ev_init(&s_q);
    memset(s_bg, 0, sizeof *s_bg);
    ESP_LOGI(TAG, "up: world %u B, canvas %u B, mask %u B, static-layer cache %u B | internal heap %u KB free", (unsigned)sizeof s_world, (unsigned)sizeof *s_r8, (unsigned)sizeof *s_mask, (unsigned)sizeof *s_bg,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);

    int64_t last = esp_timer_get_time(), last_log = last, tick_us = 0, render_us = 0, flush_us = 0;
    int frames = 0;
    for (;;) {
        int64_t t0 = esp_timer_get_time();
        float dt = (t0 - last) / 1e6f; last = t0;
        if (dt > 0.1f) dt = 0.1f;
        keys_poll();
        ev_t ev; while (ev_pop(&s_q, &ev)) { ESP_LOGI(TAG, "event %s %d %d", ev_name((ev_type_t)ev.type), ev.a, ev.b); world_handle_event(&s_world, &ev); }
        world_tick(&s_world, dt);
        int64_t t1 = esp_timer_get_time();
        world_render_cached(&s_world, s_r8, s_bg);
        int64_t t2 = esp_timer_get_time();
        display_port_flush_gen(fill_stripe, NULL);
        int64_t t3 = esp_timer_get_time();
        tick_us += t1 - t0; render_us += t2 - t1; flush_us += t3 - t2; frames++;
        if (t3 - last_log > 5000000) {
            int64_t wait, send; display_port_flush_prof(&wait, &send);
            ESP_LOGI(TAG, "%.1f fps | per frame: tick %lld us, render %lld us, flush %lld us (fill %lld, wire wait %lld, send %lld) | internal heap %u KB, min %u KB",
                     frames * 1e6f / (t3 - last_log), (long long)(tick_us / frames), (long long)(render_us / frames), (long long)(flush_us / frames),
                     (long long)(s_fill_us / frames), (long long)(wait / frames), (long long)(send / frames),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024, (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024);
            last_log = t3; frames = 0; tick_us = render_us = flush_us = s_fill_us = 0;
        }
        vTaskDelay(1);
    }
}
