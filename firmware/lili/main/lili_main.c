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
#include "gesture.h"
#include "touch_216.h"
#include "body_216.h"
#include "ui.h"
#include "battery_port.h"
#include "plan.h"
#include "life.h"
#include "save.h"
#include "brain.h"
#include "q4_model.h"
#include "freertos/queue.h"
#include "rtc_port.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <time.h>

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
static bool      s_touch;                      /* the touch chip answered */
static ui_t      *s_ui;                        /* PSRAM: the glass interface (280 KB) */
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
    ui_apply_rows(s_ui, dst, y0, n);                     /* the glass status bar and the settings card (M10) */
    uint32_t *p = (uint32_t *)dst;
    for (int i = 0, m = n * LILI_PHYS_W / 2; i < m; i++) { uint32_t v = p[i]; p[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu); }
    s_fill_us += esp_timer_get_time() - t;
}

/* the save (spec 9.7): the NVS blob "lili_save"; the clock: the RTC (local time, spec 5.6) through time() */
static int64_t s_first_start;
extern i2c_master_bus_handle_t board_i2c_bus(void);
static void lili_save(void) {
    uint8_t buf[SAVE_MAX]; const int n = save_write(&s_world, s_first_start, buf, sizeof buf); nvs_handle_t h;
    if (!n || nvs_open("lili", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "lili_save", buf, (size_t)n); nvs_commit(h); nvs_close(h);
}
static void lili_load(void) {
    if (nvs_flash_init() != ESP_OK) { nvs_flash_erase(); nvs_flash_init(); }
    const bool clock = rtc_port_init(board_i2c_bus());
    const int64_t now = (int64_t)time(NULL);
    life_init(&s_world, now); s_first_start = now;
    uint8_t buf[SAVE_MAX]; size_t n = sizeof buf; nvs_handle_t h; int64_t at = 0;
    if (nvs_open("lili", NVS_READONLY, &h) == ESP_OK) { if (nvs_get_blob(h, "lili_save", buf, &n) != ESP_OK) n = 0; nvs_close(h); } else n = 0;
    if (!n || !save_read(&s_world, buf, (int)n, &at, &s_first_start)) { ESP_LOGI(TAG, "save: none, a new start (clock %s)", clock ? "from the RTC" : "unset"); return; }
    s_world.clock_s = 0; s_world.epoch0 = at;
    offline_log_t lg; const int sc = clock && now > at ? life_offline(&s_world, now - at, &lg) : SCENE_GREET;
    s_world.epoch0 = now - (int64_t)s_world.clock_s;
    if (clock && now > at) brain_owner_back(&s_world, now - at);
    ESP_LOGI(TAG, "save: away %.1f h -> %s", clock && now > at ? (now - at) / 3600.0 : 0.0, life_scene_name(sc));
}

/* ---- the learned brain on Core 1 (spec 4.4, 10.12, M8.2): the model in its partition through an mmap, a task that
   answers one question at a time; the frame loop on Core 0 puts the question (the encoded state, the actions possible
   now) and takes the answer when it comes. Until then she idles (spec 8.7); without a model the rules answer at once ---- */
typedef struct { brain_state_t s; uint8_t mask[ACT_COUNT]; uint32_t rng; } brain_q_t;
typedef struct { decision_t d; float margin; int ok; int ms; } brain_a_t;
static QueueHandle_t s_bq, s_ba;
static bool s_llm, s_asking;
static size_t model_size(const uint8_t *h) {           /* the file's length from its header (the partition is larger) */
    int32_t c[7], gs; memcpy(c, h + 8, sizeof c); memcpy(&gs, h + 37, sizeof gs);   /* after the 7 ints: the shared flag (1 B), then the group size */
    const int64_t dim = c[0], hid = c[1], L = c[2], vocab = c[5];
    if (gs != 64 || dim <= 0 || dim > 1024 || L <= 0 || L > 16 || vocab != SCHEMA_VOCAB) return 0;
    int64_t n = 256 + (2 * L * dim + dim) * 4, w[] = { vocab * dim, L * dim * dim * 4, L * dim * hid * 3 };
    for (int i = 0; i < 3; i++) n += w[i] / 2 + w[i] / gs * 2;
    return (size_t)(n + 16);
}
static void *psram_any(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
static void brain_task(void *arg) {
    (void)arg; brain_q_t q; brain_a_t a;
    for (;;) {
        if (xQueueReceive(s_bq, &q, portMAX_DELAY) != pdTRUE) continue;
        const int64_t t0 = esp_timer_get_time();
        a.ok = brain_llm_decide_masked(&q.s, q.mask, &q.rng, &a.d, &a.margin, BRAIN_TEMP) == 0;
        a.ms = (int)((esp_timer_get_time() - t0) / 1000);
        xQueueSend(s_ba, &a, portMAX_DELAY);
    }
}
static void llm_init(void) {
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "model");
    uint8_t head[64];
    if (!p || esp_partition_read(p, 0, head, sizeof head) != ESP_OK) { ESP_LOGI(TAG, "brain: no model partition, the rules"); return; }
    const size_t len = model_size(head);
    if (!len || len > p->size) { ESP_LOGI(TAG, "brain: no Lili model in the partition (tools/flash_216.py model), the rules"); return; }
    const void *map; esp_partition_mmap_handle_t h;
    if (esp_partition_mmap(p, 0, len, ESP_PARTITION_MMAP_DATA, &map, &h) != ESP_OK) { ESP_LOGW(TAG, "brain: model mmap failed"); return; }
    if (!brain_llm_load(map, len, psram_any)) { ESP_LOGW(TAG, "brain: model refused (%s), the rules", brain_llm_status()); return; }
    s_bq = xQueueCreate(1, sizeof(brain_q_t)); s_ba = xQueueCreate(1, sizeof(brain_a_t));
    if (!s_bq || !s_ba || xTaskCreatePinnedToCore(brain_task, "brain", 8192, NULL, 4, NULL, 1) != pdPASS) { ESP_LOGW(TAG, "brain: no task"); return; }
    s_llm = true;
    ESP_LOGI(TAG, "brain: model %u B (%s) on Core 1", (unsigned)len, brain_llm_status());
}
/* the frame loop's half: put the question when it is due, apply the answer when it is back */
static void llm_tick(float dt) {
    brain_a_t a;
    if (s_asking && xQueueReceive(s_ba, &a, 0) == pdTRUE) {
        s_asking = false;
        if (a.ok) {
            brain_apply(&s_world, &a.d, a.margin, BRAIN_RULE_LLM);
            ESP_LOGI(TAG, "brain: model -> %s %s %s (margin %.2f, %d ms)", ACTION_NAME(a.d.a[0]), ACTION_NAME(a.d.a[1]), ACTION_NAME(a.d.a[2]), a.margin, a.ms);
        }
    }
    if (!s_asking && brain_due(&s_world, dt)) {
        brain_q_t q; brain_encode(&s_world, 0, &q.s); brain_env_mask(&s_world, 0, q.mask); q.rng = world_rand(&s_world);
        if (xQueueSend(s_bq, &q, 0) == pdTRUE) { s_asking = true; brain_asked(&s_world); }
    }
}

static void keys_init(void) {
    gpio_config_t pu = { .pin_bit_mask = (1ULL << PIN_BOOT) | (1ULL << PIN_KEY3), .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&pu);
}
static void keys_poll(void) {                  /* a press = one event on the falling edge */
    static int prev_boot = 1, prev_key3 = 1;
    int boot = gpio_get_level(PIN_BOOT), key3 = gpio_get_level(PIN_KEY3);
    if (prev_key3 && !key3) { demo_stop(); ev_push(&s_q, EV_SWITCH_VIEW, 0, 0); }   /* the tour's spots belong to its own view */
    if (prev_boot && !boot) { ui_show(s_ui); body_owner(&s_world); }     /* BOOT: the glass status bar (spec 5.4, 7.8); the demo: the console's `demo` */
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
    s_touch = touch216_init();
    lili_load();
    llm_init();
    body_init();
    s_ui = psram(sizeof *s_ui); if (s_ui) ui_init(s_ui);
    battery_port_init(board_i2c_bus());
    if (s_ui) ui_show(s_ui);                               /* power-on: the bar for 4 s */
    /* M6.4: from power-on she lives by her own brain; BOOT starts (and stops) the demo tour */
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
        if (s_touch) {                                  /* the finger, through the gestures (M4.3) */
            static gesture_t g; gesture_ctx_t gc; int tx, ty;
            world_gesture_ctx(&s_world, &gc);
            const bool down = touch216_read(&tx, &ty);
            gesture_feed(&g, &gc, down, tx, ty, (uint32_t)(t0 / 1000), &s_q);
            world_finger(&s_world, down, tx, ty);
        }
        const bool screen = body_poll(&s_world, &s_q, dt);      /* the mic, the IMU, the PWR key, the console, the energy states */
        ev_t ev; while (ev_pop(&s_q, &ev)) {
            ESP_LOGI(TAG, "event %s %d %d %d", ev_name((ev_type_t)ev.type), ev.a, ev.b, ev.c);
            if (ev.type == EV_TAP && ui_tap(s_ui, &s_world, ev.a, ev.b)) continue;   /* the interface's tap, not the world's */
            world_handle_event(&s_world, &ev);
        }
        ui_tick(s_ui, dt); body_set_brightness(ui_brightness(&s_world));
        { static int64_t bat_t; if (t0 - bat_t > 2000000) { bat_t = t0; float f; bool ch; s_ui->has_battery = battery_port_read(&f, &ch); s_ui->battery = f; s_ui->charging = ch; } }
        if (demo_active()) demo_tick(&s_world, dt);
        else {
            if (s_llm) llm_tick(dt); else brain_tick(&s_world, dt);
            plan_tick(&s_world, 0, dt);
            if (!s_llm && s_world.ask_t == 0 && s_world.rule >= 0)              /* a new decision: the serial log shows what she chose */
                ESP_LOGI(TAG, "brain: %s -> %s %s %s (margin %.2f)", s_world.rule == BRAIN_RULE_LLM ? "model" : BRAIN_RULES[s_world.rule].id, ACTION_NAME(s_world.plans[0].d.a[0]),
                         ACTION_NAME(s_world.plans[0].d.a[1]), ACTION_NAME(s_world.plans[0].d.a[2]), s_world.margin);
        }
        { static float save_t; save_t += dt; if (save_t > 300.0f && !demo_active()) { save_t = 0; lili_save(); } }   /* every 5 minutes (spec 9.7) */
        world_tick(&s_world, dt);
        int64_t t1 = esp_timer_get_time();
        if (!screen) { vTaskDelay(pdMS_TO_TICKS(60)); continue; }   /* the screen asleep: the world lives on, nothing is drawn */
        world_render_cached(&s_world, s_r8, s_bg);
        ui_prepare(s_ui, &s_world, s_r8);
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
