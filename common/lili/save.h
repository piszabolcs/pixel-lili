/* save.h - Lili's save (spec 9.7, M5.5): a versioned blob with a CRC, that may only grow at its end (the
 * pocket-tank's rule): a newer firmware reads an older save (the missing tail gets its defaults), an older one
 * reads the fields it knows. The platform keeps the bytes: an NVS blob "lili_save" on the panel, a file in the
 * simulator. Written every 5 minutes, before sleep and power-off, and after a care event. Platform-free. */
#ifndef LILI_SAVE_H
#define LILI_SAVE_H
#include "lili_types.h"

#define SAVE_VERSION 1
#define SAVE_MAX     256
int  save_write(const world_t *w, int64_t first_start, uint8_t *buf, int cap);   /* the bytes' length, 0 = too small */
/* true = a valid save (its magic and CRC); the world gets what it holds; *saved_at = its local time stamp,
   *first_start = the day the device first ran (statistics) */
bool save_read(world_t *w, const uint8_t *buf, int len, int64_t *saved_at, int64_t *first_start);

#endif
