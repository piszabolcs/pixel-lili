/* palette.h - the index map of the 256-colour palette (spec 7.2): 0 transparent,
 * 1-15 UI and outline, then ramps of 8 shades per material (dark -> light).
 * M2.2 ships a placeholder palette; M3 replaces the numbers from the photos
 * (tools/palette_from_photos.py -> assets/palettes/master.gpl), not the map. */
#ifndef LILI_PALETTE_H
#define LILI_PALETTE_H
#include <stdint.h>

enum {
    PAL_CLEAR   = 0,        /* transparent in sprites */
    PAL_OUTLINE = 1,        /* the 1 px dark brown contour, #3B2418 (spec 7.1) */
    PAL_BLACK   = 2,
    PAL_WHITE   = 3,
    PAL_DEBUG   = 4,        /* magenta: debug layers only */
    PAL_RAMP    = 8,        /* shades per ramp */
    PAL_FLOOR   = 16,       /* 16..23 */
    PAL_WALL    = 24,       /* 24..31 */
    PAL_GRASS   = 32,       /* 32..39 */
    PAL_HEDGE   = 40,       /* 40..47 */
    PAL_FUR     = 48,       /* 48..55: Lili's cream-fawn (L2) */
    PAL_FUR_W   = 56,       /* 56..63: her white chest and paws (L3) */
    PAL_WOOD    = 64, PAL_BLANKET = 72, PAL_METAL = 80, PAL_WATER = 88,
    PAL_PAD     = 96, PAL_HOUSE   = 104, PAL_FLOWER = 112, PAL_LEAF = 120,
    PAL_RUG_GREEN = 128, PAL_RUG_GRAY = 136,      /* the owner's rug: green, off-white, light and dark grey triangles */
};

/* the ramps in the order of tools/gen_maps.py RAMPS (RAMP_ID_*): ramp k starts at 16 + 8 k */
static inline uint8_t pal_ramp(int ramp_id, int shade) { return (uint8_t)(16 + ramp_id * PAL_RAMP + shade); }

/* fills a palette: the fixed slots, and each ramp from 35 % to 115 % of its base colour */
void pal_placeholder(uint8_t out[256][3]);
/* the master palette (M3.2, approved 2026-10-21): tools/palette_from_photos.py -> master.json -> palette_gen.c */
extern const uint8_t PAL_MASTER[256][3];
void pal_master(uint8_t out[256][3]);

#endif
