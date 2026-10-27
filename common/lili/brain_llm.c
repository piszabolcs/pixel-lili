/* brain_llm.c - the learned brain (spec 10.5, 10.12, M8): the student model on the pocket-tank's 4-bit engine
 * (common/llm/q4_model.c, unchanged), asked the same question as the rules. Constrained decoding: every answer slot
 * may only be a token of its class (100 % valid answers); the first action is also masked to the actions possible
 * where she is (the environment mask: the context, never an override), and the three actions are sampled at the
 * temperature (0.8, spec 10.5), the gait, face, bubble and urgency taken greedily. The model file carries a trailer
 * (model/export_lili_q4.py): a model of another schema or vocabulary is refused and the rules keep running. */
#include "brain.h"
#include "plan.h"
#include "lili_world.h"
#include "q4_model.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static q4_model_t *s_m;
static char s_why[80] = "no model";
const char *brain_llm_status(void) { return s_why; }
bool brain_llm_ready(void) { return s_m != NULL; }

uint32_t brain_vocab_hash(void) {                     /* FNV-1a over every token name and a newline (the export's) */
    uint32_t h = 2166136261u;
    for (int i = 0; i < SCHEMA_N_TOKENS; i++) {
        for (const char *c = SCHEMA_TOKENS[i]; *c; c++) { h ^= (uint8_t)*c; h *= 16777619u; }
        h ^= '\n'; h *= 16777619u;
    }
    return h;
}

bool brain_llm_load(const uint8_t *bin, size_t len, void *(*alloc)(size_t)) {
    s_m = NULL;
    if (!bin || len < 256 + 16) { snprintf(s_why, sizeof s_why, "no model file"); return false; }
    const uint8_t *t = bin + len - 16; uint32_t v[3]; memcpy(v, t + 4, sizeof v);
    if (memcmp(t, "LSCH", 4)) { snprintf(s_why, sizeof s_why, "no schema trailer: not a Lili model"); return false; }
    if (v[0] != SCHEMA_VERSION || v[1] != brain_vocab_hash() || v[2] != SCHEMA_N_TOKENS) {
        snprintf(s_why, sizeof s_why, "schema v%u / vocab %08x, the firmware's v%d / %08x: refused", (unsigned)v[0], (unsigned)v[1], SCHEMA_VERSION, (unsigned)brain_vocab_hash());
        return false;
    }
    q4_model_t *m = q4_model_open(bin, len - 16, alloc);
    if (!m) { snprintf(s_why, sizeof s_why, "the engine refused the file"); return false; }
    const q4_config_t *c = q4_model_config(m);
    if (c->vocab_size != SCHEMA_VOCAB || c->seq_len < F_COUNT + 2 + SLOT_COUNT) { snprintf(s_why, sizeof s_why, "vocab %d, seq %d: does not fit", c->vocab_size, c->seq_len); return false; }
    s_m = m;
    snprintf(s_why, sizeof s_why, "dim %d, %d layers, %.1fM", c->dim, c->n_layers, (len / 1e6) * 2 / 1.06);
    return true;
}

static uint32_t xs32(uint32_t *r) { uint32_t x = *r ? *r : 0x2545F491u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *r = x; }
/* a token of [first, first + n) from the logits: masked (mask[i] == 0: never), sampled at temp (<= 0: greedy) */
static int pick(uint32_t *rng, const float *lg, int first, int n, const uint8_t *mask, float temp, float *p1, float *p2) {
    float mx = -1e30f; int best = -1;
    for (int i = 0; i < n; i++) if ((!mask || mask[i]) && lg[first + i] > mx) { mx = lg[first + i]; best = i; }
    if (best < 0) return -1;
    float sum = 0, pr[64], tt = temp > 0 ? temp : 1.0f;
    for (int i = 0; i < n && i < 64; i++) { pr[i] = (!mask || mask[i]) ? expf((lg[first + i] - mx) / tt) : 0; sum += pr[i]; }
    float a = 0, b = 0;
    for (int i = 0; i < n && i < 64; i++) { pr[i] /= sum; if (pr[i] > a) { b = a; a = pr[i]; } else if (pr[i] > b) b = pr[i]; }
    if (p1) *p1 = a;
    if (p2) *p2 = b;
    if (temp <= 0) return best;
    float u = (xs32(rng) >> 8) * (1.0f / 16777216.0f);
    for (int i = 0; i < n && i < 64; i++) { u -= pr[i]; if (u <= 0 && pr[i] > 0) return i; }
    return best;
}

int brain_llm_decide_masked(const brain_state_t *s, const uint8_t *mask, uint32_t *rng, decision_t *out, float *margin, float temp) {
    if (!s_m) return -1;
    uint8_t ids[F_COUNT + 2]; int toks[F_COUNT + 2 + SLOT_COUNT];
    const int n = brain_prompt(s, ids);
    for (int i = 0; i < n; i++) toks[i] = ids[i];
    int any = 0; for (int a = 0; a < ACT_COUNT; a++) any += mask[a] != 0;
    if (!any) return -1;
    const float *lg = q4_model_prefill(s_m, toks, n);
    if (!lg) return -1;
    int pos = n, vals[SLOT_COUNT];
    for (int k = 0; k < SLOT_COUNT; k++) {
        float p1 = 1, p2 = 0;
        const int v = pick(rng, lg, SCHEMA_SLOT_FIRST[k], SCHEMA_SLOT_N[k], k == SLOT_A1 ? mask : NULL, k <= SLOT_A3 ? temp : 0, &p1, &p2);
        if (v < 0) return -1;
        vals[k] = v;
        if (k == SLOT_A1 && margin) *margin = p1 - p2;
        if (k + 1 < SLOT_COUNT) { lg = q4_model_forward(s_m, SCHEMA_SLOT_FIRST[k] + v, pos++); if (!lg) return -1; }
    }
    out->a[0] = (uint8_t)vals[SLOT_A1]; out->a[1] = (uint8_t)vals[SLOT_A2]; out->a[2] = (uint8_t)vals[SLOT_A3];
    out->style = (uint8_t)vals[SLOT_STYLE]; out->face = (uint8_t)vals[SLOT_FACE]; out->bubble = (uint8_t)vals[SLOT_BUBBLE];
    out->urg = (uint8_t)(vals[SLOT_URG] + 1); out->margin = margin ? *margin : 1;
    return 0;
}
int brain_llm_decide(world_t *w, int actor, const brain_state_t *s, decision_t *out, float *margin, float temp) {
    uint8_t mask[ACT_COUNT]; brain_env_mask(w, actor, mask);
    uint32_t rng = world_rand(w);
    return brain_llm_decide_masked(s, mask, &rng, out, margin, temp);
}

/* the raw logits after a prompt (lilisim --selftest-llm's Q4 vs fp32 check) */
const float *q4_model_prefill_lili(const int *toks, int n) { return s_m ? q4_model_prefill(s_m, toks, n) : NULL; }
