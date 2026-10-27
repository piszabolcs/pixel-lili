/* motion.h - the body's events from the accelerometer (spec 5.5, M5.6): STILL (on the desk), PICKUP (lifted from
 * still), SHAKE (three hard peaks in quick succession), IN_MOTION (carried: a pocket, a walk). The platform feeds one
 * sample at a time (milli-g, any rate around 30 Hz, with its time); out come EV_PICKUP and EV_SHAKE into the queue,
 * and the state for the energy modes (DESK: still; POCKET: in motion). Platform-free, no malloc. */
#ifndef LILI_MOTION_H
#define LILI_MOTION_H
#include <stdbool.h>
#include <stdint.h>
#include "events.h"

enum { MOTION_UNKNOWN = 0, MOTION_STILL, MOTION_HANDHELD, MOTION_IN_MOTION };
#define MOTION_HIST 160                    /* over 3 s of samples at up to 50 Hz */
typedef struct {
    uint8_t  state;                        /* MOTION_* */
    uint32_t still_since, move_since;      /* ms: when the last still / moving stretch began */
    float    ref[3];                       /* the gravity vector of the still stretch (mg) */
    uint8_t  n_peak; uint32_t peak_t[4];   /* the shake's hard peaks */
    bool     in_peak;
    uint32_t last_shake, last_pickup;
    uint8_t  n, i; struct { float a[3]; uint32_t t; } h[MOTION_HIST];
} motion_t;

void motion_init(motion_t *m);
void motion_feed(motion_t *m, const float mg[3], uint32_t t_ms, ev_queue_t *q);

#endif
