/* brain.h - Lili's sixth layer (spec 10, M6): the brain's input (the world encoded as 24 tokens), the teacher's
 * rule brain (model/teacher_rules.yaml -> brain_rules_gen.c: score, softmax, sample), and the life loop that asks it
 * (spec 10.6: when the plan ends or fails, a need crosses its line, a signal comes, she changes view, a spot appears,
 * the owner comes or goes; at the earliest 3 s after the last question, at the latest 20 s) and hesitates when the
 * two best answers are close (spec 8.7: a head tilt first). The LLM (M7-M8) will answer the same question.
 * Platform-free, no malloc. */
#ifndef LILI_BRAIN_H
#define LILI_BRAIN_H
#include "lili_types.h"
#include "schema_gen.h"

typedef struct { uint8_t f[F_COUNT]; } brain_state_t;      /* each field's value: its token's index, or a bucket 0..9 */
void brain_encode(const world_t *w, int actor, brain_state_t *s);
int  brain_prompt(const brain_state_t *s, uint8_t *ids);   /* <bos> F1 .. F24 => : F_COUNT + 2 token ids */

/* the teacher's templates (generated) */
typedef struct {
    const char *id, *habit;
    int   (*when)(const uint8_t *f);
    float (*score)(const uint8_t *f);
    uint8_t alt[3][4], n_alt[3];
    uint8_t style, face, bubble, urg, kind;
} brain_rule_t;
extern const brain_rule_t BRAIN_RULES[];
extern const int BRAIN_N_RULES;

#define BRAIN_TAU          0.6f      /* the softmax temperature (spec 10.7) */
#define BRAIN_HESITATE     0.15f     /* top-2 margin under this: a head tilt first (spec 8.7) */
#define BRAIN_ASK_MIN_S    3.0f
#define BRAIN_ASK_MAX_S    20.0f
/* one decision from the rules for the world as it is; the chosen template's index (-1 = none could run) and the
   top-2 margin of the softmax */
int  brain_rules_decide(world_t *w, int actor, const brain_state_t *s, decision_t *out, float *margin);
/* the probabilities each template had in the last decision (for the probes and the traces) */
const float *brain_last_probs(void);

enum { OWNER_BACK_NONE = 0, OWNER_BACK_SHORT, OWNER_BACK_LONG };
void brain_owner_back(world_t *w, int64_t away_s);         /* after the time away: the next question says so (F16) */
void brain_signal(world_t *w, const ev_t *e);              /* an event's signal for the next question (F18) */
void brain_tick(world_t *w, float dt);                     /* the life loop: ask when it is time, start the plan */
/* the same loop in pieces, for a brain that answers later (the panel's model on Core 1): due -> encode + mask ->
   asked -> (the answer) -> apply */
bool brain_due(world_t *w, float dt);
void brain_asked(world_t *w);
void brain_apply(world_t *w, decision_t *d, float margin, int rule);
void brain_env_mask(world_t *w, int actor, uint8_t *mask);   /* the actions possible now (ACT_COUNT bytes) */
void brain_ask_now(world_t *w);                            /* ask at the next tick (a trigger the loop cannot see) */
/* the learned brain (brain_llm.c, M8): the 4-bit student model on the pocket-tank's engine. load checks the file's
   schema trailer; decide answers like the rules (0 = ok, -1 = no model or no possible action). brain_use_llm makes
   the life loop ask the model instead of the rules (the rules stay the fallback) */
#include <stddef.h>
bool        brain_llm_load(const uint8_t *bin, size_t len, void *(*alloc)(size_t));
bool        brain_llm_ready(void);
const char *brain_llm_status(void);
uint32_t    brain_vocab_hash(void);
int         brain_llm_decide(world_t *w, int actor, const brain_state_t *s, decision_t *out, float *margin, float temp);
/* the same with the mask given and its own PRNG state: touches no world (another core may call it) */
int         brain_llm_decide_masked(const brain_state_t *s, const uint8_t *mask, uint32_t *rng, decision_t *out, float *margin, float temp);
void        brain_use_llm(bool on);
const float *q4_model_prefill_lili(const int *toks, int n);   /* the logits after a prompt (tests) */
#define BRAIN_TEMP 0.8f                                    /* spec 10.5 */
#define BRAIN_RULE_LLM 100                                 /* world_t.rule after a model's decision */

/* a world set up to match a brain state (the probes, the traces' perturbations): the place, the clock's daypart
   and season and day, the needs, the things, the owner, the toys */
void brain_world_from_state(world_t *w, const brain_state_t *s);

#endif
