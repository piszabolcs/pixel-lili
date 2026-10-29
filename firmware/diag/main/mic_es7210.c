#include "mic_es7210.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "mic";
#define ES7210_ADDR 0x40
#define PIN_MCLK 42
#define PIN_BCLK 9
#define PIN_WS   45
#define PIN_DIN  10
#define RATE     16000

static i2c_master_dev_handle_t s_dev;
static i2s_chan_handle_t s_rx, s_tx;
static int s_tx_dout = -1;              /* mic_with_tx: the same I2S controller drives the ES8311 speaker codec too (Lili, 2026-10-29) */
void mic_with_tx(int dout_pin) { s_tx_dout = dout_pin; }
i2s_chan_handle_t mic_tx_chan(void) { return s_tx; }
static bool s_up;
static int s_gain = 10;                 /* 30 dB: the IDF example's choice for the ES7210's own mics */
static int16_t s_buf[2 * 1024];

static bool wr(uint8_t reg, uint8_t val) { uint8_t b[2] = { reg, val }; return i2c_master_transmit(s_dev, b, 2, 100) == ESP_OK; }
static bool rd(uint8_t reg, uint8_t *val) { return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, 100) == ESP_OK; }

bool mic_init(i2c_master_bus_handle_t bus) {
    if (!bus || i2c_master_probe(bus, ES7210_ADDR, 50) != ESP_OK) { ESP_LOGW(TAG, "no ES7210 at 0x40"); return false; }
    i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = ES7210_ADDR, .scl_speed_hz = 400000 };
    if (i2c_master_bus_add_device(bus, &cfg, &s_dev) != ESP_OK) return false;
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    cc.dma_desc_num = 4; cc.dma_frame_num = 512;
    /* the speaker's codec hangs on the same MCLK / BCLK / LRCK: two controllers would fight over them, so with a TX
       wanted the one controller runs both ways (full duplex), the same clocks, the data out on its own pin */
    if (i2s_new_channel(&cc, s_tx_dout >= 0 ? &s_tx : NULL, &s_rx) != ESP_OK) { ESP_LOGE(TAG, "no I2S RX channel"); return false; }
    i2s_std_config_t std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(RATE),                  /* MCLK = 256 fs = 4.096 MHz */
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = { .mclk = PIN_MCLK, .bclk = PIN_BCLK, .ws = PIN_WS, .dout = I2S_GPIO_UNUSED, .din = PIN_DIN,
                      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false } },
    };
    if (i2s_channel_init_std_mode(s_rx, &std) != ESP_OK) { ESP_LOGE(TAG, "I2S std init failed"); return false; }
    if (s_tx) {
        std.gpio_cfg.dout = s_tx_dout; std.gpio_cfg.din = I2S_GPIO_UNUSED;
        if (i2s_channel_init_std_mode(s_tx, &std) != ESP_OK) { ESP_LOGE(TAG, "I2S TX init failed"); i2s_del_channel(s_tx); s_tx = NULL; }
    }
    return mic_up();
}

/* the register set of Espressif's es7210 driver (esp-bsp, Apache-2.0), reduced to
 * this board: I2S slave, 16-bit, MCLK 4.096 MHz / 16 kHz row of its clock table
 * (adc_div 1, dll 1, doubler 1, osr 0x20, lrck 256), mic bias 2.87 V, MIC1+2 on,
 * MIC3+4 off, ADC digital gain 0 dB */
bool mic_up(void) {
    if (!s_dev) return false;
    if (s_up) return true;
    i2s_channel_enable(s_rx);                                         /* the clocks first: the chip's state machine wants MCLK */
    vTaskDelay(pdMS_TO_TICKS(5));
    const uint8_t seq[][2] = {
        { 0x00, 0xFF }, { 0x00, 0x32 },                               /* software reset */
        { 0x09, 0x30 }, { 0x0A, 0x30 },                               /* chip-state / power-on cycle times */
        { 0x23, 0x2A }, { 0x22, 0x0A }, { 0x21, 0x2A }, { 0x20, 0x0A }, /* HPF */
        { 0x11, 0x60 }, { 0x12, 0x00 },                               /* I2S, 16-bit; no TDM */
        { 0x40, 0x43 },                                               /* analog on, VDDA 3.3 V */
        { 0x41, 0x70 }, { 0x42, 0x70 },                               /* mic bias 2.87 V */
        { 0x43, (uint8_t)(0x10 | s_gain) }, { 0x44, (uint8_t)(0x10 | s_gain) }, { 0x45, 0x10 }, { 0x46, 0x10 },
        { 0x47, 0x08 }, { 0x48, 0x08 }, { 0x49, 0x08 }, { 0x4A, 0x08 },
        { 0x07, 0x20 }, { 0x02, 0xC1 }, { 0x04, 0x01 }, { 0x05, 0x00 }, /* the 4.096 MHz / 16 kHz row */
        { 0x06, 0x04 },                                               /* DLL off (bypassed) */
        { 0x4B, 0x00 }, { 0x4C, 0xFF },                               /* MIC1/2 powered, MIC3/4 down */
        { 0x1B, 0xBF }, { 0x1C, 0xBF },                               /* ADC1/2 digital gain 0 dB */
        { 0x00, 0x71 }, { 0x00, 0x41 },                               /* state machine: run */
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof seq / sizeof seq[0]; i++) { ok &= wr(seq[i][0], seq[i][1]); if (i == 1) vTaskDelay(pdMS_TO_TICKS(2)); }
    s_up = ok;
    ESP_LOGI(TAG, "ES7210 up%s: 16 kHz 16-bit stereo (MIC1 left, MIC2 right), gain code %d", ok ? "" : " - a write FAILED", s_gain);
    return ok;
}
void mic_down(void) {
    if (!s_dev || !s_up) return;
    static const uint8_t seq[][2] = {                                 /* the pocket-tank's suspend (codec_port_mic_adc_down) */
        { 0x47, 0xFF }, { 0x48, 0xFF }, { 0x49, 0xFF }, { 0x4A, 0xFF }, { 0x4B, 0xFF }, { 0x4C, 0xFF },
        { 0x40, 0xC0 }, { 0x01, 0x7F }, { 0x06, 0x07 },
    };
    for (size_t i = 0; i < sizeof seq / sizeof seq[0]; i++) wr(seq[i][0], seq[i][1]);
    i2s_channel_disable(s_rx);
    s_up = false;
    ESP_LOGI(TAG, "ES7210 suspended, I2S off");
}
bool mic_is_up(void) { return s_up; }
int mic_read(int16_t *left, int16_t *right, int n, int timeout_ms) {
    if (!s_up || n > 1024) return 0;
    size_t got = 0;
    if (i2s_channel_read(s_rx, s_buf, (size_t)n * 4, &got, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) return 0;
    int frames = (int)(got / 4);
    for (int i = 0; i < frames; i++) { left[i] = s_buf[2 * i]; right[i] = s_buf[2 * i + 1]; }
    return frames;
}
void mic_set_gain(int code) {
    if (code < 0) code = 0;
    if (code > 14) code = 14;
    s_gain = code;
    if (s_dev && s_up) { wr(0x43, (uint8_t)(0x10 | code)); wr(0x44, (uint8_t)(0x10 | code)); }
    ESP_LOGI(TAG, "mic gain code %d (%s dB)", code, code <= 10 ? (const char *[]){"0","3","6","9","12","15","18","21","24","27","30"}[code] : code == 11 ? "33" : code == 12 ? "34.5" : code == 13 ? "36" : "37.5");
}
int mic_gain(void) { return s_gain; }
void mic_dump(void) {
    if (!s_dev) { ESP_LOGI(TAG, "no ES7210"); return; }
    static const uint8_t regs[] = { 0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x07, 0x08, 0x11, 0x12, 0x40, 0x41, 0x43, 0x44, 0x47, 0x48, 0x4B, 0x4C };
    char line[160]; int k = 0;
    for (size_t i = 0; i < sizeof regs && k < (int)sizeof line - 8; i++) { uint8_t v = 0xEE; rd(regs[i], &v); k += snprintf(line + k, sizeof line - k, "%02x=%02x ", regs[i], v); }
    ESP_LOGI(TAG, "ES7210 regs: %s", line);
}
