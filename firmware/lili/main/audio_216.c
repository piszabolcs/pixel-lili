/* audio_216.c - see audio_216.h. The bring-up order is the pocket-tank's (audio_port_es8311.c): clocks, codec,
 * silence while the DAC settles, amp, silence while the amp starts; down the other way round. */
#include "audio_216.h"
#include "sfx.h"
#include "mic_es7210.h"
#include "codec_port.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "audio";
#define PIN_DOUT      8                  /* ESP -> ES8311 DSDIN */
#define PIN_AMP       46                 /* NS4150B CTRL, a 10k pull-down on the board */
#define BLOCK         160                /* 10 ms at 16 kHz */
#define IDLE_MS       2000
#define SELF_MUTE_MS  150
#define MASTER_DB     (-12)              /* the square's box drives its speaker hard: the pocket-tank's level for it */

static i2s_chan_handle_t s_tx;
static SemaphoreHandle_t s_mx;
static bool s_up;
static volatile int64_t s_last_sound_us;
static int16_t s_mono[BLOCK], s_st[2 * BLOCK];

static void lock(bool on) { if (on) xSemaphoreTake(s_mx, portMAX_DELAY); else xSemaphoreGive(s_mx); }
static void amp(bool on) { gpio_set_level(PIN_AMP, on); }
static void silence(int ms) {
    memset(s_st, 0, sizeof s_st); size_t w;
    for (int i = 0; i < ms / 10; i++) i2s_channel_write(s_tx, s_st, sizeof s_st, &w, 100);
}
static void up(void) {
    const int64_t t0 = esp_timer_get_time();
    i2s_channel_enable(s_tx);                                 /* the clocks run already (the mic's RX) */
    const bool ok = codec_port_up();
    silence(150); codec_port_settled();
    amp(true); silence(100);
    s_up = true;
    ESP_LOGI(TAG, "up in %lld ms%s", (long long)((esp_timer_get_time() - t0) / 1000), ok ? "" : " (codec writes FAILED)");
}
static void down(void) {
    amp(false); silence(10);
    i2s_channel_disable(s_tx);
    codec_port_down();
    s_up = false;
    ESP_LOGI(TAG, "down (idle)");
}
static void player(void *arg) {
    (void)arg; int64_t quiet_since = 0;
    for (;;) {
        if (!s_up) {
            lock(true); const bool any = sfx_active(); lock(false);
            if (!any) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
            up(); quiet_since = 0;
        }
        lock(true); const int live = sfx_render(s_mono, BLOCK); lock(false);
        for (int i = 0; i < BLOCK; i++) s_st[2 * i] = s_st[2 * i + 1] = s_mono[i];   /* the same in both slots */
        size_t w; i2s_channel_write(s_tx, s_st, sizeof s_st, &w, 100);              /* ~10 ms: the DMA paces us */
        const int64_t now = esp_timer_get_time();
        if (live) { quiet_since = 0; s_last_sound_us = now; }
        else if (!quiet_since) quiet_since = now;
        else if (now - quiet_since > IDLE_MS * 1000LL) down();
    }
}

void audio216_before_mic(void) { mic_with_tx(PIN_DOUT); }
bool audio216_init(i2c_master_bus_handle_t bus, const pak_t *pak) {
    s_tx = mic_tx_chan();
    if (!s_tx) { ESP_LOGW(TAG, "no I2S TX beside the mic: silent"); return false; }
    if (!codec_port_init(bus)) { ESP_LOGW(TAG, "no ES8311: silent"); return false; }
    if (!sfx_init(pak, MASTER_DB)) { ESP_LOGW(TAG, "no sound bank in the pack (tools/lili_sounds.py, tools/pack_assets.py): silent"); return false; }
    gpio_config_t io = { .pin_bit_mask = 1ULL << PIN_AMP, .mode = GPIO_MODE_OUTPUT, .pull_down_en = GPIO_PULLDOWN_ENABLE };
    gpio_config(&io); amp(false);
    s_mx = xSemaphoreCreateMutex(); sfx_set_lock(lock);
    xTaskCreatePinnedToCore(player, "audio", 4096, NULL, 6, NULL, 1);
    ESP_LOGI(TAG, "%d cues, master %d dB (the volume: the settings card)", SFX_COUNT, MASTER_DB);
    return true;
}
bool audio216_self_mute(void) { return s_up && (esp_timer_get_time() - s_last_sound_us) < SELF_MUTE_MS * 1000LL; }

