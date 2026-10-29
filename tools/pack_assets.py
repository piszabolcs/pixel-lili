#!/usr/bin/env python3
"""Pixel Lili: B9, the packer (spec 7.5, 7.6). assets/sprites + the palette -> assets/build/assets.pak,
and the ids the C code uses -> common/asset_ids.h.

    py -3.12 tools/pack_assets.py

LPAK v1 (little endian), read by common/lili/pak.c:
  header (20 B): "LPAK", u16 version = 1, u16 n_entries, u32 toc_offset, u32 total_size, u32 crc32 (of bytes 20..total_size)
  TOC entry (12 B): u16 id, u8 type, u8 pad, u32 offset, u32 size            types: 1 SPRITE_SET, 2 PALETTE, 3 LUT, 4 MAP, 5 SOUND, 6 FONT
  PALETTE: 256 x RGB888
  SPRITE_SET: u16 anim_id, u8 n_dirs, u8 n_frames, u8 cell_w, u8 cell_h, u8 pivot_x, u8 pivot_y, u8 fps, u8 loop (0 once, 1 round, 2 there and back round, 3 there and back once), u8 dir_mask (bit d = direction d present), u8 pad;
              then n_dirs x n_frames frame records (12 B): u32 offset (from the set's start), u16 rle_len, u8 mouth_x, mouth_y, head_x, head_y, head_w, head_h;
              then the RLE data. RLE: a control byte c: c < 128 -> the next c + 1 bytes are literal; c >= 128 -> the next byte repeats c - 126 times (2..129).
  IMAGE (type 7, 2026-10-23: the backgrounds, tools/bg_build.py): u16 w, u16 h, i16 x, i16 y (where it sits on the 480 canvas),
              i16 sort_y (the Y-sort key of a prop, -1 = none), u8 flags (1 = a mask: the bytes are 0/1), u8 pad; then the RLE of w * h bytes.
  PALETTE ids: 1 the master (fixed 0-15, Lili 144-183), then one per background (its 200 colours in 16-143 and 184-255).
  LUT (type 3): 256 bytes, index -> index; id = palette id * 8 + table (LUT_SUN_STRONG ... LUT_SHADE_IN): the light and shadow remaps.
Index 0 is transparent (render8: r8_blit skips it).
"""
import json, os, struct, zlib
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T_SPRITE_SET, T_PALETTE, T_LUT, T_SOUND, T_IMAGE = 1, 2, 3, 5, 7
YARD_OFFSET = 100                                  # the yard's smaller sprite sets: anim id + 100 (tools/pixellab_to_sprites.py)
PALS = ["master", "room", "yard_spring", "yard_summer", "yard_autumn", "yard_winter"]          # PAL_ID = position + 1
LUTS = ["sun_strong", "sun_weak", "cast_day", "cast_eve", "shade_out", "shade_in"]             # tools/bg_build.py TABLES
IMAGES = {"room": 1, "yard_spring": 2, "yard_summer": 3, "yard_autumn": 4, "yard_winter": 5,
          "door_spring": 6, "door_summer": 7, "door_autumn": 8, "door_winter": 9,
          "pad": 10, "vase": 11, "bowl_food": 12, "bowl_water": 13, "bench": 14,
          "mask_sun": 20, "mask_cast": 21, "mask_shade_out": 22, "mask_shade_in": 23, "mask_cast_far": 24}
COVERS_PER_SEASON = 8                                   # the yard's covers: id IMG_COVER_BASE + season * 8 + k (tools/bg_build.py COVERS)
for _s, _season in enumerate(["spring", "summer", "autumn", "winter"]):
    for _k in range(COVERS_PER_SEASON):
        IMAGES[f"cover_{_season}_{_k}"] = 40 + _s * COVERS_PER_SEASON + _k

def backgrounds():
    """the IMAGE, PALETTE and LUT entries from tools/bg_build.py (none if it has not run)"""
    d = os.path.join(ROOT, "assets", "backgrounds", "game")
    if not os.path.exists(os.path.join(d, "game.json")):
        print("pack: no backgrounds yet (tools/bg_build.py), placeholder rooms"); return [], 0
    meta = json.load(open(os.path.join(d, "game.json"), encoding="utf-8"))
    out, raw = [], 0
    for name, pal in meta["palettes"].items():
        pid = PALS.index(name) + 1
        out.append((pid, T_PALETTE, bytes(sum(pal, []))))
        for t, table in meta["luts"][name].items():
            out.append((pid * 8 + LUTS.index(t), T_LUT, bytes(table)))
    for im in meta["images"]:
        a = np.asarray(Image.open(os.path.join(d, im["file"])))
        if im.get("mask"): a = (a > 0).astype(np.uint8)
        h, w = a.shape
        data = rle(a.flatten())
        assert unrle(data, a.size) == bytes(a.flatten()), "RLE round trip"
        head = struct.pack("<HHhhhBB", w, h, im["x"], im["y"], im.get("sort_y", -1), 1 if im.get("mask") else 0, 0)
        out.append((IMAGES[im["name"]], T_IMAGE, head + data)); raw += a.size
    return out, raw

def rle(row_major):
    out = bytearray()
    data = bytes(row_major)
    i, n = 0, len(data)
    while i < n:
        j = i
        while j + 1 < n and data[j + 1] == data[i] and j - i < 127:
            j += 1
        run = j - i + 1
        if run >= 2:
            out += bytes([126 + run, data[i]]); i += run; continue
        j = i
        while j < n and j - i < 127 and not (j + 2 < n and data[j] == data[j + 1] == data[j + 2]):
            j += 1
        out += bytes([j - i - 1]) + data[i:j]; i = j
    return bytes(out)

def unrle(b, n):
    out = bytearray(); i = 0
    while len(out) < n:
        c = b[i]; i += 1
        if c < 128:
            out += b[i:i + c + 1]; i += c + 1
        else:
            out += bytes([b[i]]) * (c - 126); i += 1
    return bytes(out)

def sprite_set(folder, man):
    frames_idx = []
    for fr in man["files"]:
        im = Image.open(os.path.join(folder, fr["file"]))
        assert im.mode == "P", fr["file"]
        frames_idx.append((fr, np.asarray(im, dtype=np.uint8)))
    dirs = man["dirs"]; nd, nf = len(dirs), man["frames"]
    cw, ch = man.get("cell_w", man.get("cell")), man.get("cell_h", man.get("cell"))       # cropped sets since 2026-10-27, square before
    loop = {"pingpong": 2, "there_and_back": 3}.get(man.get("play"), 1 if man["loop"] else 0)   # 0 once, 1 round, 2 there and back round, 3 there and back once
    head = struct.pack("<HBBBBBBBBBB", man["id"], nd, nf, cw, ch, man["pivot"][0], man["pivot"][1], man["fps"], loop,
                       sum(1 << d for d in dirs), 0)
    recs, blob = [], bytearray()
    base = len(head) + 12 * nd * nf
    for fr, idx in frames_idx:
        data = rle(idx.flatten())
        assert unrle(data, idx.size) == bytes(idx.flatten()), "RLE round trip"
        m = fr.get("mouth") or [0, 0]; h = fr.get("head") or [0, 0, 0, 0]
        recs.append(struct.pack("<IHBBBBBB", base + len(blob), len(data), m[0], m[1], h[0], h[1], h[2], h[3]))
        blob += data
    return head + b"".join(recs) + bytes(blob), sum(idx.size for _, idx in frames_idx)

def main():
    cat = {k: v for k, v in json.load(open(os.path.join(ROOT, "assets", "anims.json"), encoding="utf-8")).items() if not k.startswith("_")}
    pal = [list(c) for c in json.load(open(os.path.join(ROOT, "assets", "palettes", "master.json"), encoding="utf-8"))["rgb"]]
    for i, c in enumerate(json.load(open(os.path.join(ROOT, "assets", "palettes", "lili_sprites.json"), encoding="utf-8"))["rgb"]):
        pal[144 + i] = list(c)                      # Lili's 40 colours (tools/sprite_clean.py); master.json leaves these slots empty
    for i, c in enumerate(json.load(open(os.path.join(ROOT, "assets", "palettes", "toy_colors.json"), encoding="utf-8"))["rgb"]):
        pal[184 + i] = list(c)                      # the toys' 6 (tools/toy_sprites.py)
    entries = [(1, T_PALETTE, bytes(sum(pal, [])))]
    bg, bg_raw = backgrounds()
    entries += bg
    raw_total = 0
    for sub, off in (("lili", 0), ("lili_yard", YARD_OFFSET)):     # the room's sets, the yard's smaller ones (id + 100)
        sprites = os.path.join(ROOT, "assets", "sprites", sub)
        for name, spec in cat.items():
            if spec.get("reverse_of"): continue                        # another set backwards (ANIM_REVERSE_OF): no frames of its own
            folder = os.path.join(sprites, name)
            if not os.path.exists(os.path.join(folder, "manifest.json")):
                print(f"pack: {sub}/{name}: no sprites yet, skipped"); continue
            man = json.load(open(os.path.join(folder, "manifest.json"), encoding="utf-8"))
            man["id"] = spec["id"] + off
            if spec.get("play"): man["play"] = spec["play"]                 # there and back (tools/pixellab_to_sprites.py PLAY)
            payload, raw = sprite_set(folder, man)
            raw_total += raw
            entries.append((man["id"], T_SPRITE_SET, payload))
            print(f"pack: {sub}/{name} (id {man['id']}): {len(man['files'])} frames, {raw} B raw -> {len(payload)} B")
    sfx = []                                        # Lili's sound bank (tools/lili_sounds.py, 2026-10-29): one entry, its cues -> SFX_*
    bank = os.path.join(ROOT, "assets", "sounds", "lili")
    if os.path.exists(os.path.join(bank, "bank.bin")):
        entries.append((1, T_SOUND, open(os.path.join(bank, "bank.bin"), "rb").read()))
        sfx = [c["cue"] for c in json.load(open(os.path.join(bank, "bank.json"), encoding="utf-8"))["cues"]]
        print(f"pack: sounds: {len(sfx)} cues, {len(entries[-1][2])} B")
    body = bytearray(); toc = bytearray(); off = 20
    for eid, et, pl in entries:
        toc += struct.pack("<HBBII", eid, et, 0, off, len(pl)); off += len(pl); body += pl
    toc_off = off; total = off + len(toc)
    rest = bytes(body) + bytes(toc)
    hdr = struct.pack("<4sHHIII", b"LPAK", 1, len(entries), toc_off, total, zlib.crc32(rest) & 0xFFFFFFFF)
    out = os.path.join(ROOT, "assets", "build"); os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "assets.pak"), "wb").write(hdr + rest)
    ids = ["/* GENERATED by tools/pack_assets.py from assets/anims.json - do not edit */", "#ifndef LILI_ASSET_IDS_H", "#define LILI_ASSET_IDS_H",
           "enum { PAK_T_SPRITE_SET = 1, PAK_T_PALETTE = 2, PAK_T_LUT = 3, PAK_T_MAP = 4, PAK_T_SOUND = 5, PAK_T_FONT = 6, PAK_T_IMAGE = 7 };",
           "enum {"] + [f"    PAL_ID_{n.upper()} = {i + 1}," for i, n in enumerate(PALS)] + ["};",
           "enum {"] + [f"    LUT_{n.upper()} = {i}," for i, n in enumerate(LUTS)] + ["    LUT_COUNT", "};",
           "#define LUT_ID(pal, t) ((uint16_t)((pal) * 8 + (t)))",
           f"#define ANIM_YARD_OFFSET {YARD_OFFSET}   /* the yard's smaller sprite set of an animation: its id + this */",
           "enum {"] + [f"    IMG_{n.upper()} = {i}," for n, i in IMAGES.items() if not n.startswith("cover_")] + ["};",
           f"#define IMG_COVER(season, k) ((uint16_t)(40 + (season) * {COVERS_PER_SEASON} + (k)))   /* the yard's covers, k < {COVERS_PER_SEASON} */",
           f"#define IMG_COVERS_PER_SEASON {COVERS_PER_SEASON}",
           "enum {"] + [f"    ANIM_{n.upper()} = {s['id']}," for n, s in cat.items()] + ["    ANIM_COUNT_LISTED", "};",
           "/* an animation drawn from another one's frames, backwards (stand_up = sit_down reversed); 0 = it has its own */",
           "#define ANIM_REVERSE_OF(a) (" + "".join(f"(a) == ANIM_{n.upper()} ? ANIM_{s['reverse_of'].upper()} : " for n, s in cat.items() if s.get("reverse_of")) + "0)",
           "/* Lili's sounds: the cues of the bank (PAK_T_SOUND 1, tools/lili_sounds.py), in its order */",
           "enum {"] + [f"    SFX_{n.upper()}," for n in sfx] + ["    SFX_COUNT", "};",
           "#endif", ""]
    open(os.path.join(ROOT, "common", "asset_ids.h"), "w", encoding="utf-8", newline="\n").write("\n".join(ids))
    print(f"pack: assets.pak {total} B ({len(entries)} entries, sprites {raw_total} B raw, backgrounds {bg_raw} B raw), common/asset_ids.h")

if __name__ == "__main__":
    main()
