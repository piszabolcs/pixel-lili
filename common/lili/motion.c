/* motion.c - see motion.h. Spec 5.5's starting values, by name. */
#include "motion.h"
#include <math.h>
#include <string.h>

enum { M_STILL_MS = 3000, M_PICKUP_MS = 500, M_SHAKE_MS = 600, M_SHAKE_GAP_MS = 2000, M_PICKUP_GAP_MS = 3000, M_MOTION_MS = 3000 };
#define M_STILL_SD_MG   20.0f      /* the spread of |a| under 0.02 g for 3 s: on the desk */
#define M_LEVEL_DEG     25.0f      /* and lying near flat */
#define M_PICKUP_MG     150.0f     /* lifted: the vector moves 0.15 g ... */
#define M_PICKUP_DEG    10.0f      /* ... or tilts 10 degrees, within half a second of the still stretch */
#define M_SHAKE_MG      1800.0f    /* a hard peak */
#define M_MOVE_LO_MG    50.0f      /* carried: 0.05-0.5 g swings, again and again for 3 s */
#define M_MOVE_HI_MG    500.0f

void motion_init(motion_t *m) { memset(m, 0, sizeof *m); }

static float norm3(const float *a) { return sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
static float angle(const float *a, const float *b) {
    const float na = norm3(a), nb = norm3(b);
    if (na < 1 || nb < 1) return 0;
    float c = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (na * nb);
    c = c > 1 ? 1 : c < -1 ? -1 : c;
    return acosf(c) * 57.29578f;
}

void motion_feed(motion_t *m, const float a[3], uint32_t t, ev_queue_t *q) {
    m->h[m->i].a[0] = a[0]; m->h[m->i].a[1] = a[1]; m->h[m->i].a[2] = a[2]; m->h[m->i].t = t;
    m->i = (uint8_t)((m->i + 1) % MOTION_HIST); if (m->n < MOTION_HIST) m->n++;
    const float g = norm3(a);
    /* SHAKE: three peaks over 1.8 g within 600 ms (a peak = a crossing of the line upwards) */
    if (g > M_SHAKE_MG && !m->in_peak) {
        m->in_peak = true;
        m->peak_t[m->n_peak % 4] = t; m->n_peak++;
        int recent = 0; for (int k = 0; k < 4 && k < m->n_peak; k++) recent += t - m->peak_t[k] <= M_SHAKE_MS;
        if (recent >= 3 && t - m->last_shake > M_SHAKE_GAP_MS) { ev_push(q, EV_SHAKE, 0, 0); m->last_shake = t; m->n_peak = 0; }
    } else if (g < M_SHAKE_MG * 0.8f) m->in_peak = false;
    /* the spread of |a| over the last 3 s, and the mean vector */
    float sum = 0, sum2 = 0, mean[3] = { 0, 0, 0 }; int k = 0; uint32_t oldest = t;
    float lo = 1e9f, hi = -1e9f;
    for (int j = 0; j < m->n; j++) {
        const int idx = (m->i + MOTION_HIST - 1 - j) % MOTION_HIST;
        if (t - m->h[idx].t > M_STILL_MS) break;
        const float gg = norm3(m->h[idx].a);
        sum += gg; sum2 += gg * gg; for (int c = 0; c < 3; c++) mean[c] += m->h[idx].a[c];
        if (gg < lo) lo = gg;
        if (gg > hi) hi = gg;
        oldest = m->h[idx].t; k++;
    }
    if (!k) return;
    const float mu = sum / k, sd = sqrtf(fmaxf(0, sum2 / k - mu * mu));
    for (int c = 0; c < 3; c++) mean[c] /= k;
    const float up[3] = { 0, 0, 1000 };
    const bool long_enough = t - oldest >= M_STILL_MS - 200;
    /* PICKUP: from still, the sample moves 0.15 g or tilts 10 degrees off the still stretch's gravity */
    if (m->state == MOTION_STILL) {
        const float d[3] = { a[0] - m->ref[0], a[1] - m->ref[1], a[2] - m->ref[2] };
        if (norm3(d) > M_PICKUP_MG || angle(a, m->ref) > M_PICKUP_DEG) {
            if (t - m->last_pickup > M_PICKUP_GAP_MS) { ev_push(q, EV_PICKUP, 0, 0); m->last_pickup = t; }
            m->state = MOTION_HANDHELD; m->move_since = t;
            return;
        }
        for (int c = 0; c < 3; c++) m->ref[c] = 0.98f * m->ref[c] + 0.02f * a[c];   /* the desk's slow drift */
        return;
    }
    /* STILL: quiet and near flat for 3 s */
    const float tilt = angle(mean, up), flat = tilt < 90 ? tilt : 180 - tilt;    /* screen up or down: the board's z either way */
    if (long_enough && sd < M_STILL_SD_MG && flat < M_LEVEL_DEG && (m->state == MOTION_UNKNOWN || t - m->move_since >= M_STILL_MS)) {
        m->state = MOTION_STILL; m->still_since = t; memcpy(m->ref, mean, sizeof m->ref);
        return;
    }
    /* IN_MOTION: 0.05-0.5 g swings for 3 s (a pocket, a walk); otherwise in the hand */
    if (long_enough && hi - lo > M_MOVE_LO_MG && hi - lo < M_MOVE_HI_MG * 2 && sd > M_STILL_SD_MG) {
        if (m->state != MOTION_IN_MOTION) { m->state = MOTION_IN_MOTION; m->move_since = t; }
    } else if (m->state == MOTION_UNKNOWN) m->state = MOTION_HANDHELD;
}
