/* audio_216.h - Lili's sounds out of the 2.16's speaker (2026-10-29, spec 5.9, M9.2): common/lili/sfx.c mixes, a
 * task here sends it through the I2S TX the mic's controller runs beside its RX (mic_es7210.c mic_with_tx: the
 * ES8311 and the ES7210 share MCLK 42, BCLK 9, LRCK 45; the speaker's data goes out on 8), the ES8311 (codec_port.c)
 * and the NS4150B amplifier (its CTRL on 46). The codec and the amp are up only while something plays and for
 * IDLE_MS after (the pops stay inside the silence written around them). While it plays and SELF_MUTE_MS after,
 * the mic does not listen (spec 5.8: her own bark is not a whistle). */
#ifndef AUDIO_216_H
#define AUDIO_216_H
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "pak.h"

void audio216_before_mic(void);                               /* before mic_init: the TX beside the mic's RX */
bool audio216_init(i2c_master_bus_handle_t bus, const pak_t *pak);   /* after mic_init; false = silent */
bool audio216_self_mute(void);                                /* the speaker is (just) playing: the mic does not listen */
void audio216_set_volume(int level);                         /* 0 off, 1 quiet, 2 normal (NVS "lili" / "snd") */
int  audio216_volume(void);
#endif
