#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/output"
DATA="$OUT/data"
BUILD="$OUT/build"
BUILD="$OUT/build/run"
mkdir -p "$BUILD"
CFLAGS="-std=c99 -Wall -Wextra -I$ROOT"

echo "== 阶段 1/4: 编译新版 example =="
gcc $CFLAGS -DDRWAV_HAS_WRITE_F32 "$OUT/example_float_wav.c" -lm -o "$BUILD/example_float"
sleep 2

echo "== 阶段 2/4: 四个用例写出并读回 =="
run_case() {
    local name="$1" mode="$2"
    local extra=()
    [ "$mode" = int ] && extra=(int16)
    printf '%-14s ' "$name:"
    "$BUILD/example_float" "$DATA/$name.txt" "$OUT/$name.wav" "${extra[@]}"
}
run_case mono float
run_case stereo float
run_case outofrange float
run_case integer_path int
sleep 2

echo "== 阶段 3/4: pristine 头文件构建整数路径对照 =="
git -C "$ROOT" show HEAD:dr_wav.h > "$BUILD/dr_wav.h"
gcc -std=c99 -Wall -Wextra -I"$BUILD" -I"$ROOT" "$OUT/example_float_wav.c" -lm -o "$BUILD/example_ref"
"$BUILD/example_ref" "$DATA/integer_path.txt" "$OUT/integer_path_ref.wav" int16
sleep 2

echo "== 阶段 4/4: 逐字节比对 =="
if cmp -s "$OUT/integer_path.wav" "$OUT/integer_path_ref.wav"; then
    echo "integer path: byte-identical to pristine output"
else
    echo "integer path: MISMATCH"; exit 1
fi
ls -l "$OUT"/*.wav
echo "ALL CHECKS PASSED"
