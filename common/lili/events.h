/* events.h - everything that happens TO the world: a finger, a shake, a whistle, a key
 * of the simulator's inspector (spec 5.3, 5.5, 5.8, 6.3, 8.8).
 *
 * One small ring buffer. The producers (the touch driver, the IMU, the microphone, the
 * simulator's mouse and inspector) push; the one consumer (the frame loop) pops and hands
 * each event to the world. So the world never knows who sent it, and a recorded stream of
 * events replays the same life (the selftests use that). Platform-free, no malloc. */
#ifndef LILI_EVENTS_H
#define LILI_EVENTS_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    EV_NONE = 0,
    /* the finger (spec 5.3, gesture.c); a, b = logical x, y, except: FLICK a, b = vx, vy (px / s), c = the toy;
       DRAG / DROP c = the toy (the finger moves it / lets it go); APPROACH a = the finger's distance to her */
    EV_TAP, EV_LONG, EV_PET_START, EV_PET_TICK, EV_PET_END, EV_FLICK, EV_EDGE_SWIPE, EV_DRAG, EV_DROP, EV_APPROACH,
    /* the body (spec 5.5) */
    EV_PICKUP, EV_SHAKE,
    /* the voice (spec 5.8) */
    EV_WHISTLE, EV_CALL_NAME, EV_VOICE_ACTIVITY,
    /* the buttons and the simulator */
    EV_SWITCH_VIEW,            /* KEY3 / Tab / an edge swipe */
    EV_TIME_SKIP,              /* a = seconds to add to the world's clock (the sim's T key: +1 h) */
    EV_SET_SEASON,             /* a = 0..3 spring..winter, -1 = from the date (spec 9.4) */
    EV_SET_DAYPART,            /* a = 0..3 morning..night, -1 = from the clock (spec 9.3) */
    EV_COUNT
} ev_type_t;

typedef struct { uint8_t type; int16_t a, b, c; } ev_t;

#define EV_QUEUE_LEN 32       /* a power of two; a full queue drops the OLDEST (fresh input beats stale) */
typedef struct { ev_t q[EV_QUEUE_LEN]; uint8_t head, count; uint32_t dropped; } ev_queue_t;

void        ev_init(ev_queue_t *q);
void        ev_push(ev_queue_t *q, ev_type_t type, int a, int b);
void        ev_push3(ev_queue_t *q, ev_type_t type, int a, int b, int c);
bool        ev_pop(ev_queue_t *q, ev_t *out);
const char *ev_name(ev_type_t type);

#endif
