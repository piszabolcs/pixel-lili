/* touch_216.c - the 2.16's touch chip (CST9220 at 0x5A, I2C on GPIO 15 / 14, reset on 40), polled once a frame,
 * in the Lili picture's logical px (M4.3), on the bus the display port made. The read and the lift rule are the pocket-tank's
 * (firmware/main/touch_port_ft3168.c cst9217_read): D0 00 reads 10 bytes, D0 00 AB acknowledges; a report has AB
 * at byte 6, a finger when byte 0's low nibble is 6 and byte 5 counts one; the finger stays down until a report
 * says "no finger" or none has shown it for 60 ms (the chip skips reports inside a press). The raw point is the
 * panel's own orientation (the pocket-tank's square mapping, swap + mirror x, then its 0xA0 picture); the Lili
 * picture is the panel's own turned 180 degrees (LILI_MADCTL 0xC0 = MX|MY), so the point is (479 - rx, 479 - ry).
 * To be checked on the panel with a finger [G]: the serial log prints every gesture's event with its x, y. */
#include "touch_216.h"
#include "board_pins.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_cst;
extern i2c_master_bus_handle_t board_i2c_bus(void);   /* display_port_sh8601.c makes the board's bus (GPIO 15 / 14) */
#define TOUCH_LIFT_US 60000
#define TOUCH_PANEL   480

bool touch216_init(void) {
    s_bus = board_i2c_bus();
    if (!s_bus) { ESP_LOGW(TAG, "no I2C bus (display_port_init first)"); return false; }
    gpio_config_t rst = { .pin_bit_mask = 1ULL << S_PIN_TP_RST, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&rst);
    gpio_set_level(S_PIN_TP_RST, 0); vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(S_PIN_TP_RST, 1); vTaskDelay(pdMS_TO_TICKS(50));
    if (i2c_master_probe(s_bus, I2C_ADDR_CST9217, 50) != ESP_OK) { ESP_LOGW(TAG, "no CST92xx at 0x%02x", I2C_ADDR_CST9217); return false; }
    i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = I2C_ADDR_CST9217, .scl_speed_hz = 400000,
                                .flags.disable_ack_check = 1 };   /* it NACKs a read now and then while it scans */
    if (i2c_master_bus_add_device(s_bus, &cfg, &s_cst) != ESP_OK) { s_cst = NULL; return false; }
    ESP_LOGI(TAG, "CST9220 ready");
    return true;
}

bool touch216_read(int *x, int *y) {
    static const uint8_t cmd[2] = { 0xD0, 0x00 }, ack[3] = { 0xD0, 0x00, 0xAB };
    static int lx, ly; static int64_t seen_us; static bool down;
    if (!s_cst) return false;
    uint8_t d[10] = { 0 };
    const int64_t now = esp_timer_get_time();
    bool finger = false, said = false;
    if (i2c_master_transmit_receive(s_cst, cmd, 2, d, sizeof d, 20) == ESP_OK) {
        i2c_master_transmit(s_cst, ack, 3, 20);
        finger = d[6] == 0xAB && (d[5] & 0x7F) && (d[0] & 0x0F) == 0x06;
        said = d[6] == 0xAB && d[0] != 0xAB && !finger;            /* a report, and no finger in it */
    }
    if (finger) {
        int rx = (d[1] << 4) | (d[3] >> 4), ry = (d[2] << 4) | (d[3] & 0x0F);
        if (rx > TOUCH_PANEL - 1) rx = TOUCH_PANEL - 1;
        if (ry > TOUCH_PANEL - 1) ry = TOUCH_PANEL - 1;
        lx = TOUCH_PANEL - 1 - rx; ly = TOUCH_PANEL - 1 - ry;
        seen_us = now; down = true;
    } else if (down && (said || now - seen_us > TOUCH_LIFT_US)) down = false;
    *x = lx; *y = ly;
    return down;
}
