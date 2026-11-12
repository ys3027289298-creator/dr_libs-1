#!/usr/bin/env bash
# Builds the VBRI check program and compares its output against ffprobe.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$ROOT/output"
FILES=(vbri_stereo.mp3 vbri_mono.mp3 vbri_corrupt.mp3 no_vbr_header.mp3)

mkdir -p "$OUT"
cd "$HERE"

if [ ! -f "$HERE/vbri_stereo.mp3" ]; then
    python3 make_data.py
fi

gcc -std=c89 -Wall -Wextra -I"$ROOT" vbri_check.c -o "$OUT/vbri_check"

{
    echo "== dr_mp3 (tests/vbri/vbri_check.c) =="
    "$OUT/vbri_check" "${FILES[@]}"
    echo
    echo "== ffprobe reference =="
    for f in "${FILES[@]}"; do
        dur=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$f")
        br=$(ffprobe -v error -show_entries format=bit_rate -of csv=p=0 "$f")
        echo "$f duration=$dur avg_bitrate=$br"
    done
} | tee "$OUT/vbri_results.txt"

echo
echo "Saved to output/vbri_results.txt"
