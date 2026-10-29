/* sfx.c - see sfx.h */
#include "sfx.h"
#include "asset_ids.h"
#include "lili_world.h"
#include <string.h>

typedef struct { uint8_t first, n, loop, pad; uint16_t gain_q8, cool_ms; char name[12]; } bank_cue_t;
typedef struct { const uint8_t *d; uint32_t pos, len; int32_t gain_q8; int16_t fade; uint8_t cue, live; } voice_t;

static const bank_cue_t *s_cues; static const uint8_t *s_clips, *s_data; static int s_n_cues, s_n_clips;
static int16_t s_ulaw[256];
static voice_t s_v[SFX_VOICES];
static int s_volume = 2, s_master_q8 = 256;
static uint32_t s_now_ms, s_last_ms[SFX_COUNT > 0 ? SFX_COUNT : 1];
static uint8_t s_last_var[SFX_COUNT > 0 ? SFX_COUNT : 1];
static uint16_t s_count[SFX_COUNT > 0 ? SFX_COUNT : 1];
static uint32_t s_rng = 0x5EED1234u;
static void (*s_lock)(bool);

static int q8_of_db(int db) {                                 /* 256 * 10^(db / 20), for whole dB in -40..+12 */
    static const uint16_t T[] = { 256, 228, 203, 181, 161, 144, 128, 114, 102, 91, 81, 72, 64, 57, 51, 46, 41, 36, 32, 29, 26 };
    if (db >= 0) { int g = 256; for (int i = 0; i < db && i < 12; i++) g = g * 287 / 256; return g; }
    int g = 256; db = -db;
    while (db > 20) { g = g * 26 / 256; db -= 20; }
    return g * T[db] / 256;
}
static uint32_t rnd(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static void lock(bool on) { if (s_lock) s_lock(on); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

bool sfx_init(const pak_t *p, int master_db) {
    uint32_t size = 0; const uint8_t *b = p ? pak_find(p, PAK_T_SOUND, 1, &size) : NULL;
    s_cues = NULL;
    if (!b || size < 12 || memcmp(b, "LSND", 4)) return false;
    s_n_cues = b[6] | b[7] << 8; s_n_clips = b[8] | b[9] << 8;
    if (s_n_cues != SFX_COUNT) return false;                  /* the pack and the code disagree: rebuild both */
    s_cues = (const bank_cue_t *)(b + 12);                    /* 20 B each, 2-aligned: read field by field below */
    s_clips = b + 12 + 20 * s_n_cues; s_data = s_clips + 8 * s_n_clips;
    for (int i = 0; i < 256; i++) {                           /* G.711 mu-law */
        const int u = ~i & 0xFF, t = (((u & 0x0F) << 3) + 0x84) << ((u & 0x70) >> 4);
        s_ulaw[i] = (int16_t)((u & 0x80) ? 0x84 - t : t - 0x84);
    }
    s_master_q8 = q8_of_db(master_db);
    memset(s_v, 0, sizeof s_v); memset(s_count, 0, sizeof s_count); memset(s_last_ms, 0, sizeof s_last_ms);
    s_now_ms = 100000;                                        /* so a cue's first start is never "just played" */
    return true;
}
void sfx_set_lock(void (*l)(bool)) { s_lock = l; }
void sfx_set_volume(int level) { s_volume = level < 0 ? 0 : level > 2 ? 2 : level; }
int  sfx_volume(void) { return s_volume; }
uint16_t sfx_count(int cue) { return cue >= 0 && cue < SFX_COUNT ? s_count[cue] : 0; }

void sfx_play(int cue, int gain_db) {
    if (!s_cues || cue < 0 || cue >= s_n_cues || !s_volume) return;
    const uint8_t *c = (const uint8_t *)&s_cues[cue];
    const int first = c[0], n = c[1], gain = c[4] | c[5] << 8, cool = c[6] | c[7] << 8;
    if (!n || (s_last_ms[cue] && (int32_t)(s_now_ms - s_last_ms[cue]) < cool)) return;
    int k = n > 1 ? (int)(rnd() % (uint32_t)(n - 1)) : 0;
    if (n > 1 && k >= s_last_var[cue]) k++;                  /* not the one before */
    const uint8_t *cl = s_clips + 8 * (first + k);
    lock(true);
    int slot = -1;
    for (int i = 0; i < SFX_VOICES; i++) if (!s_v[i].live) { slot = i; break; }
    if (slot < 0)                                             /* all busy: the one nearest its end makes room (a short care sound is not lost) */
        for (int i = 0, best = -1; i < SFX_VOICES; i++) { const int left = (int)(s_v[i].len - s_v[i].pos); if (best < 0 || left < best) { best = left; slot = i; } }
    if (slot >= 0) {
        voice_t *v = &s_v[slot];
        v->d = s_data + rd32(cl); v->len = rd32(cl + 4); v->pos = 0; v->fade = 0; v->cue = (uint8_t)cue; v->live = 1;
        v->gain_q8 = (int32_t)gain * q8_of_db(gain_db) / 256 * s_master_q8 / 256 * (s_volume == 1 ? 64 : 256) / 256;
        s_last_ms[cue] = s_now_ms; s_last_var[cue] = (uint8_t)k; s_count[cue]++;
    }
    lock(false);
}
void sfx_stop_all(void) { lock(true); for (int i = 0; i < SFX_VOICES; i++) s_v[i].live = 0; lock(false); }
bool sfx_active(void) { for (int i = 0; i < SFX_VOICES; i++) if (s_v[i].live) return true; return false; }

int sfx_render(int16_t *out, int n) {
    enum { FADE = SFX_RATE / 200 };                           /* 5 ms in and out: a hard edge clicks on the small speaker */
    int live = 0;
    for (int i = 0; i < n; i++) {
        int32_t acc = 0;
        for (int k = 0; k < SFX_VOICES; k++) {
            voice_t *v = &s_v[k];
            if (!v->live) continue;
            int32_t s = s_ulaw[v->d[v->pos]] * v->gain_q8 / 256;
            const uint32_t left = v->len - v->pos;
            if (v->fade < FADE) { s = s * v->fade / FADE; v->fade++; }
            if (left < FADE) s = s * (int32_t)left / FADE;
            acc += s;
            if (++v->pos >= v->len) v->live = 0;
        }
        out[i] = (int16_t)(acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc);
    }
    for (int k = 0; k < SFX_VOICES; k++) live += s_v[k].live;
    return live;
}

/* ---- the moments ---- */
/* an animation's sound: at `at` s after it starts, then every `every` s while it plays (0 = once); a second cue
   takes turns with the first (the petting: a whimper, then a snorting breath) */
typedef struct { uint16_t anim; int8_t cue, cue2; float at, every; } rule_t;
static const rule_t RULES[] = {
    { ANIM_BARK_HEAD_FLIP, SFX_BARK, -1, 0.2f, 0 },           /* the head up (spec A12: the 3rd frame) */
    { ANIM_HUFF, SFX_HUFF, -1, 0.1f, 0 },                     /* A13: the 2nd frame */
    { ANIM_SNIFF, SFX_SNIFF, -1, 0.0f, 2.6f },
    { ANIM_PET_ENJOY, SFX_WHIMPER, -1, 0.4f, 3.0f },          /* the snorting breath out (the owner, 2026-10-29: "scary") */
    { ANIM_PANT_SMILE, SFX_PANT, -1, 0.0f, 1.8f },
    { ANIM_SLEEP_CURL, SFX_SNORE, -1, 1.5f, 4.5f },
    { ANIM_ASK_PET, SFX_WHINE, -1, 0.3f, 4.0f },
    { ANIM_GUILTY_PEEK, SFX_WHINE, -1, 1.0f, 6.0f },
    { ANIM_STARTLE, SFX_YELP, -1, 0.0f, 0 },
    { ANIM_TUG_ROPE, SFX_GROWL, -1, 0.2f, 2.4f },
    { ANIM_CURL_UP, SFX_YAWN, -1, 0.2f, 0 },
    { ANIM_EAT_DRINK, SFX_EAT, -1, 0.0f, 2.0f },
    { ANIM_DRINK, SFX_DRINK, -1, 0.0f, 2.0f },
    { ANIM_EAT_TREAT, SFX_CRUNCH, -1, 0.1f, 0 },
    { ANIM_CHEW_TREAT, SFX_CHEW, -1, 0.3f, 2.2f },
    { ANIM_CHEW_TOY, SFX_CHEW, -1, 0.3f, 2.2f },
};
static uint16_t s_anim; static float s_t, s_next; static uint8_t s_turn;
void sfx_tick(world_t *w, float dt) {
    s_now_ms += (uint32_t)(dt * 1000.0f + 0.5f);
    if (!s_cues) return;
    const actor_t *a = &w->actors[0];
    if (!a->present) return;
    if (a->anim != s_anim || a->anim_t + 1e-4f < s_t) {      /* it started (again) */
        s_anim = a->anim; s_turn = 0; s_next = -1;
        for (size_t i = 0; i < sizeof RULES / sizeof RULES[0]; i++) if (RULES[i].anim == a->anim) { s_next = RULES[i].at; break; }
    }
    s_t = a->anim_t;
    if (s_next < 0 || a->anim_t < s_next) return;
    for (size_t i = 0; i < sizeof RULES / sizeof RULES[0]; i++) {
        const rule_t *r = &RULES[i];
        if (r->anim != a->anim) continue;
        const int cue = r->cue2 >= 0 && (s_turn & 1) ? r->cue2 : r->cue;
        const bool voice = cue < SFX_EAT;                     /* her own voice (the bank's first cues): quieter at night */
        sfx_play(cue, (voice && world_daypart(w) == 3 ? -10 : 0) + (a->view != w->view ? -12 : 0));
        s_turn++; s_next = r->every > 0 ? a->anim_t + r->every : 1e9f;
        break;
    }
}
