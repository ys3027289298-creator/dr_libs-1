#!/usr/bin/env bash
# MPEG-1 Layer II verification: build, ffmpeg references, decode, compare.
# All artifacts (binary, references, log) are written under output/.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
dir="$root/tests/layer2"
out="$root/output"
mkdir -p "$out/ref"

pause() { sleep 3; }
settle() { sleep 2; }

echo "=== Stage 1/4: build frame-level Layer II test ==="
cc -O2 -Wall -Wextra -std=c99 "$dir/layer2_test.c" -lm -o "$out/layer2_test"
echo "built $out/layer2_test"
pause

echo "=== Stage 2/4: generate reference waveforms (ffmpeg f32le) ==="
for name in stereo44100 mono48000 joint_stereo corrupt_frame; do
    ffmpeg -nostdin -y -i "$dir/$name.mp2" -f f32le "$out/ref/$name.f32" \
        -loglevel error
    echo "ref: $name.f32"
    settle
done
pause

echo "=== Stage 3/4: frame-by-frame decode and max-error comparison ==="
: > "$out/layer2_log.txt"
run() {
    echo "--- $1 ---" | tee -a "$out/layer2_log.txt"
    if [ "${3:-}" = "corrupt" ]; then
        "$out/layer2_test" "$dir/$1.mp2" "$out/ref/$1.f32" corrupt \
            2>&1 | tee -a "$out/layer2_log.txt"
    else
        "$out/layer2_test" "$dir/$1.mp2" "$out/ref/$1.f32" \
            2>&1 | tee -a "$out/layer2_log.txt"
    fi
}
run stereo44100
settle
settle
run mono48000
settle
settle
run joint_stereo
settle
run corrupt_frame corrupt
pause

echo "All stages finished; cooling down before exit."
settle
settle
settle

echo "=== Stage 4/4: verdict ==="
passes="$(grep -c 'result=PASS' "$out/layer2_log.txt" || true)"
echo "PASS count: $passes/4 (log: output/layer2_log.txt)"
[ "$passes" -eq 4 ]
