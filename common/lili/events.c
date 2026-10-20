/* events.c - see events.h. A ring: `head` is the oldest event, `count` how many are waiting. */
#include "events.h"
#include <string.h>

void ev_init(ev_queue_t *q) { memset(q, 0, sizeof *q); }

void ev_push(ev_queue_t *q, ev_type_t type, int a, int b) {
    if (q->count == EV_QUEUE_LEN) {                    /* full: drop the oldest, keep the newest */
        q->head = (uint8_t)((q->head + 1) % EV_QUEUE_LEN);
        q->count--; q->dropped++;
    }
    ev_t *e = &q->q[(q->head + q->count) % EV_QUEUE_LEN];
    e->type = (uint8_t)type; e->a = (int16_t)a; e->b = (int16_t)b;
    q->count++;
}

bool ev_pop(ev_queue_t *q, ev_t *out) {
    if (!q->count) return false;
    *out = q->q[q->head];
    q->head = (uint8_t)((q->head + 1) % EV_QUEUE_LEN);
    q->count--;
    return true;
}

const char *ev_name(ev_type_t t) {
    static const char *const N[EV_COUNT] = {
        "NONE", "TAP", "LONG", "PET_START", "PET_TICK", "PET_END", "FLICK", "EDGE_SWIPE",
        "PICKUP", "SHAKE", "WHISTLE", "CALL_NAME", "VOICE_ACTIVITY",
        "SWITCH_VIEW", "TIME_SKIP", "SET_SEASON", "SET_DAYPART",
    };
    return t < EV_COUNT ? N[t] : "?";
}
