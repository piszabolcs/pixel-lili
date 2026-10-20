/* inspector.c - see inspector.h. The labels are ASCII: LVGL's built-in Montserrat has no
 * Hungarian accents, and this is a developer tool; the game's own pixel font (M10) has them. */
#include "inspector.h"
#include <stdio.h>
#include <string.h>

enum { ACT_NONE = 0, ACT_DEBUG, ACT_DAYPART_CYCLE };
typedef struct { const char *label; int w; int type, a, act; } btn_t;

/* rows of buttons; a row ends at the next { NULL } */
static const btn_t BTNS[] = {
    { "Szoba / Udvar", 108, EV_SWITCH_VIEW, 0, ACT_NONE },    { "Futty (W)", 108, EV_WHISTLE, 0, ACT_NONE },        { "Nev: Lili (N)", 108, EV_CALL_NAME, 0, ACT_NONE },
    { "Kezbevetel", 108, EV_PICKUP, 0, ACT_NONE },            { "Razas (Space)", 108, EV_SHAKE, 0, ACT_NONE },      { "Hang / taps", 108, EV_VOICE_ACTIVITY, 0, ACT_NONE },
    { "+1 ora (T)", 108, EV_TIME_SKIP, 3600, ACT_NONE },      { "+8 ora", 108, EV_TIME_SKIP, 28800, ACT_NONE },     { "Napszak (D)", 108, 0, 0, ACT_DAYPART_CYCLE },
    { "Tavasz", 64, EV_SET_SEASON, 0, ACT_NONE },             { "Nyar", 64, EV_SET_SEASON, 1, ACT_NONE },           { "Osz", 64, EV_SET_SEASON, 2, ACT_NONE },
    { "Tel", 64, EV_SET_SEASON, 3, ACT_NONE },                { "Auto", 64, EV_SET_SEASON, -1, ACT_NONE },
    { "Debug retegek (M)", 170, 0, 0, ACT_DEBUG },
};
#define NBTN ((int)(sizeof BTNS / sizeof BTNS[0]))

static world_t *s_world; static ev_queue_t *s_q; static bool *s_debug;
static lv_obj_t *s_status, *s_console_label;
static char s_lines[9][72]; static int s_nlines;

static void on_click(lv_event_t *e) {
    const btn_t *b = (const btn_t *)lv_event_get_user_data(e);
    if (b->act == ACT_DEBUG) { *s_debug = !*s_debug; char l[48]; snprintf(l, sizeof l, "debug layers %s", *s_debug ? "ON" : "off"); inspector_log(l); }
    else if (b->act == ACT_DAYPART_CYCLE) { int cur = s_world->daypart_force; ev_push(s_q, EV_SET_DAYPART, cur >= 3 ? -1 : cur + 1, 0); }
    else ev_push(s_q, (ev_type_t)b->type, b->a, 0);
}

static lv_obj_t *transparent_box(lv_obj_t *parent, bool row_wrap) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_width(c, LV_PCT(100)); lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0); lv_obj_set_style_pad_row(c, 6, 0); lv_obj_set_style_pad_column(c, 6, 0);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    if (row_wrap) lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
    return c;
}

void inspector_init(world_t *w, ev_queue_t *q, bool *debug) {
    s_world = w; s_q = q; s_debug = debug;
    lv_obj_t *panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel, INSPECTOR_W, LILI_PHYS_H); lv_obj_set_pos(panel, LILI_PHYS_W, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1c1c22), 0); lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0); lv_obj_set_style_pad_all(panel, 8, 0); lv_obj_set_style_pad_row(panel, 6, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Pixel Lili - inspector");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);

    lv_obj_t *row = NULL; int used = 0;
    for (int i = 0; i < NBTN; i++) {
        if (!row || used + BTNS[i].w > 344) { row = transparent_box(panel, true); used = 0; }
        used += BTNS[i].w + 6;
        lv_obj_t *b = lv_button_create(row);
        lv_obj_set_size(b, BTNS[i].w, 30);
        lv_obj_add_event_cb(b, on_click, LV_EVENT_CLICKED, (void *)&BTNS[i]);
        lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, BTNS[i].label); lv_obj_center(l);
    }

    s_status = lv_label_create(panel);
    lv_obj_set_style_text_color(s_status, lv_color_hex(0xb8e8b8), 0);
    lv_label_set_text(s_status, "...");

    lv_obj_t *con = lv_obj_create(panel);
    lv_obj_set_width(con, LV_PCT(100)); lv_obj_set_flex_grow(con, 1);
    lv_obj_set_style_bg_color(con, lv_color_hex(0x0c0c10), 0); lv_obj_set_style_border_width(con, 0, 0);
    lv_obj_set_style_radius(con, 4, 0); lv_obj_set_style_pad_all(con, 6, 0); lv_obj_remove_flag(con, LV_OBJ_FLAG_SCROLLABLE);
    s_console_label = lv_label_create(con);
    lv_obj_set_width(s_console_label, LV_PCT(100));
    lv_obj_set_style_text_color(s_console_label, lv_color_hex(0xd0d0ff), 0);
    lv_label_set_text(s_console_label, "");
}

static void on_canvas(lv_event_t *e) {
    lv_point_t p; lv_indev_t *in = lv_indev_active();
    if (!in) return;
    lv_indev_get_point(in, &p);
    ev_push(s_q, lv_event_get_code(e) == LV_EVENT_LONG_PRESSED ? EV_LONG : EV_TAP, p.x / LILI_SCALE, p.y / LILI_SCALE);   /* physical -> logical px */
}
void inspector_bind_canvas(lv_obj_t *canvas) {
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(canvas, on_canvas, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(canvas, on_canvas, LV_EVENT_LONG_PRESSED, NULL);
}

void inspector_log(const char *line) {
    if (s_nlines == 9) { memmove(s_lines[0], s_lines[1], sizeof s_lines[0] * 8); s_nlines = 8; }
    snprintf(s_lines[s_nlines++], sizeof s_lines[0], "%s", line);
    static char all[9 * 74]; all[0] = 0;
    for (int i = 0; i < s_nlines; i++) { strcat(all, s_lines[i]); if (i + 1 < s_nlines) strcat(all, "\n"); }
    lv_label_set_text(s_console_label, all);
}

void inspector_update(float fps, float frame_ms) {
    static const char *const VIEW[] = { "szoba", "udvar" };
    static const char *const SEASON[] = { "auto", "tavasz", "nyar", "osz", "tel" };
    static const char *const PART[] = { "auto", "reggel", "delutan", "este", "ejszaka" };
    static const char *const DIR[] = { "S", "SW", "W", "NW", "N", "NE", "E", "SE" };
    const world_t *w = s_world; const actor_t *a = &w->actors[0];
    uint32_t seen = 0; for (int i = 0; i < EV_COUNT; i++) seen += w->events_seen[i];
    lv_label_set_text_fmt(s_status,
        "fps %.1f   keret %.2f ms\n"
        "nezet: %s   ido: %.0f s\n"
        "Lili: %s  x %.0f  y %.0f  z %.0f  irany %s\n"
        "evszak: %s   napszak: %s\n"
        "esemenyek: %u   (eldobva %u)",
        fps, frame_ms, VIEW[w->view], w->clock_s,
        a->present ? VIEW[a->view] : "nincs", a->x, a->y, a->z, DIR[a->dir],
        SEASON[w->season_force + 1], PART[w->daypart_force + 1], (unsigned)seen, (unsigned)s_q->dropped);
}
