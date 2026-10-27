/* body_216.c - the panel's senses and energy (M9.3, M5.6, M10.7): the microphone and the whistle detector (the diag
 * firmware's, M1.6: 24/24 whistles, 0 false), a voice-activity line, the accelerometer through common/lili/motion.c
 * (shake, pickup, still, carried), the PWR key, the energy states and a serial console (settime, status, brain, demo).
 * Energy (spec 5.7): ACTIVE -> 60 s without the owner -> DIM -> 10 more minutes on the desk -> OFF (the screen
 * asleep, the world still living at a slower beat); carried in a pocket for 5 s -> OFF at once; a finger, a key,
 * a pickup, a whistle wake her; after more than 15 minutes asleep she greets (R6). */
#include "body_216.h"
#include "board_pins.h"
#include "display_port.h"
#include "mic_es7210.h"
#include "whistle.h"
#include "imu_qmi.h"
#include "rtc_pcf.h"
#include "motion.h"
#include "plan.h"
#include "life.h"
#include "brain.h"
#include "lili_demo.h"
#include "driver/gpio.h"
#include "nvs.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "body";
extern i2c_master_bus_handle_t board_i2c_bus(void);
static bool s_mic, s_imu, s_con;
static motion_t s_motion;
static int s_whistles;
static int64_t s_owner_us, s_off_since_us;
static uint8_t s_bright_active, s_state = BODY_ACTIVE;
#define DIM_AFTER_US     (60LL * 1000000)
#define OFF_AFTER_US     (11LL * 60 * 1000000)
#define POCKET_OFF_US    (5LL * 1000000)
#define DIM_LEVEL        48
#define VOICE_DB         (-38.0f)        /* the frame's level a voice must pass (tune on the panel [G]) */

static void audio_task(void *arg) {
    (void)arg; static int16_t l[WH_N], r[WH_N];
    for (;;) { if (mic_read(l, r, WH_N, 200) == WH_N) whistle_frame(l, r); else vTaskDelay(1); }
}

/* the summer time (spec 5.6): the RTC keeps the local time, so at the change (the last Sunday of March / October) it
   is an hour off; NVS "dst" remembers which regime its hours are in, and the clock is moved once when the date has
   crossed into the other (checked at power-on and every minute; settime marks the regime it was set in) */
static uint8_t s_dst = 0xFF;
static void dst_store(uint8_t v) {
    nvs_handle_t h; s_dst = v;
    if (nvs_open("lili", NVS_READWRITE, &h) == ESP_OK) { nvs_set_u8(h, "dst", v); nvs_commit(h); nvs_close(h); }
}
static void dst_check(world_t *w) {
    int64_t t = (int64_t)time(NULL);
    if (t < 1700000000) return;                                     /* no clock yet */
    if (s_dst == 0xFF) {
        nvs_handle_t h; uint8_t v = 0xFF;
        if (nvs_open("lili", NVS_READONLY, &h) == ESP_OK) { if (nvs_get_u8(h, "dst", &v) != ESP_OK) v = 0xFF; nvs_close(h); }
        if (v == 0xFF) { dst_store(life_is_dst(t)); return; }       /* the first time: the clock is taken as right */
        s_dst = v;
    }
    int shift = 0;
    if (s_dst == 1 && !life_is_dst(t - 3600)) shift = -3600;         /* the end of summer: back an hour */
    else if (s_dst == 0 && life_is_dst(t + 3600)) shift = 3600;      /* its start: on an hour */
    if (!shift) return;
    const time_t nt = (time_t)(t + shift); struct tm tm; gmtime_r(&nt, &tm);
    pcf_rtc_write(&tm);
    if (w) w->epoch0 += shift;
    dst_store(shift > 0);
    ESP_LOGI(TAG, "summer time %s: the clock moved %+d h", shift > 0 ? "began" : "ended", shift / 3600);
}

void body_init(void) {
    i2c_master_bus_handle_t bus = board_i2c_bus();
    s_mic = bus && mic_init(bus) && mic_up();
    if (s_mic) { whistle_init(); xTaskCreatePinnedToCore(audio_task, "mic", 6144, NULL, 5, NULL, 1); }
    s_imu = bus && imu_init(bus);
    motion_init(&s_motion);
    pcf_rtc_init(bus);
    gpio_config_t pwr = { .pin_bit_mask = 1ULL << S_PIN_PWR_SENSE, .mode = GPIO_MODE_INPUT };
    gpio_config(&pwr);
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    s_con = usb_serial_jtag_driver_install(&cfg) == ESP_OK;
    s_bright_active = display_port_brightness() ? display_port_brightness() : 200;
    s_owner_us = esp_timer_get_time();
    ESP_LOGI(TAG, "mic %s, imu %s, console %s (type help)", s_mic ? "on" : "none", s_imu ? "on" : "none", s_con ? "ready" : "off");
}

int body_state(void) { return s_state; }
void body_set_brightness(uint8_t level) { if (level != s_bright_active) { s_bright_active = level; if (s_state == BODY_ACTIVE) display_port_set_brightness(level); } }

static void set_state(int st, world_t *w) {
    if (st == s_state) return;
    if (st == BODY_ACTIVE) {
        if (s_state == BODY_OFF) {
            display_port_wake();
            if (esp_timer_get_time() - s_off_since_us > 15LL * 60 * 1000000) reflex_greet(w);   /* back after a while: R6 */
        }
        display_port_set_brightness(s_bright_active);
    } else if (st == BODY_DIM) display_port_set_brightness(DIM_LEVEL);
    else { display_port_sleep(); s_off_since_us = esp_timer_get_time(); }
    ESP_LOGI(TAG, "energy: %s", st == BODY_ACTIVE ? "ACTIVE" : st == BODY_DIM ? "DIM" : "OFF");
    s_state = (uint8_t)st;
}
void body_owner(world_t *w) { s_owner_us = esp_timer_get_time(); set_state(BODY_ACTIVE, w); }

static void console(world_t *w, char *line) {
    char *cmd = strtok(line, " "), *arg = strtok(NULL, "");
    if (!cmd) return;
    if (!strcmp(cmd, "help")) ESP_LOGI(TAG, "help | time | settime YYYY-MM-DDTHH:MM:SS | status | brain rules|model | demo | whistle | shake | wake");
    else if (!strcmp(cmd, "time")) { lili_date_t d; life_date(world_now(w), &d); ESP_LOGI(TAG, "world %04d-%02d-%02d %02d:%02d:%02d", d.year, d.month, d.day, d.hour, d.minute, d.second); pcf_rtc_log(); }
    else if (!strcmp(cmd, "settime") && arg) {                        /* spec 5.6 (b), D-11: the RTC and the world's clock */
        if (pcf_rtc_set_iso(arg)) { w->epoch0 = (int64_t)time(NULL) - (int64_t)w->clock_s; w->clock_ok = true; dst_store(life_is_dst((int64_t)time(NULL))); ESP_LOGI(TAG, "time set: %s", arg); }
        else ESP_LOGW(TAG, "settime: YYYY-MM-DDTHH:MM:SS");
    } else if (!strcmp(cmd, "status")) {
        char p[64]; plan_describe(w, 0, p, sizeof p);
        ESP_LOGI(TAG, "hunger %.1f energy %.1f happy %.1f bored %.1f lonely %.1f bladder %.1f guilty %d spots %d pad %d food %d | %s | %s",
                 w->hunger, w->energy, w->happy, w->bored, w->lonely, w->bladder, w->things.guilty, w->things.n_mess, w->things.pad, w->things.food, p, brain_llm_status());
    } else if (!strcmp(cmd, "brain") && arg) { brain_use_llm(!strcmp(arg, "model")); ESP_LOGI(TAG, "brain: %s", !strcmp(arg, "model") ? "the model (if loaded)" : "the rules"); }
    else if (!strcmp(cmd, "demo")) { if (demo_active()) demo_stop(); else demo_start(); }
    else if (!strcmp(cmd, "whistle")) { ev_t e = { EV_WHISTLE, 0, 0, 0 }; world_handle_event(w, &e); body_owner(w); }
    else if (!strcmp(cmd, "shake")) { ev_t e = { EV_SHAKE, 0, 0, 0 }; world_handle_event(w, &e); }
    else if (!strcmp(cmd, "wake")) body_owner(w);
    else ESP_LOGI(TAG, "? %s (help)", cmd);
}

bool body_poll(world_t *w, ev_queue_t *q, float dt) {
    (void)dt;
    const int64_t now = esp_timer_get_time();
    /* the whistle (R2) and a voice (R8) */
    if (s_mic) {
        whistle_state_t ws; whistle_get(&ws);
        if (ws.count > s_whistles) ev_push(q, EV_WHISTLE, 0, 0);    /* a new whistle since the last frame */
        s_whistles = ws.count;
        static int loud; static int64_t last_voice;
        loud = ws.rms_db_l > VOICE_DB && !ws.tone ? loud + 1 : 0;
        if (loud > 10 && now - last_voice > 8000000) { ev_push(q, EV_VOICE_ACTIVITY, 0, 0); last_voice = now; loud = 0; }
    }
    /* the body: shake, pickup, still or carried */
    if (s_imu) {
        int16_t a[3];
        if (imu_read(a)) { const float mg[3] = { a[0] / 16.384f, a[1] / 16.384f, a[2] / 16.384f }; motion_feed(&s_motion, mg, (uint32_t)(now / 1000), q); }
    }
    /* the PWR key: a press puts the screen to sleep or wakes it */
    static bool pwr_prev;
    const bool pwr = gpio_get_level(S_PIN_PWR_SENSE);
    if (pwr && !pwr_prev) { if (s_state == BODY_OFF) body_owner(w); else set_state(BODY_OFF, w); }
    pwr_prev = pwr;
    /* the console */
    if (s_con) {
        static char line[96]; static int len; uint8_t buf[32]; int n;
        while ((n = usb_serial_jtag_read_bytes(buf, sizeof buf, 0)) > 0)
            for (int i = 0; i < n; i++) {
                const char ch = (char)buf[i];
                if (ch == '\n' || ch == '\r') { if (len) { line[len] = 0; console(w, line); } len = 0; }
                else if (len < (int)sizeof line - 1) line[len++] = ch;
            }
    }
    { static int64_t dst_t; if (now - dst_t > 60000000) { dst_t = now; dst_check(w); } }   /* the summer time, once a minute */
    /* the energy states */
    for (int i = 0; i < q->count; i++) {                               /* an owner's event wakes and keeps her up */
        const uint8_t t = q->q[(q->head + i) % EV_QUEUE_LEN].type;
        if ((t != EV_NONE && t < EV_VOICE_ACTIVITY) || t == EV_SWITCH_VIEW) { body_owner(w); break; }
    }
    if (s_motion.state == MOTION_IN_MOTION && now - s_motion.move_since * 1000LL > POCKET_OFF_US && s_state != BODY_OFF) set_state(BODY_OFF, w);   /* a pocket */
    else if (s_state == BODY_ACTIVE && now - s_owner_us > DIM_AFTER_US) set_state(BODY_DIM, w);
    else if (s_state == BODY_DIM && now - s_owner_us > OFF_AFTER_US && s_motion.state != MOTION_HANDHELD) set_state(BODY_OFF, w);
    return s_state != BODY_OFF;
}
