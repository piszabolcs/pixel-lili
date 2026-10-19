/* config.h - every tunable number of Pixel Lili, by name (spec 4.6).
 * Platform-free: the simulator and the firmware compile it unchanged. */
#ifndef LILI_CONFIG_H
#define LILI_CONFIG_H

/* ---- the glass (spec 7.2, measured in M1) ---- */
#define LILI_PHYS_W          480                          /* the CO5300's pixels */
#define LILI_PHYS_H          480
#define LILI_SCALE           2                            /* one logical pixel = 2 x 2 physical */
#define LILI_LOG_W           (LILI_PHYS_W / LILI_SCALE)   /* 240: the world's grid */
#define LILI_LOG_H           (LILI_PHYS_H / LILI_SCALE)
#define LILI_CORNER_R_PHYS   72                           /* M1.7, 2026-10-19: the 64 px arc is cut, 72 shows whole */
#define LILI_CORNER_R        (LILI_CORNER_R_PHYS / LILI_SCALE)

/* ---- the actors (spec 11.5: Bella-ready, one dog in v1) ---- */
#define MAX_ACTORS           1

/* ---- switches (spec 4.6) ---- */
#define CONFIG_LILI_THOUGHT_BUBBLES 1
#define CONFIG_LILI_SEARCH_HINT     0                     /* D-06: off, find her yourself */
#define CONFIG_LILI_MIC             1
#define CONFIG_LILI_NAME_WAKEWORD   0                     /* D-07: experimental, off */

#endif
