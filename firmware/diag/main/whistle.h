/* whistle.h - the spec 5.8 whistle detector on 32 ms frames: a 512-point FFT
 * of the left microphone, the 1000-3500 Hz band's peak against its mean
 * (> 12 dB), the peak frequency stable within +-15 % for >= 250 ms (8 frames). */
#ifndef WHISTLE_H
#define WHISTLE_H
#include <stdint.h>
#include <stdbool.h>
#define WH_N      512
#define WH_RATE   16000
#define WH_BIN0   32            /* 1000 Hz at 31.25 Hz / bin */
#define WH_BIN1   112           /* 3500 Hz */
#define WH_BAND   (WH_BIN1 - WH_BIN0 + 1)
#define WH_FULL   64            /* the whole 0-8 kHz spectrum in 64 bars of 4 bins */
typedef struct {
    float rms_db_l, rms_db_r;   /* dBFS of the frame */
    float band_db[WH_BAND];     /* the band's bins, dBFS (a full-scale sine's peak bin = 0) */
    float full_db[WH_FULL];
    float peak_hz, peak_db, mean_db, ratio_db;
    float frac;                 /* tonality: energy of the peak's +-2 bins / the whole band's (a whistle ~1, speech low) */
    float low_rel_db;           /* 94-875 Hz energy relative to the peak bin, dB (a whistle has little low end) */
    int   streak;               /* frames the candidate has held */
    bool  tone;                 /* this frame passes the ratio + floor gates */
    bool  shown;                /* "FÜTTY" is being shown (hold after an event) */
    int   count;                /* whistles detected since boot */
    int64_t last_us;            /* esp_timer stamp of the last event */
    uint32_t frames;
} whistle_state_t;
typedef struct { float thr_db; int frames; float tol; float floor_db; } whistle_params_t;
void whistle_init(void);
void whistle_frame(const int16_t *left, const int16_t *right);   /* WH_N frames; from the audio task */
void whistle_get(whistle_state_t *out);                           /* a consistent copy, from any task */
whistle_params_t *whistle_params(void);
/* the step-5 measurement: one CSV line per frame on the console, tagged with a label
 * (0 none, 1 silence, 2 whistle, 3 speech, 4 put down / pick up, 5 knock) */
void whistle_set_log(bool on);
void whistle_set_label(int label);
#endif
