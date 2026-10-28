/* config.h - every tunable number of Pixel Lili, by name (spec 4.6).
 * Platform-free: the simulator and the firmware compile it unchanged. */
#ifndef LILI_CONFIG_H
#define LILI_CONFIG_H

/* ---- the glass (spec 7.2, measured in M1) ---- */
#define LILI_PHYS_W          480                          /* the CO5300's pixels */
#define LILI_PHYS_H          480
#define LILI_SCALE           1                            /* one logical pixel = one physical (2026-10-21, the owner: the 2x look was too coarse; it was 2 until M3.4) */
#define LILI_LOG_W           (LILI_PHYS_W / LILI_SCALE)   /* 240: the world's grid */
#define LILI_LOG_H           (LILI_PHYS_H / LILI_SCALE)
#define LILI_DESIGN_W        240                          /* the maps (assets/maps JSON) and the spec's layout numbers are written on this grid ... */
#define LILI_K               (LILI_LOG_W / LILI_DESIGN_W)  /* ... and scaled by this (2) into logical px by tools/gen_maps.py and the code that draws by hand */
#define LILI_CORNER_R_PHYS   72                           /* M1.7, 2026-10-19: the 64 px arc is cut, 72 shows whole */
#define LILI_CORNER_R        (LILI_CORNER_R_PHYS / LILI_SCALE)

/* ---- time and light (spec 7.2, 9.3, 9.4; the painted world, 2026-10-23) ---- */
#define LILI_SEASON_DEFAULT  2                            /* autumn: the first art round (spec 7.1); the date replaces it with the RTC (M5) */
#define LILI_START_HOUR      14.0f                        /* the world clock starts in the afternoon until the RTC sets it (M5) */
#define LILI_DEFAULT_DATE    2026, 10, 1                  /* ... on an autumn day: the tests' and a clockless board's date (life.c) */
#define LILI_ROOM_FLOOR_Y    166                          /* the painted room's wall ends here (logical px): no shadow above it */
#define LILI_ROOM_LIGHT_X    236                          /* the door's glass: by day the room's light comes from here, */
#define LILI_ROOM_LIGHT_Y    120                          /* so shadows fall towards the viewer and away from the door (the owner) */
#define LILI_SHADOW_HMAX     60                           /* a sprite pixel higher than this above the paws throws no longer shadow */
#define LILI_SPRITE_CELL_MAX 96                           /* the largest sprite cell side: the shadow decodes a frame into a buffer this big (the room's sets are 0.75 of the 128 px masters, cropped) */

/* ---- the actors (spec 11.5: Bella-ready, one dog in v1) ---- */
#define MAX_ACTORS           1
/* spec 8.4's gait speeds are for the 240 design grid's dog; the painted world's Lili (0.75 of her 128 px
   masters) strides this much of that per second: the demo's walk the owner saw on 2026-10-23 (38 px/s) */
#define LILI_NAV_SPEED_SCALE 0.6f

/* ---- switches (spec 4.6) ---- */
#define CONFIG_LILI_THOUGHT_BUBBLES 1
#define CONFIG_LILI_PLUSH_AWAY      0                     /* the owner (2026-10-28): it was in the toy box without a picture; back since the owner's Gemini sheet (09.jpg) */
#define CONFIG_LILI_SEARCH_HINT     0                     /* D-06: off, find her yourself */
#define CONFIG_LILI_MIC             1
#define CONFIG_LILI_NAME_WAKEWORD   0                     /* D-07: experimental, off */

#endif
