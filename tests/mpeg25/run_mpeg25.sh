#!/usr/bin/env bash
# MPEG-2.5 validation: per-frame decode of four vectors plus an MPEG-1 regression.
set -u
cd "$(dirname "$0")"
ROOT="../.."
OUT="$ROOT/output"
LOG="$OUT/mpeg25_report.txt"
mkdir -p "$OUT"
: > "$LOG"

section() {
    echo ""
    echo "==================== $1 ===================="
    echo ""
    sleep 6
}

section "1. Build current and baseline dr_mp3 harnesses" | tee -a "$LOG"
cc -O2 -Wall -Wextra -o "$OUT/mpeg25_test" mpeg25_test.c -lm 2>&1 | tee -a "$LOG"
git -C "$ROOT" show HEAD:dr_mp3.h > "$OUT/dr_mp3_baseline.h"
BASE_INCLUDE="$(cd "$OUT" && pwd)/dr_mp3_baseline.h"
sed "s#../../dr_mp3.h#$BASE_INCLUDE#" mpeg25_test.c > "$OUT/mpeg25_test_baseline.c"
cc -O2 -Wall -Wextra -o "$OUT/mpeg25_test_baseline" "$OUT/mpeg25_test_baseline.c" -lm 2>&1 | tee -a "$LOG"

run() {
    section "2.$1 $2" | tee -a "$LOG"
    if [ "${3:-}" = "bad" ]; then
        if "$OUT/mpeg25_test" "$2" --bad 2>&1 | tee -a "$LOG"; then
            echo "RESULT: rejected as expected" | tee -a "$LOG"
        else
            echo "RESULT: FAIL (bad file decoded)" | tee -a "$LOG"
        fi
    else
        if "$OUT/mpeg25_test" "$2" 2>&1 | tee -a "$LOG"; then
            echo "RESULT: ok" | tee -a "$LOG"
        else
            echo "RESULT: FAIL" | tee -a "$LOG"
        fi
    fi
}

run 1 mpeg25_8k_mono.mp3
run 2 mpeg25_8k_stereo.mp3
run 3 mpeg25_11k_mono.mp3
run 4 mpeg25_bad_samplerate.mp3 bad

section "3. MPEG-1 regression (sample-identical vs baseline)" | tee -a "$LOG"
"$OUT/mpeg25_test" regression_mpeg1_44k_mono.mp3 --dump "$OUT/mpeg1_current.raw" > "$OUT/mpeg1_current.txt" 2>&1
"$OUT/mpeg25_test_baseline" regression_mpeg1_44k_mono.mp3 --dump "$OUT/mpeg1_baseline.raw" > "$OUT/mpeg1_baseline.txt" 2>&1
cat "$OUT/mpeg1_current.txt" | tee -a "$LOG"
if cmp -s "$OUT/mpeg1_current.raw" "$OUT/mpeg1_baseline.raw" && cmp -s "$OUT/mpeg1_current.txt" "$OUT/mpeg1_baseline.txt"; then
    echo "MPEG-1 regression: PASS (all samples identical vs git HEAD)" | tee -a "$LOG"
else
    echo "MPEG-1 regression: FAIL (output changed)" | tee -a "$LOG"
    diff -u "$OUT/mpeg1_baseline.txt" "$OUT/mpeg1_current.txt" | tee -a "$LOG"
fi

echo "" | tee -a "$LOG"
echo "Report written to $LOG"
