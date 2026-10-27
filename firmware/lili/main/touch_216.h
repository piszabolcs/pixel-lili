/* touch_216.h - the 2.16's touch chip for the Lili world (see touch_216.c) */
#ifndef LILI_TOUCH_216_H
#define LILI_TOUCH_216_H
#include <stdbool.h>
bool touch216_init(void);                /* the I2C bus, the chip's reset; false = no chip (no touch, the game still runs) */
bool touch216_read(int *x, int *y);      /* once a frame: true while a finger is down, and where (the picture's logical px) */
#endif
