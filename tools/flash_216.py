#!/usr/bin/env python3
"""Pixel Lili: flash the 2.16 panel from Windows with the Windows-side esptool.

    py -3.12 tools/flash_216.py diag [--port COM3]     the diag firmware (firmware/build/diag)
    py -3.12 tools/flash_216.py lili [--port COM3]     the Pixel Lili world (firmware/build/lili) and its pack
    py -3.12 tools/flash_216.py pak [--port COM3]      only the pack (assets/build/assets.pak): after a sprite change
    py -3.12 tools/flash_216.py tank [--port COM3]     the pocket-tank square set back (firmware/build/216)
    py -3.12 tools/flash_216.py <set> --dry            print the esptool command only

Each set is the build's own flash_args (ESP-IDF writes it next to the binaries:
the flash mode / frequency / size on the first line, then "offset file" rows),
so what idf.py would flash is what this flashes. The model partition
(0x290000) is in neither set: a diag <-> tank swap never touches it. The world's
pack goes to its own assets partition (0xA90000, firmware/lili/partitions.csv,
spec 5.10; since 2026-10-27, before that it was linked into the app). Close any
serial reader on the port first (serial_log.py), the bootloader needs it.
"""
import argparse, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SETS = {"diag": os.path.join(ROOT, "firmware", "build", "diag"),
        "lili": os.path.join(ROOT, "firmware", "build", "lili"),
        "tank": os.path.join(ROOT, "firmware", "build", "216"),
        "pak": os.path.join(ROOT, "firmware", "build", "lili"),
        "model": os.path.join(ROOT, "firmware", "build", "lili")}
PAK_OFFSET, PAK_SIZE = 0xA90000, 0x570000          # firmware/lili/partitions.csv: assets
MODEL_OFFSET, MODEL_SIZE = 0x290000, 0x800000      # ... and model (Lili's brain, model/export_lili_q4.py)
MODEL = os.path.join(ROOT, "model", "out", "lili", "lili_S_q4.bin")
PAK = os.path.join(ROOT, "assets", "build", "assets.pak")

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("set", choices=sorted(SETS))
    ap.add_argument("--port", default="COM3")
    ap.add_argument("--baud", default="921600")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    bdir = SETS[a.set]
    fa = os.path.join(bdir, "flash_args")
    if not os.path.exists(fa):
        sys.exit(f"no {fa}: build first (tools/build_diag.sh in WSL copies the artefacts here)")
    lines = [l.strip() for l in open(fa, encoding="utf-8") if l.strip()]
    opts = lines[0].split()
    pairs = []
    for l in (lines[1:] if a.set not in ("pak", "model") else []):
        off, rel = l.split()
        path = os.path.join(bdir, rel.replace("/", os.sep))
        if not os.path.exists(path):
            sys.exit(f"missing {path}")
        pairs += [off, path]
    if a.set in ("lili", "pak"):
        if not os.path.exists(PAK): sys.exit(f"no {PAK}: py -3.12 tools/pack_assets.py")
        if os.path.getsize(PAK) > PAK_SIZE: sys.exit(f"{PAK} is {os.path.getsize(PAK)} B, the assets partition holds {PAK_SIZE} B")
        pairs += [hex(PAK_OFFSET), PAK]
    if a.set == "model" or (a.set == "lili" and os.path.exists(MODEL)):        # Lili's brain model, when there is one
        if not os.path.exists(MODEL): sys.exit(f"no {MODEL}: model/train_lili.py, model/export_lili_q4.py")
        if os.path.getsize(MODEL) > MODEL_SIZE: sys.exit(f"{MODEL} is {os.path.getsize(MODEL)} B, the model partition holds {MODEL_SIZE} B")
        pairs += [hex(MODEL_OFFSET), MODEL]
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "-p", a.port, "-b", a.baud,
           "--before", "default_reset", "--after", "hard_reset", "write_flash"] + opts + pairs
    print(" ".join(cmd))
    if a.dry:
        return
    for off, path in zip(pairs[0::2], pairs[1::2]):
        print(f"  {off:>10}  {os.path.getsize(path):>9} B  {os.path.relpath(path, ROOT)}")
    sys.exit(subprocess.call(cmd))

if __name__ == "__main__":
    main()
