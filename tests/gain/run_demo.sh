#!/usr/bin/env bash
# End-to-end demo: build assets, compile the reader, and verify Replay Gain parsing.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/output"
GAIN="$ROOT/tests/gain"
PAUSE=2

mkdir -p "$OUT"
LOG="$OUT/demo.log"
: > "$LOG"

section() {
    echo ""
    echo "============================================================"
    echo "$1"
    echo "============================================================"
    sleep "$PAUSE"
}

pause_after() {
    sleep "$PAUSE"
}

note() {
    echo "$1"
    sleep "$PAUSE"
}

section "1/5  Generate four MP3 fixtures with ID3v2.3 TXXX tags"
note "fixtures cover: track gain, album gain+peak, mixed case/colon/equals/bad value, and no tag"
python3 "$GAIN/make_assets.py" "$GAIN" | tee -a "$LOG"
pause_after

section "2/5  Compile dr_mp3 (C89 and C99) and the 60-line reader"
note "warning-free compilation proves the new parser does not break existing build flags"
gcc -std=c89 -Wall -Wextra -Wpedantic -c "$ROOT/tests/compilation/dr_mp3.c" -o "$OUT/dr_mp3_c89.o"
gcc -std=c99 -Wall -Wextra -Wpedantic -c "$ROOT/tests/compilation/dr_mp3.c" -o "$OUT/dr_mp3_c99.o"
gcc -std=c99 -Wall -Wextra -Wpedantic -I"$ROOT" "$GAIN/read_gain.c" -lm -o "$OUT/read_gain"
gcc -std=c99 -Wall -Wextra -Wpedantic -I"$ROOT" "$GAIN/edge_check.c" -lm -o "$OUT/edge_check"
gcc -std=c99 -Wall -Wextra -Wpedantic -I"$ROOT" "$GAIN/decode_check.c" -lm -o "$OUT/decode_check"
echo "compilation clean (no warnings)" | tee -a "$LOG"
pause_after

section "3/5  Read back the four Replay Gain fields per file"
note "each file prints track/album gain and peak; missing fields render as (empty)"
"$OUT/read_gain" \
    "$GAIN/track_gain.mp3" \
    "$GAIN/album_gain_peak.mp3" \
    "$GAIN/mixed_case.mp3" \
    "$GAIN/no_tag.mp3" | tee "$OUT/gain_values.txt"
pause_after

section "4/5  Edge cases: null input, truncated tag, UTF-16 TXXX"
note "parser must never crash and must simply return an empty result on unusable input"
"$OUT/edge_check" | tee -a "$LOG"
pause_after

section "5/5  Verify decode is unaffected and values match the fixtures"
note "grepping the captured output against every value written into the fixtures"
fail=0
grep -q "track gain  = -3.45"          "$OUT/gain_values.txt" || fail=1
grep -q "album gain  = 1.2"            "$OUT/gain_values.txt" || fail=1
grep -q "album peak  = 0.876543"       "$OUT/gain_values.txt" || fail=1
grep -q "track gain  = -7.5"           "$OUT/gain_values.txt" || fail=1
grep -q "track peak  = 0.555"          "$OUT/gain_values.txt" || fail=1
grep -q "album gain  = 2.25"           "$OUT/gain_values.txt" || fail=1
grep -q "album peak  = 0.998001"       "$OUT/gain_values.txt" || fail=1
grep -q "no_tag.mp3 (id3v2 tag: no)"   "$OUT/gain_values.txt" || fail=1

"$OUT/decode_check" "$GAIN"/*.mp3 | tee -a "$LOG"

echo ""
if [ "$fail" -eq 0 ]; then
    echo "RESULT: all written values match, bad fields skipped, untagged file empty, decode intact."
    sleep 8
else
    echo "RESULT: verification FAILED"
    exit 1
fi
