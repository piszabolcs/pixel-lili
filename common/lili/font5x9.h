/* font5x9.h - the 5x9 pixel font with the Hungarian accents (font5x9.c) */
#ifndef LILI_FONT5X9_H
#define LILI_FONT5X9_H
#include <stdint.h>
int font_glyph(uint32_t cp, uint16_t cols[5]);   /* a code point's 5 columns (bit n = row n, 9 rows); 0 = no glyph */
#endif
