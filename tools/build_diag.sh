#!/usr/bin/env bash
# Pixel Lili: build the diag firmware in WSL and copy the flashable set to
# firmware/build/diag, where tools/flash_216.py (Windows side) picks it up.
#   bash tools/build_diag.sh            (from the repo root, in WSL)
# The build tree lives in the WSL filesystem (~/build/lili-diag): ninja on
# /mnt/c is several times slower and the object files are throwaway anyway.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="${DIAG_BUILD:-$HOME/build/lili-diag}"
OUT="$ROOT/firmware/build/diag"
. "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1
cd "$ROOT/firmware/diag"
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" build
mkdir -p "$OUT/bootloader" "$OUT/partition_table"
cp "$B/lili_diag.bin" "$B/flash_args" "$OUT/"
cp "$B/bootloader/bootloader.bin" "$OUT/bootloader/"
cp "$B/partition_table/partition-table.bin" "$OUT/partition_table/"
echo "diag set -> $OUT"
