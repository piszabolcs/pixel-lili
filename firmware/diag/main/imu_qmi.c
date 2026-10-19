#include "imu_qmi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "imu";
#define REG_WHO_AM_I  0x00
#define REG_CTRL1     0x02
#define REG_CTRL2     0x03
#define REG_CTRL7     0x08
#define REG_CTRL9     0x0A
#define REG_CAL1_L    0x0B
#define REG_CAL1_H    0x0C
#define REG_STATUSINT 0x2D
#define REG_STATUS1   0x2F
#define REG_AX_L      0x35
#define REG_RESET     0x60
#define CMD_WOM       0x08
#define CMD_ACK       0x00
static i2c_master_dev_handle_t s_dev;
static bool s_armed;

static bool wr8(uint8_t reg, uint8_t v) { uint8_t b[2] = { reg, v }; return s_dev && i2c_master_transmit(s_dev, b, 2, 100) == ESP_OK; }
static bool rdn(uint8_t reg, uint8_t *v, size_t n) { return s_dev && i2c_master_transmit_receive(s_dev, &reg, 1, v, n, 100) == ESP_OK; }
static bool rd8(uint8_t reg, uint8_t *v) { return rdn(reg, v, 1); }

static bool live_config(void) {
    return wr8(REG_CTRL1, 0x40) && wr8(REG_CTRL2, 0x08) && wr8(REG_CTRL7, 0x01);   /* auto-increment; +-2 g 31.25 Hz; accel on */
}
bool imu_init(i2c_master_bus_handle_t bus) {
    if (!bus) return false;
    uint8_t addr = 0x6B;
    if (i2c_master_probe(bus, addr, 50) != ESP_OK) { addr = 0x6A; if (i2c_master_probe(bus, addr, 50) != ESP_OK) { ESP_LOGW(TAG, "no QMI8658"); return false; } }
    i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr, .scl_speed_hz = 400000 };
    if (i2c_master_bus_add_device(bus, &cfg, &s_dev) != ESP_OK) return false;
    uint8_t who = 0; rd8(REG_WHO_AM_I, &who);
    wr8(REG_CTRL1, 0x40); vTaskDelay(pdMS_TO_TICKS(5));
    wr8(REG_RESET, 0xB0); vTaskDelay(pdMS_TO_TICKS(30));
    bool ok = live_config();
    uint8_t c1 = 0, c2 = 0, c7 = 0; rd8(REG_CTRL1, &c1); rd8(REG_CTRL2, &c2); rd8(REG_CTRL7, &c7);
    ESP_LOGI(TAG, "QMI8658 at 0x%02x whoami 0x%02x (want 05): ctrl 1=%02x 2=%02x 7=%02x (want 40/08/01)%s", addr, who, c1, c2, c7, ok ? "" : " - a write FAILED");
    return ok && who == 0x05;
}
bool imu_read(int16_t a[3]) {
    uint8_t raw[6];
    if (!rdn(REG_AX_L, raw, 6)) return false;
    for (int i = 0; i < 3; i++) a[i] = (int16_t)(raw[2 * i] | raw[2 * i + 1] << 8);
    return true;
}
/* CTRL9 handshake: the command, STATUSINT bit 7 set, the ACK, bit 7 clear */
static bool ctrl9(uint8_t cmd) {
    if (!wr8(REG_CTRL9, cmd)) return false;
    int64_t t0 = esp_timer_get_time(); uint8_t st = 0;
    while (esp_timer_get_time() - t0 < 300000) { if (rd8(REG_STATUSINT, &st) && (st & 0x80)) break; vTaskDelay(1); }
    if (!(st & 0x80)) { ESP_LOGW(TAG, "CTRL9 cmd %02x: no done flag (STATUSINT %02x)", cmd, st); return false; }
    if (!wr8(REG_CTRL9, CMD_ACK)) return false;
    t0 = esp_timer_get_time();
    while (esp_timer_get_time() - t0 < 300000) { if (rd8(REG_STATUSINT, &st) && !(st & 0x80)) return true; vTaskDelay(1); }
    ESP_LOGW(TAG, "CTRL9 cmd %02x: the done flag did not clear", cmd);
    return false;
}
bool imu_wom_arm(int thr_mg, int blank) {
    if (!s_dev) return false;
    if (thr_mg < 1) thr_mg = 1;
    if (thr_mg > 255) thr_mg = 255;
    if (blank < 0) blank = 0;
    if (blank > 63) blank = 63;
    bool ok = wr8(REG_CTRL7, 0x00);                                   /* accel off while the setting is written */
    ok &= wr8(REG_CTRL2, 0x0D);                                       /* +-2 g, low-power 21 Hz (WoM wants a low-power ODR, gyro off) */
    ok &= wr8(REG_CAL1_L, (uint8_t)thr_mg);
    ok &= wr8(REG_CAL1_H, (uint8_t)((0x00 << 6) | blank));           /* INT1, initial level 0, blanking */
    ok &= ctrl9(CMD_WOM);
    ok &= wr8(REG_CTRL7, 0x01);                                       /* accel on */
    ok &= wr8(REG_CTRL1, 0x48);                                       /* INT1 enabled (+ auto-increment) */
    s_armed = ok;
    uint8_t s1 = 0; rd8(REG_STATUS1, &s1);                            /* a stale flag cleared */
    ESP_LOGI(TAG, "WoM %s: threshold %d mg, blanking %d samples, INT1 (GPIO %d) idle low", ok ? "armed" : "NOT armed", thr_mg, blank, IMU_INT1_GPIO);
    return ok;
}
bool imu_wom_disarm(void) {
    if (!s_dev) return false;
    bool ok = wr8(REG_CTRL7, 0x00) && wr8(REG_CAL1_L, 0x00) && wr8(REG_CAL1_H, 0x00) && ctrl9(CMD_WOM);   /* threshold 0 = WoM off */
    ok &= live_config();
    s_armed = false;
    return ok;
}
bool imu_wom_armed(void) { return s_armed; }
uint8_t imu_status1(void) { uint8_t v = 0; rd8(REG_STATUS1, &v); return v; }
uint8_t imu_statusint(void) { uint8_t v = 0; rd8(REG_STATUSINT, &v); return v; }
void imu_sleep(void) { wr8(REG_CTRL7, 0x00); s_armed = false; }
