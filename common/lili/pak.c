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

void pak_blit_dither(render8_t *r, const sprite_set_t *s, const sprite_frame_t *f, int x, int y, int level) {
    static const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    if (level >= 16) { pak_blit(r, s, f, x, y); return; }
    if (level <= 0) return;
    const int W = s->cell_w, H = s->cell_h, ox = x - s->pivot_x, oy = y - s->pivot_y;
    const uint8_t *p = f->rle, *end = f->rle + f->rle_len;
    int i = 0, n = W * H;
    while (i < n && p < end) {
        int c = *p++, run, lit;
        if (c < 128) { run = c + 1; lit = 1; } else { run = c - 126; lit = 0; }
        for (int k = 0; k < run && i < n; k++, i++) {
            uint8_t v = lit ? p[k] : p[0];
            if (!v) continue;
            int px = ox + i % W, py = oy + i / W;
            if (px < 0 || py < 0 || px >= LILI_LOG_W || py >= LILI_LOG_H || BAYER[py & 3][px & 3] >= level) continue;
            r->px[py * LILI_LOG_W + px] = v;
        }
        p += lit ? run : 1;
    }
}

void pak_frame_decode(const sprite_set_t *s, const sprite_frame_t *f, uint8_t *out) {
    const uint8_t *p = f->rle, *end = f->rle + f->rle_len;
    int i = 0, n = s->cell_w * s->cell_h;
    while (i < n && p < end) {
        int c = *p++;
        if (c < 128) { int run = c + 1; for (int k = 0; k < run && i < n; k++) out[i++] = p[k]; p += run; }
        else { int run = c - 126; for (int k = 0; k < run && i < n; k++) out[i++] = *p; p++; }
    }
    while (i < n) out[i++] = 0;
}

bool pak_palette_id(const pak_t *p, uint16_t id, uint8_t out[256][3]) {
    uint32_t sz; const uint8_t *d = pak_find(p, PAK_T_PALETTE, id, &sz);
    if (!d || sz != 768) return false;
    memcpy(out, d, 768); return true;
}
bool pak_palette(const pak_t *p, uint8_t out[256][3]) { return pak_palette_id(p, PAL_ID_MASTER, out); }

const uint8_t *pak_lut(const pak_t *p, uint16_t id) {
    uint32_t sz; const uint8_t *d = pak_find(p, PAK_T_LUT, id, &sz);
    return d && sz == 256 ? d : NULL;
}

bool pak_image(const pak_t *p, uint16_t id, pak_image_t *im) {
    uint32_t sz; const uint8_t *d = pak_find(p, PAK_T_IMAGE, id, &sz);
    if (!d || sz < 12) return false;
    im->w = rd16(d); im->h = rd16(d + 2); im->x = (int16_t)rd16(d + 4); im->y = (int16_t)rd16(d + 6);
    im->sort_y = (int16_t)rd16(d + 8); im->mask = d[10] & 1;
    im->rle = d + 12; im->rle_len = sz - 12;
    return im->w && im->h;
}

/* the RLE of an image, run by run, onto the canvas: a colour picture writes its non-zero bytes, a mask
   remaps the canvas where its byte is 1 (the same walk; one branch per run, not per pixel) */
static void image_walk(render8_t *r, const pak_image_t *im, const uint8_t *lut) {
    const uint8_t *p = im->rle, *end = im->rle + im->rle_len;
    const int W = im->w; int i = 0, n = im->w * im->h;
    while (i < n && p < end) {
        int c = *p++, run, lit;
        if (c < 128) { run = c + 1; lit = 1; } else { run = c - 126; lit = 0; }
        if (!lit && *p == 0) { i += run; p++; continue; }                  /* a transparent run: skip it whole */
        for (int k = 0; k < run && i < n; k++, i++) {
            uint8_t v = lit ? p[k] : p[0];
            if (!v) continue;
            int px = im->x + i % W, py = im->y + i / W;
            if (px < 0 || py < 0 || px >= LILI_LOG_W || py >= LILI_LOG_H) continue;
            uint8_t *dst = &r->px[py * LILI_LOG_W + px];
            *dst = lut ? lut[*dst] : v;
        }
        p += lit ? run : 1;
    }
}
void pak_image_blit(render8_t *r, const pak_image_t *im) { image_walk(r, im, NULL); }
void pak_image_remap(render8_t *r, const pak_image_t *m, const uint8_t lut[256]) { if (lut) image_walk(r, m, lut); }

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
