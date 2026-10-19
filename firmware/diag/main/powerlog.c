#include "powerlog.h"
#include "battery_port.h"
#include "esp_log.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "plog";
#define CELL_MAH 1000
typedef struct { uint32_t magic; uint16_t n, head; plog_row_t rows[PL_ROWS]; } log_t;
#define MAGIC 0x50104C06u
static log_t s_log;
static const char *NAMES[5] = { "ACTIVE", "DIM", "SLEEP", "OFF", "WAIT" };
const char *plog_state_name(int st) { st &= 0x0F; return st < 5 ? NAMES[st] : "?"; }

static void save(void) {
    nvs_handle_t h; if (nvs_open("diag", NVS_READWRITE, &h) != ESP_OK) return;
    if (nvs_set_blob(h, "plog", &s_log, sizeof s_log) == ESP_OK) nvs_commit(h);
    nvs_close(h);
}
void plog_init(void) {
    nvs_handle_t h; size_t len = sizeof s_log; bool ok = false;
    if (nvs_open("diag", NVS_READONLY, &h) == ESP_OK) {
        ok = nvs_get_blob(h, "plog", &s_log, &len) == ESP_OK && len == sizeof s_log && s_log.magic == MAGIC && s_log.n <= PL_ROWS && s_log.head < PL_ROWS;
        nvs_close(h);
    }
    if (!ok) { memset(&s_log, 0, sizeof s_log); s_log.magic = MAGIC; }
    ESP_LOGI(TAG, "%d rows in the power log (powerlog prints them, powerlog clear empties it)", s_log.n);
}
void plog_clear(void) { memset(&s_log, 0, sizeof s_log); s_log.magic = MAGIC; save(); }
int plog_count(void) { return s_log.n; }
const plog_row_t *plog_row(int i) {
    if (i < 0 || i >= s_log.n) return NULL;
    int first = (s_log.head - s_log.n + PL_ROWS) % PL_ROWS;
    return &s_log.rows[(first + i) % PL_ROWS];
}
void plog_add(int state, const char *why) {
    float frac = 0; bool chg = false;
    bool ok = battery_port_read(&frac, &chg);
    int st = battery_port_state();
    plog_row_t *r = &s_log.rows[s_log.head];
    r->t = (uint32_t)time(NULL); r->mv = (uint16_t)battery_port_vbat_mv(); r->pct = ok ? (uint8_t)(frac * 100 + 0.5f) : 255;
    r->st = (uint8_t)((state & 0x0F) | (st != BAT_ON_BATTERY ? 0x80 : 0));
    strncpy(r->why, why ? why : "", sizeof r->why - 1); r->why[sizeof r->why - 1] = 0;
    s_log.head = (uint16_t)((s_log.head + 1) % PL_ROWS);
    if (s_log.n < PL_ROWS) s_log.n++;
    save();
    ESP_LOGI(TAG, "row: %s %s | %d%% %d mV%s", plog_state_name(state), r->why, r->pct, r->mv, (r->st & 0x80) ? " | USB" : "");
}
void plog_print(void) {
    if (!s_log.n) { ESP_LOGI(TAG, "no rows"); return; }
    double mah[5] = {0}, hrs[5] = {0};
    const plog_row_t *prev = NULL;
    ESP_LOGI(TAG, "%d rows (time | SoC | VBAT | state | why | mA since the previous row, from the 1000 mAh cell)", s_log.n);
    for (int i = 0; i < s_log.n; i++) {
        const plog_row_t *r = plog_row(i);
        time_t t = r->t; struct tm tm; localtime_r(&t, &tm);
        char cur[40] = "";
        if (prev && r->t > prev->t && r->pct != 255 && prev->pct != 255) {
            double dh = (r->t - prev->t) / 3600.0, dmah = ((int)prev->pct - (int)r->pct) * CELL_MAH / 100.0;
            if ((prev->st & 0x80) || (r->st & 0x80)) snprintf(cur, sizeof cur, "(USB in: not counted)");
            else {
                snprintf(cur, sizeof cur, "%6.1f mA over %.2f h", dmah / dh, dh);
                int s = prev->st & 0x0F;
                if (s < 5) { mah[s] += dmah; hrs[s] += dh; }
            }
        }
        ESP_LOGI(TAG, "  %02d-%02d %02d:%02d:%02d | %3d%% | %4d mV | %-6s | %-7s |%s %s", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                 r->pct, r->mv, plog_state_name(r->st), r->why, (r->st & 0x80) ? " USB" : "", cur);
        prev = r;
    }
    for (int s = 0; s < 5; s++)
        if (hrs[s] > 0) ESP_LOGI(TAG, "  %-6s: %.2f h, %.0f mAh -> %.1f mA average (1%% = 10 mAh: a 20 min window is coarse, a night is not)", NAMES[s], hrs[s], mah[s], mah[s] / hrs[s]);
}
