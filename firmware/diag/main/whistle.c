#include "whistle.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <math.h>
#include <string.h>

static const char *TAG = "whistle";
static float s_win[WH_N], s_cos[WH_N / 2], s_sin[WH_N / 2];
static float s_re[WH_N], s_im[WH_N];
static uint16_t s_rev[WH_N];
static whistle_state_t s_st;
static whistle_params_t s_p = { .thr_db = 12.0f, .frames = 8, .tol = 0.15f, .floor_db = -60.0f };
static portMUX_TYPE s_mx = portMUX_INITIALIZER_UNLOCKED;
static float s_ref_hz; static int s_quiet;
#define HOLD_US 1500000

void whistle_init(void) {
    for (int i = 0; i < WH_N; i++) s_win[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (WH_N - 1)));
    for (int i = 0; i < WH_N / 2; i++) { s_cos[i] = cosf(-2.0f * (float)M_PI * i / WH_N); s_sin[i] = sinf(-2.0f * (float)M_PI * i / WH_N); }
    for (int i = 0; i < WH_N; i++) { int r = 0; for (int b = 0; b < 9; b++) if (i & (1 << b)) r |= 1 << (8 - b); s_rev[i] = (uint16_t)r; }
    memset(&s_st, 0, sizeof s_st);
    ESP_LOGI(TAG, "detector: band %d-%d Hz, peak/mean > %.0f dB, +-%.0f %%, %d frames (%d ms), floor %.0f dBFS",
             WH_BIN0 * WH_RATE / WH_N, WH_BIN1 * WH_RATE / WH_N, s_p.thr_db, s_p.tol * 100, s_p.frames, s_p.frames * 1000 * WH_N / WH_RATE, s_p.floor_db);
}
whistle_params_t *whistle_params(void) { return &s_p; }

static void fft512(void) {                                   /* in place, radix-2, decimation in time */
    for (int i = 0; i < WH_N; i++) { int j = s_rev[i]; if (j > i) { float t = s_re[i]; s_re[i] = s_re[j]; s_re[j] = t; t = s_im[i]; s_im[i] = s_im[j]; s_im[j] = t; } }
    for (int len = 2; len <= WH_N; len <<= 1) {
        int half = len >> 1, step = WH_N / len;
        for (int i = 0; i < WH_N; i += len)
            for (int k = 0; k < half; k++) {
                float wr = s_cos[k * step], wi = s_sin[k * step];
                float xr = s_re[i + k + half], xi = s_im[i + k + half];
                float tr = xr * wr - xi * wi, ti = xr * wi + xi * wr;
                s_re[i + k + half] = s_re[i + k] - tr; s_im[i + k + half] = s_im[i + k] - ti;
                s_re[i + k] += tr; s_im[i + k] += ti;
            }
    }
}
static float rms_db(const int16_t *x) {
    double acc = 0; for (int i = 0; i < WH_N; i++) acc += (double)x[i] * x[i];
    float rms = sqrtf((float)(acc / WH_N)) / 32768.0f;
    return 20.0f * log10f(rms + 1e-9f);
}
void whistle_frame(const int16_t *l, const int16_t *r) {
    whistle_state_t st; memcpy(&st, &s_st, sizeof st);
    st.rms_db_l = rms_db(l); st.rms_db_r = rms_db(r);
    for (int i = 0; i < WH_N; i++) { s_re[i] = l[i] * (1.0f / 32768.0f) * s_win[i]; s_im[i] = 0; }
    fft512();
    /* a full-scale sine under the Hann window puts N/4 into its bin: that is 0 dBFS */
    const float ref = 20.0f * log10f(WH_N / 4.0f);
    static float pw[WH_N / 2 + 1];
    for (int k = 0; k <= WH_N / 2; k++) pw[k] = s_re[k] * s_re[k] + s_im[k] * s_im[k];
    for (int b = 0; b < WH_FULL; b++) { float m = 0; for (int k = 0; k < 4; k++) m = fmaxf(m, pw[b * 4 + k]); st.full_db[b] = 10.0f * log10f(m + 1e-12f) - ref; }
    int pk = WH_BIN0; float pmax = 0, sum = 0;
    for (int k = WH_BIN0; k <= WH_BIN1; k++) { st.band_db[k - WH_BIN0] = 10.0f * log10f(pw[k] + 1e-12f) - ref; if (pw[k] > pmax) { pmax = pw[k]; pk = k; } sum += pw[k]; }
    /* the band's mean without the peak's three bins (a whistle is one line; it should not lift its own floor) */
    int excl = 0; float ex = 0;
    for (int k = pk - 1; k <= pk + 1; k++) if (k >= WH_BIN0 && k <= WH_BIN1) { ex += pw[k]; excl++; }
    float mean = (sum - ex) / (float)(WH_BAND - excl);
    /* the peak's frequency, refined a little between its neighbours */
    float a = pk > 0 ? pw[pk - 1] : 0, b = pw[pk], c = pk < WH_N / 2 ? pw[pk + 1] : 0;
    float d = (a + b + c) > 0 ? 0.5f * (c - a) / (a + b + c) * 2.0f : 0;   /* -1..1 */
    if (d > 0.5f) d = 0.5f;
    if (d < -0.5f) d = -0.5f;
    st.peak_hz = (pk + d) * (float)WH_RATE / WH_N;
    st.peak_db = 10.0f * log10f(pmax + 1e-12f) - ref;
    st.mean_db = 10.0f * log10f(mean + 1e-12f) - ref;
    st.ratio_db = st.peak_db - st.mean_db;
    st.tone = st.ratio_db > s_p.thr_db && st.peak_db > s_p.floor_db;
    int64_t now = esp_timer_get_time();
    if (st.tone) {
        if (st.streak > 0 && fabsf(st.peak_hz - s_ref_hz) <= s_p.tol * s_ref_hz) { st.streak++; s_ref_hz = 0.8f * s_ref_hz + 0.2f * st.peak_hz; }
        else { st.streak = 1; s_ref_hz = st.peak_hz; }
        s_quiet = 0;
        if (st.streak == s_p.frames) {                      /* the event, once per whistle */
            st.count++; st.last_us = now; st.shown = true;
            ESP_LOGI(TAG, "FÜTTY #%d: %.0f Hz, peak %.1f dBFS, %.1f dB over the band (%d frames)", st.count, s_ref_hz, st.peak_db, st.ratio_db, st.streak);
        }
    } else {
        if (++s_quiet >= 3) st.streak = 0;                  /* two frames of breath do not end a whistle; three do */
    }
    if (st.shown && now - st.last_us > HOLD_US) st.shown = false;
    st.frames++;
    taskENTER_CRITICAL(&s_mx); memcpy(&s_st, &st, sizeof st); taskEXIT_CRITICAL(&s_mx);
}
void whistle_get(whistle_state_t *out) { taskENTER_CRITICAL(&s_mx); memcpy(out, &s_st, sizeof *out); taskEXIT_CRITICAL(&s_mx); }
