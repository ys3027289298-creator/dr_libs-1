#!/usr/bin/env bash
# End-to-end gapless dr_mp3 demo. Runs five stages, never blocks on input.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/output"
SRC="$OUT/src/gapless_demo.c"
BIN="$OUT/bin/gapless_demo"
LOG="$OUT/log/demo_run.log"
mkdir -p "$OUT/bin" "$OUT/pcm" "$OUT/log" "$OUT/media"

CC="${CC:-gcc}"
CFLAGS=(-std=c99 -Wall -Wextra -O2 -I"$ROOT")

run() {
    echo "\$ $*"
    "$@"
    local code=$?
    echo "[exit code: $code]"
    return $code
}

{
echo "################ STAGE 1: ENVIRONMENT CONFIRMATION ################"
echo "## working directory"
run pwd
echo
echo "## directory listing (repo root)"
run ls -1 "$ROOT"
echo
echo "## compiler"
run "$CC" --version
echo
echo "## dr_mp3 header"
HEADER="$ROOT/dr_mp3.h"
run ls -l "$HEADER"
echo "header resolved path: $HEADER"
grep -n "dr_mp3 - v" "$HEADER" | head -1
echo
echo "## ffmpeg (test vector generator)"
echo "\$ ffmpeg -hide_banner -version | head -1"
ffmpeg -hide_banner -version | head -1
echo "[exit code: ${PIPESTATUS[0]}]"

echo
echo "################ STAGE 2: BUILD DEMO PROGRAM ################"
run "$CC" "${CFLAGS[@]}" "$SRC" -lm -o "$BIN"
BUILD_CODE=$?
if [ "$BUILD_CODE" -ne 0 ]; then
    echo "Build failed, aborting."
    exit "$BUILD_CODE"
fi
run ls -l "$BIN"

echo
echo "################ STAGES 3-5: DECODE / WAVEFORM / REGRESSION ################"
run "$BIN" "$OUT/media" "$OUT/pcm"

echo
echo "################ ARTIFACTS ################"
run find "$OUT" -type f | sort
} 2>&1 | tee "$LOG"

echo
echo "Log saved to $LOG"
