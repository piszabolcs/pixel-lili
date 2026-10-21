#!/usr/bin/env bash
# Pixel Lili: build the world firmware (M2.6) in WSL and copy the flashable set to
# firmware/build/lili, where tools/flash_216.py (Windows side) picks it up.
#   bash tools/build_lili.sh            (from the repo root, in WSL)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="${LILI_BUILD:-$HOME/build/lili-world}"
OUT="$ROOT/firmware/build/lili"
. "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1
cd "$ROOT/firmware/lili"
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" build
mkdir -p "$OUT/bootloader" "$OUT/partition_table"
cp "$B/lili_world.bin" "$B/flash_args" "$OUT/"
cp "$B/bootloader/bootloader.bin" "$OUT/bootloader/"
cp "$B/partition_table/partition-table.bin" "$OUT/partition_table/"
echo "lili set -> $OUT"
