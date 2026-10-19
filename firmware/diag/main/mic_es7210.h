/* mic_es7210.h - the 2.16's ES7210 microphone ADC (I2C 0x40, on A3V3 = ALDO1)
 * fed to an I2S RX channel: 16 kHz, 16-bit, MIC1 in the left slot, MIC2 in
 * the right (spec 5.8). The ESP is the I2S master: MCLK 42 (256 fs =
 * 4.096 MHz), BCLK 9, LRCK 45, data in on 10 (ASDOUT, the schematic). */
#ifndef MIC_ES7210_H
#define MIC_ES7210_H
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
bool mic_init(i2c_master_bus_handle_t bus);        /* false = no ES7210 answered */
bool mic_up(void);                                 /* registers + the channel on (boot, and after a sleep) */
void mic_down(void);                               /* the channel off, the chip suspended (the pocket-tank's set) */
bool mic_is_up(void);
/* n stereo frames into left / right; returns the frames read (0 on timeout) */
int  mic_read(int16_t *left, int16_t *right, int n, int timeout_ms);
void mic_set_gain(int code);                       /* 0..14: 0, 3, 6 ... 30 (10), 33, 34.5, 36, 37.5 dB */
int  mic_gain(void);
void mic_dump(void);                               /* the registers that matter, to the log */
#endif
