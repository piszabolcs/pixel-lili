/* sfx.h - Lili's sounds (2026-10-29, spec 11.1-11.3, M9.2). The bank is in the pack (PAK_T_SOUND 1,
 * tools/lili_sounds.py: the ElevenLabs takes the owner picked, G.711 mu-law, 16 kHz mono); a small mixer plays
 * them, and sfx_tick plays them at their moments: the animations' frames (the bark as her head goes up, the
 * whimper and the snorting breath while she is petted, the sniffs, the snores ...). The care, the toys and the
 * door call sfx_play themselves. Her own voice is 10 dB quieter at night and through a wall (the other view).
 * Platform-free, no malloc. sfx_render runs on the sound's side (the sim's SDL callback, the panel's I2S task);
 * the world's side locks around its calls through sfx_set_lock when the two are different threads. */
#ifndef LILI_SFX_H
#define LILI_SFX_H
#include <stdbool.h>
#include <stdint.h>
#include "pak.h"
#include "lili_types.h"

#define SFX_RATE   16000
#define SFX_VOICES 4

bool sfx_init(const pak_t *p, int master_db);   /* the bank from the pack; false = none: every sfx_* is a no-op */
void sfx_set_lock(void (*lock)(bool on));      /* the platform's lock around the mixer (NULL = one thread) */
void sfx_play(int cue, int gain_db);           /* a variant at random (not the one before), unless in its cooldown */
void sfx_stop_all(void);
int  sfx_render(int16_t *out, int n);          /* mix n mono samples; the voices still live after it (0 = silence) */
bool sfx_active(void);
void sfx_set_volume(int level);                /* 0 off, 1 quiet (-12 dB), 2 normal */
int  sfx_volume(void);
uint16_t sfx_count(int cue);                   /* how many times a cue started (the selftests) */
void sfx_tick(world_t *w, float dt);           /* her moments; called by world_tick */

#endif
