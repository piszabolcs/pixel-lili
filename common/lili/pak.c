/* pak.c - see pak.h and tools/pack_assets.py for the byte layout */
#include "pak.h"
#include "config.h"
#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

uint32_t pak_crc32(const uint8_t *d, uint32_t n) {        /* the zlib polynomial, bitwise: 2 MB takes ~100 ms on the S3, once at boot */
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

bool pak_open(pak_t *p, const uint8_t *data, uint32_t size) {
    memset(p, 0, sizeof *p);
    if (!data || size < 20 || memcmp(data, "LPAK", 4) != 0 || rd16(data + 4) != 1) return false;
    uint32_t total = rd32(data + 12);
    if (total > size || rd32(data + 8) + 12u * rd16(data + 6) > total) return false;
    if (pak_crc32(data + 20, total - 20) != rd32(data + 16)) return false;
    p->data = data; p->size = total; p->n = rd16(data + 6); p->toc = rd32(data + 8);
    return true;
}

const uint8_t *pak_find(const pak_t *p, uint8_t type, uint16_t id, uint32_t *size) {
    if (!p->data) return NULL;
    for (uint16_t i = 0; i < p->n; i++) {
        const uint8_t *e = p->data + p->toc + 12u * i;
        if (rd16(e) == id && e[2] == type) {
            uint32_t off = rd32(e + 4), sz = rd32(e + 8);
            if (off + sz > p->size) return NULL;
            if (size) *size = sz;
            return p->data + off;
        }
    }
    return NULL;
}

bool pak_palette(const pak_t *p, uint8_t out[256][3]) {
    uint32_t sz; const uint8_t *d = pak_find(p, PAK_T_PALETTE, PAL_ID_MASTER, &sz);
    if (!d || sz != 768) return false;
    memcpy(out, d, 768); return true;
}

bool pak_sprite_set(const pak_t *p, uint16_t anim_id, sprite_set_t *s) {
    uint32_t sz; const uint8_t *d = pak_find(p, PAK_T_SPRITE_SET, anim_id, &sz);
    if (!d || sz < 12) return false;
    s->base = d; s->anim_id = rd16(d); s->n_dirs = d[2]; s->n_frames = d[3]; s->cell_w = d[4]; s->cell_h = d[5];
    s->pivot_x = d[6]; s->pivot_y = d[7]; s->fps = d[8]; s->loop = d[9]; s->dir_mask = d[10];
    return s->n_dirs && s->n_frames;
}

bool pak_frame(const sprite_set_t *s, int dir, int frame, sprite_frame_t *f) {
    if (!s->base || s->n_frames == 0) return false;
    dir &= 7;
    int slot = -1, best = 99;
    for (int d = 0, k = 0; d < 8; d++) if (s->dir_mask & (1 << d)) {   /* the k-th present direction nearest to dir on the circle */
        int dist = (d - dir + 8) & 7; if (dist > 4) dist = 8 - dist;
        if (dist < best) { best = dist; slot = k; }
        k++;
    }
    if (slot < 0) return false;
    frame %= s->n_frames;
    const uint8_t *rec = s->base + 12 + 12 * (slot * s->n_frames + frame);   /* the 12-byte set header, then the records */
    f->rle = s->base + rd32(rec); f->rle_len = rd16(rec + 4);
    f->mouth_x = rec[6]; f->mouth_y = rec[7]; f->head_x = rec[8]; f->head_y = rec[9]; f->head_w = rec[10]; f->head_h = rec[11];
    return true;
}

void pak_blit(render8_t *r, const sprite_set_t *s, const sprite_frame_t *f, int x, int y) {
    const int W = s->cell_w, H = s->cell_h;
    int ox = x - s->pivot_x, oy = y - s->pivot_y;        /* the cell's top-left on the canvas */
    const uint8_t *p = f->rle, *end = f->rle + f->rle_len;
    int i = 0, n = W * H;
    while (i < n && p < end) {
        int c = *p++, run, lit;
        if (c < 128) { run = c + 1; lit = 1; } else { run = c - 126; lit = 0; }
        for (int k = 0; k < run && i < n; k++, i++) {
            uint8_t v = lit ? p[k] : p[0];
            if (v) {
                int px = ox + i % W, py = oy + i / W;
                if (px >= 0 && py >= 0 && px < LILI_LOG_W && py < LILI_LOG_H) r->px[py * LILI_LOG_W + px] = v;
            }
        }
        p += lit ? run : 1;
    }
}
