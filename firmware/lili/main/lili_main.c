/* lili_main.c - Pixel Lili's world on the 2.16 panel (M2.6).
 *
 * The same common/lili core the simulator runs: world_tick, world_render into the 480 x 480 (native since M3.4)
 * byte canvas (PSRAM), then the panel's stripes are made straight into the DMA buffer:
 * 2x LUT flush, the glass mask, the byte swap. No RGB565 frame buffer exists.
 * The painted world (2026-10-23): the pack (assets/build/assets.pak) is read in place from flash, since 2026-10-27
 * from its own "assets" partition (spec 5.10); the demo tour (common/lili/lili_demo.c, the simulator's script) runs from power-on.
 * Keys (the stand-in for the finger until M4): BOOT = the tour on / off, KEY3 = room <-> yard (stops the tour). */
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
#include "esp_partition.h"
#include "board_pins.h"
#include "display_port.h"
#include "lili_world.h"
#include "render8.h"
#include "palette.h"
#include "mask.h"
#include "events.h"
#include "pak.h"
#include "lili_demo.h"
#include "nav.h"

static const char *TAG = "lili";
#define PIN_BOOT 0
#define PIN_KEY3 S_PIN_KEY3                    /* 18, pull-up on the board, low when pressed */
/* which way is up (the panel's MADCTL, display_port_set_madctl): the pocket-tank's 0xA0 is upright with the USB at
   the foot; the owner (2026-10-23) wants the picture turned 90 degrees anticlockwise from that, so the cable leaves
   at the side and the box lies flat. 0xC0 by the DCS reading (MV|MY = 90 anticlockwise from the panel's own, MX|MY = 180);
   if a controller reads the bits the other way round, 0x00 is that turn */
#define LILI_MADCTL 0xC0

static world_t   s_world;
static render8_t *s_r8;                        /* PSRAM: 57 600 B canvas + the LUT */
static mask_t    *s_mask;                      /* PSRAM: the glass's quadrant table */
static bgcache_t *s_bg;                        /* PSRAM: the static layer (floor, wall, flat props), drawn once per view */
static ev_queue_t s_q;
static pak_t     s_pak;                        /* the pack, in place in its flash partition (through the cache) */
static nav_scratch_t *s_nav;                   /* PSRAM: the path search (nav.h, ~200 KB) */
static int64_t   s_fill_us;                    /* time spent making stripes (this window) */

static void *psram(size_t n) { return heap_caps_aligned_alloc(64, (n + 63) & ~(size_t)63, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

/* the pack's partition mapped into the data address space (the MMU's 64 KB pages): read like RAM, held in none.
   Only the pack's own length is mapped (LPAK header: total_size at byte 12); an empty partition (0xFF, never
   flashed) or a foreign one gives NULL, and the world draws its placeholder shapes (docs/decisions.md, 2026-10-27) */
static const uint8_t *pak_map(uint32_t *len) {
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "assets");
    uint8_t head[20];
    if (!p || esp_partition_read(p, 0, head, sizeof head) != ESP_OK || memcmp(head, "LPAK", 4)) return NULL;
    uint32_t total = (uint32_t)head[12] | (uint32_t)head[13] << 8 | (uint32_t)head[14] << 16 | (uint32_t)head[15] << 24;
    if (total < sizeof head || total > p->size) return NULL;
    const void *map; esp_partition_mmap_handle_t h;
    if (esp_partition_mmap(p, 0, total, ESP_PARTITION_MMAP_DATA, &map, &h) != ESP_OK) return NULL;
    *len = total;
    return map;
}

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
    if (prev_key3 && !key3) { demo_stop(); ev_push(&s_q, EV_SWITCH_VIEW, 0, 0); }   /* the tour's spots belong to its own view */
    if (prev_boot && !boot) { if (demo_active()) demo_stop(); else demo_start(); }
    prev_boot = boot; prev_key3 = key3;
}

void app_main(void) {
    ESP_LOGI(TAG, "Pixel Lili world | internal heap %u KB, PSRAM %u KB free", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }
    keys_init();
    s_r8 = psram(sizeof *s_r8); s_mask = psram(sizeof *s_mask); s_bg = psram(sizeof *s_bg); s_nav = psram(sizeof *s_nav);
    if (!s_r8 || !s_mask || !s_bg || !s_nav) { ESP_LOGE(TAG, "no PSRAM for the canvas"); return; }
    if (!display_port_init()) ESP_LOGE(TAG, "no display: the board was not told");
    display_port_set_madctl(LILI_MADCTL);      /* the hardware turns the picture: no cost per frame (the glass mask is the same in all four corners) */
    int64_t tp = esp_timer_get_time();
    uint32_t pak_len = 0;
    const uint8_t *pak = pak_map(&pak_len);
    if (pak && pak_open(&s_pak, pak, pak_len)) world_set_pak(&s_pak);   /* the CRC over it: once, here */
    ESP_LOGI(TAG, "pak: %u B, %d entries, %s (%lld ms)", (unsigned)pak_len, s_pak.n,
             s_pak.data ? "ok" : pak ? "REFUSED (CRC): placeholder graphics" : "none in the assets partition (tools/flash_216.py pak): placeholder graphics",
             (long long)((esp_timer_get_time() - tp) / 1000));
    uint8_t pal[256][3]; if (!pak_palette(&s_pak, pal)) pal_master(pal); r8_set_palette(s_r8, pal);   /* the painted views set their own */
    mask_init(s_mask);
    world_init(&s_world, esp_random());
    ev_init(&s_q);
    nav_init(s_nav);
    if (s_pak.data) demo_start();
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
        demo_tick(&s_world, dt);
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
