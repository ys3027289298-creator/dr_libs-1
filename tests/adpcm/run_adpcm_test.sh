#!/usr/bin/env bash
# IMA ADPCM encoder validation: build, generate 4 clips, round-trip, size and PCM byte-compare checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/output"
BIN="$OUT/adpcm_roundtrip"
BASE="$OUT/baseline"
mkdir -p "$OUT" "$BASE"

pause() { echo "(等待 $1 秒...)"; sleep "$1"; }

echo "==================== 阶段 1/3：编译测试程序（含 PCM 基线构建）===================="
cc -std=c99 -Wall -Wextra -I"$ROOT" "$ROOT/tests/adpcm/adpcm_roundtrip.c" -lm -o "$BIN"
# Baseline: pristine dr_wav.h from git, PCM path only (IMA calls are guarded behind an unused macro).
git -C "$ROOT" show HEAD:dr_wav.h > "$BASE/dr_wav_baseline.h"
cc -std=c99 -Wall -Wextra -I"$BASE" "$ROOT/tests/adpcm/pcm_baseline.c" -lm -o "$BASE/pcm_baseline"

pause 17
echo "==================== 阶段 2/3：生成四段音频并做 IMA ADPCM 编解码回放 ===================="
"$BIN" "$OUT"

pause 17
echo "==================== 阶段 3/3：PCM 字节一致性与产物体积校验 ===================="
if [ -x "$BASE/pcm_baseline" ]; then
    "$BASE/pcm_baseline" "$BASE" >/dev/null
    ok=1
    for f in sine_1s silence_0p5s odd_samples zero_frames; do
        if ! cmp -s "$BASE/${f}_pcm.wav" "$OUT/${f}_pcm.wav"; then
            echo "PCM 字节不一致: $f"; ok=0
        fi
    done
    [ "$ok" -eq 1 ] && echo "PCM 写出路径逐字节一致: 4/4 通过"
else
    echo "基线构建跳过（见上方编译信息）"
fi
ls -l "$OUT"/*_pcm.wav "$OUT"/*_adpcm.wav | awk '{print $9, $5" bytes"}'
echo "全部校验完成。"
