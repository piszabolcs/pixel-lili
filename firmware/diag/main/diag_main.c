/* diag_main.c - Pixel Lili diagnostic firmware for the Waveshare 2.16 (spec 12,
 * M1.4-M1.8). The pocket-tank's display / PMIC / codec ports unchanged; the
 * diag's own: ES7210 microphone + I2S, FFT + whistle detector (5.8), corner
 * test pattern (M1.7), RTC settime (M1.8), QMI8658 wake-on-motion from light
 * sleep (M1.4), the power log (M1.5). Pages: MIKROFON, SAROK, INFO,
 * FOGYASZTÁS - KEY3 or a short PWR press steps them, BOOT toggles DIM, KEY3
 * held 1.5 s = the desk sleep (wakes on a pick-up or PWR), PWR 1.5 s = off.
 * Serial console on the USB port (type help). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "display_port.h"
#include "battery_port.h"
#include "codec_port.h"
#include "board_pins.h"
#include "gfx.h"
#include "mic_es7210.h"
#include "whistle.h"
#include "imu_qmi.h"
#include "rtc_pcf.h"
#include "powerlog.h"

i2c_master_bus_handle_t board_i2c_bus(void);        /* display_port_sh8601.c owns the bus */
static const char *TAG = "diag";
#define DIAG_VERSION "0.1"
#define PIN_BOOT 0
#define PIN_KEY3 S_PIN_KEY3                            /* 18, 10k pull-up on the board, low when pressed */
#define PIN_PWR  S_PIN_PWR_SENSE                       /* 16 = SYS_OUT, high while the PWR key is down */
#define PIN_AMP  46
#define BRIGHT_ACTIVE 255
#define BRIGHT_DIM    77                               /* 30 % (spec 5.7) */

enum { SC_MIC, SC_CORNER, SC_INFO, SC_PLOG, SC_N };
static const char *SCENE_NAMES[SC_N] = { "MIKROFON", "SAROK", "INFO", "FOGYASZTÁS" };
static int s_scene = SC_MIC;
static bool s_dim;
static bool s_mic_ok, s_imu_ok, s_rtc_ok, s_pmic_ok;
static volatile bool s_mic_want, s_mic_busy;
static float s_fps;
static int64_t s_frames;
static char s_msg[120] = "";
static int s_wom_thr = 100, s_wom_blank = 8;           /* mg; samples at 21 Hz */
static bool s_wom_awake;                               /* WoM armed while awake (INFO page shows INT1) */
static int s_wom_events;
static int s_corner_pick;
/* the step-5 labelled recording: `label` arms it, BOOT starts it, the screen says what to do */
typedef struct { const char *say; int secs; } lab_phase_t;
static const lab_phase_t LAB[] = {
    /* run 2 (2026-10-19 evening): normal-volume whistles and a long podcast stretch, the two
       things run 1 lacked; labels stay 1 silence, 2 whistle, 3 speech/podcast */
    { "CSEND", 5 }, { "FÜTTY NORMÁLAN 10x", 40 }, { "PODCAST SZÓLJON", 75 },
};
#define LAB_N ((int)(sizeof LAB / sizeof LAB[0]))
static bool s_lab_armed; static int s_lab_phase = -1; static int64_t s_lab_t0;
static void lab_start(void) {
    s_lab_phase = 0; s_lab_t0 = esp_timer_get_time();
    whistle_set_log(true); whistle_set_label(1);
    ESP_LOGI("diag", "label run: started");
}
static void lab_tick(int64_t now) {
    if (s_lab_phase < 0) return;
    int64_t el = (now - s_lab_t0) / 1000000; int acc = 0, ph = -1;
    for (int i = 0; i < LAB_N; i++) { acc += LAB[i].secs; if (el < acc) { ph = i; break; } }
    if (ph < 0) { whistle_set_label(0); whistle_set_log(false); s_lab_phase = -1; s_lab_armed = false; ESP_LOGI("diag", "label run: done"); return; }
    if (ph != s_lab_phase) { s_lab_phase = ph; whistle_set_label(ph + 1); }
}
static int lab_left(int64_t now) {
    int acc = 0; for (int i = 0; i <= s_lab_phase; i++) acc += LAB[i].secs;
    return acc - (int)((now - s_lab_t0) / 1000000);
}
/* the M1.5 run: WAIT (the cable out) -> ACTIVE -> DIM -> SLEEP (PWR ends it) */
static int s_prun = -1; static int64_t s_prun_t0, s_prun_sample;
static int s_prun_active_s = 1200, s_prun_dim_s = 1200;

/* ---- the microphone task (core 1) ---- */
static void audio_task(void *arg) {
    (void)arg;
    static int16_t l[WH_N], r[WH_N];
    for (;;) {
        if (!s_mic_want) { s_mic_busy = false; vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        s_mic_busy = true;
        int n = mic_read(l, r, WH_N, 200);
        if (n == WH_N) whistle_frame(l, r); else vTaskDelay(1);
    }
}
static void mic_start(void) { if (s_mic_ok && mic_up()) s_mic_want = true; }
static void mic_stop(void) {
    s_mic_want = false;
    for (int i = 0; i < 40 && s_mic_busy; i++) vTaskDelay(pdMS_TO_TICKS(10));
    mic_down();
}

/* ---- brightness / DIM ---- */
static void set_dim(bool dim) {
    s_dim = dim;
    display_port_set_brightness(dim ? BRIGHT_DIM : BRIGHT_ACTIVE);
    ESP_LOGI(TAG, "%s: brightness %d, %s", dim ? "DIM" : "ACTIVE", dim ? BRIGHT_DIM : BRIGHT_ACTIVE, dim ? "10 fps" : "full rate");
}

/* ---- pages ---- */
static void bar(int x, int y, int w, int h, float v, float lo, float hi, uint16_t c) {
    float f = (v - lo) / (hi - lo);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    gfx_rect(x - 1, y - 1, w + 2, h + 2, C_GREY);
    gfx_fill(x, y, (int)(w * f), h, c);
}
/* the measured glass corner (M1.7, 2026-10-19): 72 physical px. A line of text near
   the bottom must stay inside the corner arcs: at y = 444..462 the arc leaves
   x >= 25 free (worst row y = 462: 72 - sqrt(72^2 - 54^2)), so a centred
   300 px footer (x = 90..390) is safe on every page. */
#define CORNER_R 72
static void footer(void) {
    const char *t = "KEY3/PWR: lap   BOOT: dim";
    gfx_text(240 - gfx_text_w(2, t) / 2, 444, 2, C_GREY, t);
}
static void draw_mic(const whistle_state_t *ws) {
    gfx_clear(C_BLACK);
    gfx_text(24, 14, 2, C_WHITE, "MIKROFON ES7210  16 kHz  FFT 512");
    if (s_prun >= 0) {
        int64_t el = (esp_timer_get_time() - s_prun_t0) / 1000000;
        int left = s_prun == PL_ACTIVE ? s_prun_active_s - (int)el : s_prun == PL_DIM ? s_prun_dim_s - (int)el : 0;
        if (s_prun == PL_WAIT) gfx_text(24, 38, 2, C_ORANGE, "MÉRÉS: HÚZD KI AZ USB-T");
        else gfx_textf(24, 38, 2, C_ORANGE, "MÉRÉS: %s  %d:%02d hátra", plog_state_name(s_prun), left / 60, left % 60);
    } else if (!s_mic_ok) gfx_text(24, 38, 2, C_RED, "NINCS ES7210");
    gfx_text(24, 62, 2, C_WHITE, "BAL");  bar(100, 62, 280, 14, ws->rms_db_l, -80, 0, C_GREEN); gfx_textf(392, 62, 2, C_WHITE, "%4.0f dB", ws->rms_db_l);
    gfx_text(24, 84, 2, C_WHITE, "JOBB"); bar(100, 84, 280, 14, ws->rms_db_r, -80, 0, C_GREEN); gfx_textf(392, 84, 2, C_WHITE, "%4.0f dB", ws->rms_db_r);
    /* the band 1000-3500 Hz: 81 bins, 5 px each */
    const int bx = 36, by = 118, bh = 150, bw = 5;
    gfx_text(bx, by - 14, 1, C_GREY, "1000-3500 Hz, -90..-10 dBFS");
    gfx_rect(bx - 2, by - 2, WH_BAND * bw + 3, bh + 4, C_DGREY);
    for (int i = 0; i < WH_BAND; i++) {
        float f = (ws->band_db[i] + 90.0f) / 80.0f;
        if (f < 0) f = 0;
        if (f > 1) f = 1;
        int h = (int)(f * bh);
        gfx_fill(bx + i * bw, by + bh - h, bw - 1, h, ws->tone ? C_YELLOW : C_CYAN);
    }
    for (int hz = 1000; hz <= 3500; hz += 500) { int i = hz * WH_N / WH_RATE - WH_BIN0; gfx_vline(bx + i * bw, by + bh + 2, 4, C_GREY); if (hz % 1000 == 0) gfx_textf(bx + i * bw - 8, by + bh + 8, 1, C_GREY, "%dk", hz / 1000); }
    /* 12 px per character at scale 2: a line from x = 24 may hold 38 characters (24 + 456 = 480) */
    gfx_textf(48, 292, 2, ws->tone ? C_YELLOW : C_WHITE, "%4.0f Hz  %4.0f dB  tónus %.2f  %d/%d", ws->peak_hz, ws->peak_db, ws->frac, ws->streak, whistle_params()->frames);
    /* the whole 0-8 kHz, coarse */
    const int fx = 48, fy = 322, fh = 56;
    gfx_text(fx, fy - 12, 1, C_GREY, "0-8 kHz");
    for (int i = 0; i < WH_FULL; i++) {
        float f = (ws->full_db[i] + 90.0f) / 80.0f;
        if (f < 0) f = 0;
        if (f > 1) f = 1;
        int h = (int)(f * fh);
        gfx_fill(fx + i * 6, fy + fh - h, 5, h, C_BLUE);
    }
    gfx_textf(24, 396, 3, C_WHITE, "FÜTTY: %d", ws->count);
    gfx_textf(260, 400, 2, C_GREY, "gain %d  thr %.0f dB", mic_gain(), whistle_params()->thr_db);
    if (ws->shown) {                                   /* the event, big, 1.5 s */
        gfx_fill(60, 150, 360, 130, C_YELLOW);
        gfx_rect(60, 150, 360, 130, C_WHITE);
        int w = gfx_text_w(10, "FÜTTY");
        gfx_text(240 - w / 2, 170, 10, C_BLACK, "FÜTTY");
    }
    if (s_lab_armed) {                                 /* the labelled recording's prompt, over the coarse spectrum */
        gfx_fill(36, 318, 408, 70, C_DGREY); gfx_rect(36, 318, 408, 70, C_ORANGE);
        if (s_lab_phase < 0) { int w = gfx_text_w(3, "NYOMD MEG: BOOT"); gfx_text(240 - w / 2, 340, 3, C_ORANGE, "NYOMD MEG: BOOT"); }
        else {
            int64_t now = esp_timer_get_time();
            int w = gfx_text_w(3, LAB[s_lab_phase].say); gfx_text(240 - w / 2, 326, 3, C_ORANGE, LAB[s_lab_phase].say);
            gfx_textf(196, 360, 2, C_WHITE, "%d/%d  %2d s", s_lab_phase + 1, LAB_N, lab_left(now));
        }
    }
    footer();
}
static void draw_corner(void) {
    gfx_clear(C_BLACK);
    /* six candidate corner radii, 8 px apart: the 2.16's glass radius is not documented
       (pocket-tank: 40 "by the maker's pictures, not measured"; a first photo said "more") */
    enum { NR = 6 };
    static const int R[NR] = { 32, 40, 48, 56, 64, 72 };
    static const uint16_t COL[NR] = { C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_CYAN, C_BLUE };
    static const char *NAME[NR] = { "PIROS", "NARANCS", "SÁRGA", "ZÖLD", "CIÁN", "KÉK" };
    for (int k = 0; k < NR; k++) {
        float r = R[k]; int b = R[k] + 2;
        gfx_ring(r, r, r, 2.0f, COL[k], 0, 0, b, b);
        gfx_ring(GFX_W - r, r, r, 2.0f, COL[k], GFX_W - b, 0, GFX_W, b);
        gfx_ring(r, GFX_H - r, r, 2.0f, COL[k], 0, GFX_H - b, b, GFX_H);
        gfx_ring(GFX_W - r, GFX_H - r, r, 2.0f, COL[k], GFX_W - b, GFX_H - b, GFX_W, GFX_H);
    }
    /* the straight edges, 1 px at the outermost row/column: does the glass show px 0 and 479? */
    gfx_hline(80, 0, GFX_W - 160, C_WHITE); gfx_hline(80, GFX_H - 1, GFX_W - 160, C_WHITE);
    gfx_vline(0, 80, GFX_H - 160, C_WHITE); gfx_vline(GFX_W - 1, 80, GFX_H - 160, C_WHITE);
    int w = gfx_text_w(3, "SAROK TESZT"); gfx_text(240 - w / 2, 96, 3, C_WHITE, "SAROK TESZT");
    for (int k = 0; k < NR; k++) gfx_textf(150, 140 + k * 30, 3, COL[k], "%d px  %s", R[k], NAME[k]);
    w = gfx_text_w(2, "MELYIK ÍV SIMUL A KÁVA SARKÁHOZ?"); gfx_text(240 - w / 2, 330, 2, C_WHITE, "MELYIK ÍV SIMUL A KÁVA SARKÁHOZ?");
    w = gfx_text_w(2, "(a legkisebb, ami még teljesen látszik)"); gfx_text(240 - w / 2, 354, 2, C_GREY, "(a legkisebb, ami még teljesen látszik)");
    if (s_corner_pick) { char b[40]; snprintf(b, sizeof b, "VÁLASZTOTT: %d px", s_corner_pick); w = gfx_text_w(3, b); gfx_text(240 - w / 2, 384, 3, C_GREEN, b); }
    footer();
}
static void draw_info(const whistle_state_t *ws) {
    gfx_clear(C_BLACK);
    int y = 14; const int dy = 24;
    int64_t up = esp_timer_get_time() / 1000000;
    gfx_textf(24, y, 2, C_WHITE, "PIXEL LILI DIAG v%s   üzem %lld:%02lld:%02lld", DIAG_VERSION, up / 3600, (up / 60) % 60, up % 60); y += dy;
    struct tm t; bool valid = false;
    if (s_rtc_ok && pcf_rtc_read(&t, &valid)) gfx_textf(24, y, 2, valid ? C_WHITE : C_RED, "RTC  %04d-%02d-%02d %02d:%02d:%02d  %s", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, valid ? "érvényes" : "ÉRVÉNYTELEN");
    else gfx_text(24, y, 2, C_RED, "RTC  nincs");
    y += dy;
    float frac = 0; bool chg = false; bool bat = battery_port_read(&frac, &chg); int st = battery_port_state();
    gfx_textf(24, y, 2, C_WHITE, "AKKU %3d%%  %4d mV  %s  töltőáram %d mA", bat ? (int)(frac * 100 + 0.5f) : -1, battery_port_vbat_mv(),
              st == BAT_CHARGING ? "TÖLT" : st == BAT_FULL ? "USB, tele" : st == BAT_PLUGGED ? "USB" : "akkuról", battery_port_charge_ma()); y += dy;
    int16_t a[3] = { 0, 0, 0 }; if (s_imu_ok) imu_read(a);
    gfx_textf(24, y, 2, C_WHITE, "IMU g=[%6d %6d %6d]  INT1=%d INT2=%d", a[0], a[1], a[2], gpio_get_level(IMU_INT1_GPIO), gpio_get_level(IMU_INT2_GPIO)); y += dy;
    gfx_textf(24, y, 2, s_wom_awake ? C_YELLOW : C_GREY, "WoM %s  küszöb %d mg  blank %d  esemény %d", s_wom_awake ? "ÉLES" : "ki", s_wom_thr, s_wom_blank, s_wom_events); y += dy;
    gfx_textf(24, y, 2, C_WHITE, "HEAP belső %u KB (max blokk %u)  PSRAM %u KB", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
              (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024); y += dy;
    gfx_textf(24, y, 2, C_WHITE, "FPS %4.1f  fényerő %d  %s  képkocka %lld", s_fps, display_port_brightness(), s_dim ? "DIM" : "ACTIVE", (long long)s_frames); y += dy;
    gfx_textf(24, y, 2, C_WHITE, "MIC %s  bal %4.0f  jobb %4.0f dBFS  fütty %d", s_mic_ok ? (mic_is_up() ? "fut" : "áll") : "NINCS", ws->rms_db_l, ws->rms_db_r, ws->count); y += dy;
    gfx_textf(24, y, 2, C_WHITE, "GOMB  BOOT=%d  KEY3=%d  PWR_SENSE=%d", gpio_get_level(PIN_BOOT), gpio_get_level(PIN_KEY3), gpio_get_level(PIN_PWR)); y += dy;
    gfx_textf(24, y, 2, C_WHITE, "PMIC %s  ES8311 %s  mérés %s", s_pmic_ok ? "ok" : "NINCS", codec_port_present() ? "le" : "nincs", s_prun >= 0 ? plog_state_name(s_prun) : "nincs"); y += dy;
    gfx_textf(24, y, 2, C_WHITE, "napló sorok %d  sarok %d px", plog_count(), s_corner_pick); y += dy;
    y += 8;
    gfx_text(24, y, 2, C_YELLOW, s_msg[0] ? s_msg : "(még nem volt alvás)"); y += dy;
    footer();
}
static void draw_plog(void) {
    gfx_clear(C_BLACK);
    gfx_text(24, 14, 2, C_WHITE, "FOGYASZTÁS NAPLÓ (1% = 10 mAh)");
    int n = plog_count(), first = n > 16 ? n - 16 : 0, y = 40;
    const plog_row_t *prev = first > 0 ? plog_row(first - 1) : NULL;
    for (int i = first; i < n; i++) {
        const plog_row_t *r = plog_row(i);
        time_t tt = r->t; struct tm tm; localtime_r(&tt, &tm);
        char cur[16] = "";
        if (prev && r->t > prev->t && !(prev->st & 0x80) && !(r->st & 0x80) && r->pct != 255 && prev->pct != 255)
            snprintf(cur, sizeof cur, "%5.1f mA", ((int)prev->pct - (int)r->pct) * 10.0 / ((r->t - prev->t) / 3600.0));
        gfx_textf(24, y, 2, (r->st & 0x80) ? C_GREY : C_WHITE, "%02d:%02d %3d%% %4dmV %-6s %-6s %s", tm.tm_hour, tm.tm_min, r->pct, r->mv, plog_state_name(r->st), r->why, (r->st & 0x80) ? "USB" : cur);
        y += 24; prev = r;
    }
    if (!n) gfx_text(24, 40, 2, C_GREY, "üres - soros: power run");
    footer();
}

/* ---- the desk sleep (M1.4 / M1.5): panel off, mic off, light sleep; wakes
 * on the PWR key (GPIO 16 high), the IMU's INT1 (GPIO 17 high, WoM) and a
 * sampling timer that writes a power-log row and sleeps on ---- */
static void do_sleep(bool wom, int sample_s, int timeout_s, bool stay_on_wom, const char *why) {
    ESP_LOGI(TAG, "sleep (%s): panel off, mic off, WoM %s (%d mg, blank %d), a row every %d s, timeout %d s, %s", why,
             wom && s_imu_ok ? "armed" : "off", s_wom_thr, s_wom_blank, sample_s, timeout_s, stay_on_wom ? "a motion wake logs and sleeps on" : "a motion wake ends it");
    plog_add(PL_SLEEP, why);
    mic_stop();
    display_port_sleep();
    bool armed = false;
    int idle = 0;                                                         /* INT1's level at rest: a wake is the other level */
    if (wom && s_imu_ok) {
        /* spec 5.7: the desk sleep starts on a panel at rest. 1.5 s with every axis within
           ~24 mg of the previous sample (400 counts at 16384 / g), at most 15 s of waiting. */
        int still = 0; int16_t prev[3] = { 0 }, a[3]; bool have = false; int64_t w0 = esp_timer_get_time();
        while (still < 30 && esp_timer_get_time() - w0 < 15000000) {
            if (imu_read(a)) {
                bool calm = have && abs(a[0] - prev[0]) < 400 && abs(a[1] - prev[1]) < 400 && abs(a[2] - prev[2]) < 400;
                still = calm ? still + 1 : 0; memcpy(prev, a, sizeof prev); have = true;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        ESP_LOGI(TAG, "at rest: %s after %.1f s", still >= 30 ? "yes" : "NO (sleeping anyway)", (esp_timer_get_time() - w0) / 1e6f);
        armed = imu_wom_arm(s_wom_thr, s_wom_blank);
        vTaskDelay(pdMS_TO_TICKS(400));                                   /* the blanking samples at 21 Hz */
        imu_status1();                                                    /* resets the line to its initial value (datasheet 12.4) */
        vTaskDelay(pdMS_TO_TICKS(20));
        /* the datasheet: "for each WoM event, the state of the selected interrupt line is toggled".
           The 2026-10-19 test saw INT1 stay high after a STATUS1 read, and a high-level wake fired
           at once. So: whatever level INT1 rests at, the wake is armed on the other one. */
        idle = gpio_get_level(IMU_INT1_GPIO);
        ESP_LOGI(TAG, "INT1 (GPIO %d) rests at %d: the wake is armed on %d", IMU_INT1_GPIO, idle, !idle);
    } else if (s_imu_ok) imu_sleep();
    while (gpio_get_level(PIN_PWR)) vTaskDelay(pdMS_TO_TICKS(10));        /* a level wake is never armed held */
    vTaskDelay(pdMS_TO_TICKS(30));
    while (battery_port_key_poll()) { }
    int64_t t0 = esp_timer_get_time(), last_sample = t0; int woms = 0, ticks = 0; const char *reason = "?";
    for (;;) {
        int64_t now = esp_timer_get_time();
        int64_t left_timeout = timeout_s > 0 ? (int64_t)timeout_s * 1000000 - (now - t0) : INT64_MAX;
        if (left_timeout <= 0) { reason = "timeout"; break; }
        int64_t left_sample = (int64_t)sample_s * 1000000 - (now - last_sample);
        if (left_sample < 1000) left_sample = 1000;
        int64_t slice = left_timeout < left_sample ? left_timeout : left_sample;
        gpio_wakeup_enable(PIN_PWR, GPIO_INTR_HIGH_LEVEL);
        if (armed) gpio_wakeup_enable(IMU_INT1_GPIO, idle ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        esp_sleep_enable_timer_wakeup(slice);
        esp_light_sleep_start();
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        gpio_wakeup_disable(PIN_PWR);
        if (armed) gpio_wakeup_disable(IMU_INT1_GPIO);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);           /* the sources are sticky (the pocket-tank's lesson) */
        if (cause == ESP_SLEEP_WAKEUP_GPIO) {
            bool int1 = armed && gpio_get_level(IMU_INT1_GPIO) != idle;
            if (int1) {
                woms++;
                uint8_t s1 = imu_status1(); int16_t a[3] = { 0, 0, 0 }; imu_read(a);
                ESP_LOGI(TAG, "WoM wake #%d after %.1f s: STATUS1 %02x, g=[%d %d %d], INT1 now %d", woms, (esp_timer_get_time() - t0) / 1e6f, s1, a[0], a[1], a[2], gpio_get_level(IMU_INT1_GPIO));
                if (woms <= 40) plog_add(PL_SLEEP, "wom");
                if (!stay_on_wom) { reason = "wom"; break; }
                vTaskDelay(pdMS_TO_TICKS(200));                     /* sleeping on: the new rest level is whatever INT1 settles at */
                imu_status1(); vTaskDelay(pdMS_TO_TICKS(20));
                idle = gpio_get_level(IMU_INT1_GPIO);
                continue;
            }
            if (gpio_get_level(PIN_PWR) || battery_port_key_poll()) { reason = "pwr"; break; }
            continue;
        }
        if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            if (esp_timer_get_time() - last_sample >= (int64_t)sample_s * 1000000 - 5000) { last_sample = esp_timer_get_time(); ticks++; plog_add(PL_SLEEP, "tick"); }
            continue;
        }
        ESP_LOGW(TAG, "woke for another reason (%d): sleeping on", (int)cause);
    }
    float slept = (esp_timer_get_time() - t0) / 1e6f;
    if (s_imu_ok) imu_wom_disarm();
    s_wom_awake = false;
    display_port_wake();
    display_port_set_brightness(s_dim ? BRIGHT_DIM : BRIGHT_ACTIVE);
    while (gpio_get_level(PIN_PWR)) vTaskDelay(pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(80));
    while (battery_port_key_poll()) { }
    mic_start();
    plog_add(s_dim ? PL_DIM : PL_ACTIVE, reason);
    snprintf(s_msg, sizeof s_msg, "ÉBREDÉS: %s, %.0f s alvás után (WoM %d, minta %d)",
             !strcmp(reason, "wom") ? "MOZGÁS (IMU INT1)" : !strcmp(reason, "pwr") ? "PWR GOMB" : "IDŐZÍTŐ", slept, woms, ticks);
    ESP_LOGI(TAG, "wake: %s after %.1f s (WoM wakes %d, sample rows %d) | battery %d mV", reason, slept, woms, ticks, battery_port_vbat_mv());
}
static void power_off(void) {
    ESP_LOGI(TAG, "power off: the PMIC's soft cut (the PWR key boots it)");
    plog_add(PL_OFF, "off");
    mic_stop();
    display_port_sleep();
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!battery_port_poweroff()) ESP_LOGE(TAG, "no PMIC: cannot power off");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGW(TAG, "still powered after the soft cut (USB?): restarting instead");
    esp_restart();
}
static void prun_tick(void) {
    if (s_prun < 0) return;
    int64_t now = esp_timer_get_time(); int64_t el = (now - s_prun_t0) / 1000000;
    if (s_prun == PL_WAIT) {
        float f; bool c; battery_port_read(&f, &c);
        if (battery_port_state() == BAT_ON_BATTERY) { s_prun = PL_ACTIVE; s_prun_t0 = s_prun_sample = now; set_dim(false); plog_add(PL_ACTIVE, "start"); ESP_LOGI(TAG, "power run: the cable is out - ACTIVE for %d s", s_prun_active_s); }
        return;
    }
    if (now - s_prun_sample >= 300LL * 1000000) { s_prun_sample = now; plog_add(s_prun, "5min"); }
    if (s_prun == PL_ACTIVE && el >= s_prun_active_s) { s_prun = PL_DIM; s_prun_t0 = s_prun_sample = now; set_dim(true); plog_add(PL_DIM, "dim"); ESP_LOGI(TAG, "power run: DIM for %d s", s_prun_dim_s); return; }
    if (s_prun == PL_DIM && el >= s_prun_dim_s) {
        ESP_LOGI(TAG, "power run: SLEEP until the PWR key (a row every 30 min; a pick-up logs a wom row and sleeps on)");
        s_prun = PL_SLEEP;
        do_sleep(true, 1800, 0, true, "run");
        s_prun = -1; set_dim(false); s_scene = SC_PLOG;
        ESP_LOGI(TAG, "power run over: powerlog prints it");
    }
}

/* ---- the serial console ---- */
static bool s_con; static char s_line[96]; static int s_len;
static void help(void) {
    ESP_LOGI(TAG, "commands: help | scene <0-3> (mic, sarok, info, naplo) | bright <0-255> | dim on|off | time | settime YYYY-MM-DDTHH:MM:SS | bat | pmic | imu | "
                  "wom on|off [thr_mg] [blank] (arm the WoM awake: INFO shows INT1) | womsleep [timeout_s] (desk sleep: a pick-up or PWR wakes) | sleep [sample_s] (sleep, PWR wakes; WoM off) | "
                  "power run [active_min] [dim_min] | power stop | powerlog [clear] | mic gain <0-14> | mic regs | mic on|off | whistle [thr_db frames tol floor_db] | corner <px> | fps | restart | poweroff");
}
static void run(char *line) {
    char *argv[8]; int argc = 0;
    for (char *p = strtok(line, " \t"); p && argc < 8; p = strtok(NULL, " \t")) argv[argc++] = p;
    if (!argc) return;
    const char *c = argv[0];
    if (!strcmp(c, "help")) help();
    else if (!strcmp(c, "scene") && argc > 1) {
        int n = atoi(argv[1]);
        if (n >= 0 && n < SC_N) s_scene = n;
        ESP_LOGI(TAG, "scene %d %s", s_scene, SCENE_NAMES[s_scene]);
    }
    else if (!strcmp(c, "bright") && argc > 1) { display_port_set_brightness((uint8_t)atoi(argv[1])); ESP_LOGI(TAG, "brightness %d", display_port_brightness()); }
    else if (!strcmp(c, "dim")) set_dim(argc > 1 ? !strcmp(argv[1], "on") : !s_dim);
    else if (!strcmp(c, "label")) { s_lab_armed = true; s_scene = SC_MIC; if (s_dim) set_dim(false); ESP_LOGI(TAG, "label run: armed, BOOT starts it"); }
    else if (!strcmp(c, "time")) pcf_rtc_log();
    else if (!strcmp(c, "settime") && argc > 1) { char buf[40]; snprintf(buf, sizeof buf, "%s%s%s", argv[1], argc > 2 ? " " : "", argc > 2 ? argv[2] : ""); pcf_rtc_set_iso(buf); }
    else if (!strcmp(c, "bat")) { float f; bool ch; bool ok = battery_port_read(&f, &ch); ESP_LOGI(TAG, "battery %s: %d%% %d mV, %s, charge current %d mA", ok ? "ok" : "n/a", (int)(f * 100 + 0.5f), battery_port_vbat_mv(),
                                   battery_port_state() == BAT_CHARGING ? "charging" : battery_port_state() == BAT_ON_BATTERY ? "on battery" : "USB", battery_port_charge_ma()); battery_port_charge_report(); }
    else if (!strcmp(c, "pmic")) battery_port_dump();
    else if (!strcmp(c, "imu")) { int16_t a[3] = {0}; bool ok = imu_read(a); ESP_LOGI(TAG, "imu %s: g=[%d %d %d] INT1=%d INT2=%d STATUSINT %02x STATUS1 %02x", ok ? "ok" : "FAILED", a[0], a[1], a[2], gpio_get_level(IMU_INT1_GPIO), gpio_get_level(IMU_INT2_GPIO), imu_statusint(), imu_status1()); }
    else if (!strcmp(c, "wom")) {
        if (argc > 2) s_wom_thr = atoi(argv[2]);
        if (argc > 3) s_wom_blank = atoi(argv[3]);
        if (argc > 1 && !strcmp(argv[1], "off")) { imu_wom_disarm(); s_wom_awake = false; ESP_LOGI(TAG, "WoM off, live mode"); }
        else { s_wom_awake = imu_wom_arm(s_wom_thr, s_wom_blank); s_wom_events = 0; }
    }
    else if (!strcmp(c, "womsleep")) { s_scene = SC_INFO; do_sleep(true, 600, argc > 1 ? atoi(argv[1]) : 0, false, "womtest"); }
    else if (!strcmp(c, "sleep")) { s_scene = SC_INFO; do_sleep(false, argc > 1 ? atoi(argv[1]) : 1800, 0, false, "sleep"); }
    else if (!strcmp(c, "power") && argc > 1) {
        if (!strcmp(argv[1], "run")) {
            if (argc > 2) s_prun_active_s = atoi(argv[2]) * 60;
            if (argc > 3) s_prun_dim_s = atoi(argv[3]) * 60;
            s_prun = PL_WAIT; s_prun_t0 = esp_timer_get_time(); s_scene = SC_MIC; set_dim(false);
            plog_add(PL_WAIT, "wait");
            ESP_LOGI(TAG, "power run: waiting for the cable to come out, then ACTIVE %d s, DIM %d s, then sleep until the PWR key", s_prun_active_s, s_prun_dim_s);
        } else if (!strcmp(argv[1], "stop")) { s_prun = -1; set_dim(false); ESP_LOGI(TAG, "power run stopped"); }
        else if (!strcmp(argv[1], "active")) { set_dim(false); plog_add(PL_ACTIVE, "manual"); }
        else if (!strcmp(argv[1], "dim")) { set_dim(true); plog_add(PL_DIM, "manual"); }
    }
    else if (!strcmp(c, "powerlog")) { if (argc > 1 && !strcmp(argv[1], "clear")) { plog_clear(); ESP_LOGI(TAG, "power log cleared"); } else plog_print(); }
    else if (!strcmp(c, "mic") && argc > 1) {
        if (!strcmp(argv[1], "gain") && argc > 2) mic_set_gain(atoi(argv[2]));
        else if (!strcmp(argv[1], "regs")) mic_dump();
        else if (!strcmp(argv[1], "on")) mic_start();
        else if (!strcmp(argv[1], "off")) mic_stop();
    }
    else if (!strcmp(c, "whistle")) {
        whistle_params_t *p = whistle_params();
        if (argc > 1) p->thr_db = atof(argv[1]);
        if (argc > 2) p->frames = atoi(argv[2]);
        if (argc > 3) p->tol = atof(argv[3]);
        if (argc > 4) p->floor_db = atof(argv[4]);
        whistle_state_t ws; whistle_get(&ws);
        ESP_LOGI(TAG, "whistle: thr %.1f dB, %d frames (%d ms), tol %.0f %%, floor %.0f dBFS | now peak %.0f Hz %.1f dBFS ratio %.1f dB streak %d | count %d, %lu frames",
                 p->thr_db, p->frames, p->frames * 32, p->tol * 100, p->floor_db, ws.peak_hz, ws.peak_db, ws.ratio_db, ws.streak, ws.count, (unsigned long)ws.frames);
    }
    else if (!strcmp(c, "corner") && argc > 1) { s_corner_pick = atoi(argv[1]); s_scene = SC_CORNER; ESP_LOGI(TAG, "corner pick %d px", s_corner_pick); }
    else if (!strcmp(c, "fps")) ESP_LOGI(TAG, "%.1f fps, %lld frames, %s", s_fps, (long long)s_frames, s_dim ? "DIM" : "ACTIVE");
    else if (!strcmp(c, "restart")) esp_restart();
    else if (!strcmp(c, "poweroff")) power_off();
    else ESP_LOGW(TAG, "unknown: %s (help)", c);
}
static void con_init(void) {
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    s_con = usb_serial_jtag_driver_install(&cfg) == ESP_OK;
    ESP_LOGI(TAG, "%s", s_con ? "console ready (type help)" : "USB serial driver failed: console off");
}
static void con_poll(void) {
    if (!s_con) return;
    uint8_t buf[32]; int n;
    while ((n = usb_serial_jtag_read_bytes(buf, sizeof buf, 0)) > 0)
        for (int i = 0; i < n; i++) {
            char ch = (char)buf[i];
            if (ch == '\n' || ch == '\r') { if (s_len) { s_line[s_len] = 0; run(s_line); } s_len = 0; }
            else if (s_len < (int)sizeof s_line - 1) s_line[s_len++] = ch;
        }
}

/* ---- keys ---- */
static void keys_init(void) {
    gpio_config_t pu = { .pin_bit_mask = (1ULL << PIN_BOOT) | (1ULL << PIN_KEY3), .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&pu);
    gpio_config_t in = { .pin_bit_mask = (1ULL << PIN_PWR) | (1ULL << IMU_INT1_GPIO) | (1ULL << IMU_INT2_GPIO), .mode = GPIO_MODE_INPUT };
    gpio_config(&in);
    gpio_config_t amp = { .pin_bit_mask = 1ULL << PIN_AMP, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&amp); gpio_set_level(PIN_AMP, 0);
}
static void keys_poll(void) {
    static bool boot_prev = true, key3_prev = true; static int64_t key3_down;
    bool boot = gpio_get_level(PIN_BOOT), key3 = gpio_get_level(PIN_KEY3);
    if (boot_prev && !boot && s_lab_armed && s_lab_phase < 0) lab_start();
    else if (boot_prev && !boot) { set_dim(!s_dim); ESP_LOGI(TAG, "BOOT pressed: %s", s_dim ? "DIM" : "ACTIVE"); }
    if (key3_prev && !key3) key3_down = esp_timer_get_time();
    if (!key3_prev && !key3 && key3_down && esp_timer_get_time() - key3_down > 1500000) { key3_down = 0; ESP_LOGI(TAG, "KEY3 held: the desk sleep"); while (!gpio_get_level(PIN_KEY3)) vTaskDelay(pdMS_TO_TICKS(10)); s_scene = SC_INFO; do_sleep(true, 1800, 0, false, "key3"); }
    if (!key3_prev && key3 && key3_down) { key3_down = 0; s_scene = (s_scene + 1) % SC_N; ESP_LOGI(TAG, "KEY3: scene %s", SCENE_NAMES[s_scene]); }
    boot_prev = boot; key3_prev = key3;
    int k = s_pmic_ok ? battery_port_key_poll() : 0;
    if (k == 2) power_off();
    else if (k == 1) { s_scene = (s_scene + 1) % SC_N; ESP_LOGI(TAG, "PWR: scene %s", SCENE_NAMES[s_scene]); }
}

void app_main(void) {
    ESP_LOGI(TAG, "Pixel Lili diag v%s for the 2.16 square | internal heap %u KB", DIAG_VERSION, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }
    con_init();
    keys_init();
    if (!gfx_init()) return;
    if (!display_port_init()) { ESP_LOGE(TAG, "no display: the board was not told"); }
    i2c_master_bus_handle_t bus = board_i2c_bus();
    s_pmic_ok = battery_port_init(bus);
    if (s_pmic_ok) {
        battery_port_pin_rail("aldo1");                 /* A3V3: the ES7210's whole supply (and the ES8311's analog) */
        battery_port_trim_rails();
        battery_port_key_init();
    }
    codec_port_init(bus);                               /* the ES8311 fully down: it shares the I2S clocks with the ES7210 */
    s_rtc_ok = pcf_rtc_init(bus);
    s_imu_ok = imu_init(bus);
    whistle_init();
    s_mic_ok = mic_init(bus);
    if (s_mic_ok) s_mic_want = true;
    plog_init();
    gfx_clear(C_BLACK);
    gfx_text(120, 200, 4, C_WHITE, "PIXEL LILI");
    gfx_textf(150, 250, 3, C_GREY, "diag v%s", DIAG_VERSION);
    display_port_flush(gfx_fb);
    xTaskCreatePinnedToCore(audio_task, "audio", 8192, NULL, 6, NULL, 1);
    plog_add(PL_ACTIVE, "boot");
    pcf_rtc_log();
    ESP_LOGI(TAG, "up: display %s, PMIC %s, RTC %s, IMU %s (INT1 GPIO %d = %d, INT2 GPIO %d = %d), mic %s | internal heap %u KB free",
             display_port_brightness() ? "ok" : "?", s_pmic_ok ? "ok" : "none", s_rtc_ok ? "ok" : "none", s_imu_ok ? "ok" : "none",
             IMU_INT1_GPIO, gpio_get_level(IMU_INT1_GPIO), IMU_INT2_GPIO, gpio_get_level(IMU_INT2_GPIO), s_mic_ok ? "ok" : "none",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    help();
    int64_t last = esp_timer_get_time(), last_log = last, last_wom = last;
    whistle_state_t ws;
    for (;;) {
        int64_t t0 = esp_timer_get_time();
        con_poll();
        keys_poll();
        prun_tick();
        lab_tick(t0);
        whistle_get(&ws);
        if (s_wom_awake && t0 - last_wom > 500000) {    /* awake WoM check: the event flag, the INT line cleared */
            last_wom = t0;
            if (gpio_get_level(IMU_INT1_GPIO)) { uint8_t s1 = imu_status1(); s_wom_events++; ESP_LOGI(TAG, "WoM event #%d awake: STATUS1 %02x, INT1 after the read %d", s_wom_events, s1, gpio_get_level(IMU_INT1_GPIO)); }
        }
        switch (s_scene) {
        case SC_MIC: draw_mic(&ws); break;
        case SC_CORNER: draw_corner(); break;
        case SC_INFO: draw_info(&ws); break;
        default: draw_plog(); break;
        }
        display_port_flush(gfx_fb);
        s_frames++;
        int64_t t1 = esp_timer_get_time();
        float dt = (t1 - last) / 1e6f; last = t1;
        if (dt > 0) s_fps = s_fps ? 0.9f * s_fps + 0.1f / dt : 1.0f / dt;
        if (t1 - last_log > 5000000) {
            last_log = t1;
            ESP_LOGI(TAG, "%.1f fps (%s) | frame %.1f ms | mic L %.0f R %.0f dBFS, peak %.0f Hz ratio %.1f dB, whistles %d | heap %u KB | %d mV %s",
                     s_fps, s_dim ? "DIM" : "ACTIVE", (t1 - t0) / 1e3f, ws.rms_db_l, ws.rms_db_r, ws.peak_hz, ws.ratio_db, ws.count,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024, battery_port_vbat_mv(), battery_port_state() == BAT_ON_BATTERY ? "on battery" : "USB");
        }
        if (s_dim) { int64_t spent = esp_timer_get_time() - t0; if (spent < 100000) vTaskDelay(pdMS_TO_TICKS((100000 - spent) / 1000)); }
        else vTaskDelay(1);
    }
}
