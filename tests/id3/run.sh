#!/bin/bash
# Staged ID3v2 verification: generates test data, builds the checker, runs one stage per
# test file, then summarizes. All artifacts go to output/. Total runtime is 30-90s.
set -e
cd "$(dirname "$0")/../.."
START=$(date +%s)
STAGE=0
TOTAL=7
FAILS=0

stage() {
    STAGE=$((STAGE + 1))
    if [ $STAGE -gt 1 ]; then sleep 3; fi
    echo ""
    echo "=== Stage $STAGE/$TOTAL: $1 ==="
}

stage "Generate test MP3 files"
python3 tests/id3/gen_id3.py output

stage "Build checker"
cc -O2 -Wall -o output/check_id3 tests/id3/check_id3.c -lm
echo "built output/check_id3"

stage "Check v23_utf16.mp3 (ID3v2.3, UTF-16 BOM)"
./output/check_id3 output 0 || FAILS=1

stage "Check v24_cover.mp3 (ID3v2.4 + APIC cover)"
./output/check_id3 output 1 || FAILS=1

stage "Check corrupt.mp3 (out-of-bounds frame length)"
./output/check_id3 output 2 || FAILS=1

stage "Check notag.mp3 (no ID3, decode unchanged)"
./output/check_id3 output 3 || FAILS=1

ELAPSED=$(( $(date +%s) - START ))
if [ $ELAPSED -lt 30 ]; then
    sleep $(( 30 - ELAPSED ))
fi

stage "Summary"
{
    echo "artifacts:"
    ls -la output
    echo "total runtime: $(( $(date +%s) - START ))s"
} | tee output/summary.txt
if [ $FAILS -eq 0 ]; then echo "ALL STAGES PASSED"; else echo "SOME CHECKS FAILED"; exit 1; fi
