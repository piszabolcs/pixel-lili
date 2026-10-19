#include "rtc_pcf.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

static const char *TAG = "rtc";
#define ADDR 0x51
static i2c_master_dev_handle_t s_dev;
static int bcd2bin(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t bin2bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

bool pcf_rtc_init(i2c_master_bus_handle_t bus) {
    if (!bus || i2c_master_probe(bus, ADDR, 50) != ESP_OK) { ESP_LOGW(TAG, "no PCF85063"); return false; }
    i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = ADDR, .scl_speed_hz = 400000 };
    if (i2c_master_bus_add_device(bus, &cfg, &s_dev) != ESP_OK) return false;
    struct tm t; bool valid = false;
    if (pcf_rtc_read(&t, &valid) && valid && t.tm_year + 1900 >= 2024) {
        struct timeval tv = { .tv_sec = mktime(&t) }; settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "system clock from the RTC: %04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    } else ESP_LOGW(TAG, "RTC %s (%04d-%02d-%02d) - settime <YYYY-MM-DDTHH:MM:SS> sets it", valid ? "implausible" : "INVALID (oscillator stopped)", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return true;
}
bool pcf_rtc_read(struct tm *out, bool *valid) {
    uint8_t reg = 0x04, b[7];
    if (!s_dev || i2c_master_transmit_receive(s_dev, &reg, 1, b, 7, 100) != ESP_OK) return false;
    memset(out, 0, sizeof *out);
    out->tm_sec = bcd2bin(b[0] & 0x7F); out->tm_min = bcd2bin(b[1] & 0x7F); out->tm_hour = bcd2bin(b[2] & 0x3F);
    out->tm_mday = bcd2bin(b[3] & 0x3F); out->tm_wday = b[4] & 7; out->tm_mon = bcd2bin(b[5] & 0x1F) - 1; out->tm_year = bcd2bin(b[6]) + 100;
    if (valid) *valid = !(b[0] & 0x80);
    return true;
}
bool pcf_rtc_write(const struct tm *t) {
    if (!s_dev) return false;
    uint8_t w[8] = { 0x04, bin2bcd(t->tm_sec), bin2bcd(t->tm_min), bin2bcd(t->tm_hour), bin2bcd(t->tm_mday),
                     (uint8_t)t->tm_wday, bin2bcd(t->tm_mon + 1), bin2bcd(t->tm_year - 100) };
    if (i2c_master_transmit(s_dev, w, sizeof w, 100) != ESP_OK) return false;
    struct tm c = *t; struct timeval tv = { .tv_sec = mktime(&c) }; settimeofday(&tv, NULL);
    return true;
}
bool pcf_rtc_set_iso(const char *s) {
    int Y, M, D, h, m, sec = 0;
    if (sscanf(s, "%d-%d-%d%*[T ]%d:%d:%d", &Y, &M, &D, &h, &m, &sec) < 5) { ESP_LOGW(TAG, "settime: want YYYY-MM-DDTHH:MM:SS, got \"%s\"", s); return false; }
    if (Y < 2024 || Y > 2099 || M < 1 || M > 12 || D < 1 || D > 31 || h > 23 || m > 59 || sec > 59) { ESP_LOGW(TAG, "settime: out of range"); return false; }
    struct tm t; memset(&t, 0, sizeof t);
    t.tm_year = Y - 1900; t.tm_mon = M - 1; t.tm_mday = D; t.tm_hour = h; t.tm_min = m; t.tm_sec = sec;
    struct tm n = t; mktime(&n); t.tm_wday = n.tm_wday;
    bool ok = pcf_rtc_write(&t);
    ESP_LOGI(TAG, "settime %s: %04d-%02d-%02d %02d:%02d:%02d written to the PCF85063", ok ? "ok" : "FAILED", Y, M, D, h, m, sec);
    pcf_rtc_log();
    return ok;
}
void pcf_rtc_log(void) {
    struct tm t; bool valid = false;
    if (!pcf_rtc_read(&t, &valid)) { ESP_LOGW(TAG, "no RTC"); return; }
    time_t now = time(NULL); struct tm sys; localtime_r(&now, &sys);
    uint8_t reg = 0x00, c[2] = { 0, 0 }; i2c_master_transmit_receive(s_dev, &reg, 1, c, 2, 100);
    ESP_LOGI(TAG, "RTC %04d-%02d-%02d %02d:%02d:%02d (%s) | system %04d-%02d-%02d %02d:%02d:%02d | Control_1 %02x Control_2 %02x",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, valid ? "valid" : "INVALID: oscillator stopped since the last set",
             sys.tm_year + 1900, sys.tm_mon + 1, sys.tm_mday, sys.tm_hour, sys.tm_min, sys.tm_sec, c[0], c[1]);
}
