/* pak.h - the asset pack in memory (spec 7.6, LPAK v1 written by tools/pack_assets.py).
 *
 * The pack is one buffer the platform provides (the simulator reads the file, the firmware maps
 * its partition into PSRAM); nothing is copied out of it: a sprite frame is decoded straight
 * from its RLE bytes into the 8-bit canvas. Platform-free, no malloc. */
#ifndef LILI_PAK_H
#define LILI_PAK_H
#include <stdint.h>
#include <stdbool.h>
#include "render8.h"
#include "../asset_ids.h"

typedef struct { const uint8_t *data; uint32_t size; uint16_t n; uint32_t toc; } pak_t;

typedef struct {
    const uint8_t *base;         /* the set's payload */
    uint16_t anim_id;
    uint8_t  n_dirs, n_frames, cell_w, cell_h, pivot_x, pivot_y, fps, loop, dir_mask;
} sprite_set_t;

typedef struct {
    const uint8_t *rle; uint16_t rle_len;
    uint8_t mouth_x, mouth_y, head_x, head_y, head_w, head_h;
} sprite_frame_t;

/* validates the magic, the size and the CRC-32; false = boot with the placeholder graphics (spec 7.6) */
bool     pak_open(pak_t *p, const uint8_t *data, uint32_t size);
const uint8_t *pak_find(const pak_t *p, uint8_t type, uint16_t id, uint32_t *size);   /* NULL if absent */
bool     pak_palette(const pak_t *p, uint8_t out[256][3]);
bool     pak_sprite_set(const pak_t *p, uint16_t anim_id, sprite_set_t *out);
/* dir is a dir8_t; a direction the set lacks falls back to the nearest one it has (spec 7.4: 3-direction animations) */
bool     pak_frame(const sprite_set_t *s, int dir, int frame, sprite_frame_t *out);
/* the frame onto the canvas with its pivot at logical (x, y); index 0 skipped; clipped */
void     pak_blit(render8_t *r, const sprite_set_t *s, const sprite_frame_t *f, int x, int y);
uint32_t pak_crc32(const uint8_t *d, uint32_t n);

#endif
