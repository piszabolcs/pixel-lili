/* map.h - a place (room, yard) as the game sees it at run time (spec 7.6 MAP, 8.2-8.5).
 * The data comes from assets/maps/<name>.json through tools/gen_maps.py into
 * maps_gen.c: const tables, no parsing on the device. */
#ifndef LILI_MAP_H
#define LILI_MAP_H
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define MAP_CELL   4                                   /* the walkable grid: 4 x 4 logical px cells (spec 8.4) */
#define MAP_GW     (LILI_LOG_W / MAP_CELL)             /* 60 */
#define MAP_GH     (LILI_LOG_H / MAP_CELL)
#define MAP_WALK_BYTES ((MAP_GW * MAP_GH + 7) / 8)     /* 450: one bit per cell */

typedef struct { int16_t x, y, z; } map_point_t;
typedef struct { const char *name; int16_t x, y, w, h; uint8_t ramp; uint8_t height; uint8_t flat; int16_t sort_y; uint8_t pattern; } map_prop_t;   /* pattern 0 box, 1 rug */
typedef struct { const char *name; uint8_t kind; int16_t a, b, c, d; const int16_t *poly; uint8_t n; } map_zone_t;   /* kind 0 rect (a b c d = x y w h), 1 polygon */

typedef struct {
    const char *name;
    uint8_t floor_ramp, wall_ramp; int16_t wall_bottom;
    const uint8_t *walk;                               /* MAP_WALK_BYTES, bit set = walkable */
    const map_point_t *points;                         /* indexed by the generated SPOT_* ids; z < 0 = not in this map */
    const map_prop_t *props; uint8_t n_props;
    const map_zone_t *zones; uint8_t n_zones;
} map_t;

static inline bool map_walkable(const map_t *m, int lx, int ly) {
    if (lx < 0 || ly < 0 || lx >= LILI_LOG_W || ly >= LILI_LOG_H) return false;
    int c = (ly / MAP_CELL) * MAP_GW + lx / MAP_CELL;
    return (m->walk[c >> 3] >> (c & 7)) & 1;
}

#endif
