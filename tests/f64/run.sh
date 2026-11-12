#!/usr/bin/env bash
# Staged verification for 64-bit float WAV support in dr_wav.h.
# Non-interactive; all artifacts go to output/. Total runtime ~35s.
set -u
cd "$(dirname "$0")/../.."   # repo root
mkdir -p output/baseline
PASS=ok
phase() { printf '\n===== [%s/11] %s =====\n' "$1" "$2"; }
pause() { sleep 3; }

phase 1 "环境检查"
gcc --version | head -1; python3 --version
pause
phase 2 "生成 64 位浮点测试数据"
python3 tests/f64/gen_f64.py || PASS=fail
pause
phase 3 "检查生成的测试文件"
ls -l output/*.wav
od -A d -t x1 -N 48 output/f64_stereo.wav
pause
phase 4 "准备改动前的 dr_wav.h 基线"
git show HEAD:dr_wav.h > output/baseline/dr_wav.h && echo "baseline header saved"
pause
phase 5 "编译基线回归程序（原版 dr_wav.h）"
gcc -Wall -Wextra -O1 -Ioutput/baseline -I. tests/f64/regression.c -o output/regression_base -lm || PASS=fail
pause
phase 6 "运行基线回归（f32/s16/s32 既有路径）"
./output/regression_base > output/regression_base.txt && cat output/regression_base.txt || PASS=fail
pause
phase 7 "编译新版校验程序（改动后的 dr_wav.h）"
gcc -Wall -Wextra -O1 -I. tests/f64/regression.c -o output/regression_new -lm || PASS=fail
gcc -Wall -Wextra -O1 -I. tests/f64/read_f64.c -o output/read_f64 -lm || PASS=fail
gcc -Wall -Wextra -O1 -c tests/compilation/dr_wav.c -o output/dr_wav_compile_check.o || PASS=fail
pause
phase 8 "运行 64 位浮点读取校验"
./output/read_f64 | tee output/read_f64.txt
[ "${PIPESTATUS[0]}" -eq 0 ] || PASS=fail
pause
phase 9 "运行新版回归"
./output/regression_new > output/regression_new.txt || PASS=fail
pause
phase 10 "与基线对比，确认既有路径输出不变"
diff output/regression_base.txt output/regression_new.txt && echo "regression: identical" || PASS=fail
removed=$(diff <(git show HEAD:dr_wav.h) dr_wav.h | grep -c '^<' || true)
echo "dr_wav.h existing lines removed: $removed"
[ "$removed" -eq 0 ] || PASS=fail
pause
phase 11 "汇总"
ls output/
echo "overall: $PASS (${SECONDS}s elapsed)"
[ "$PASS" = ok ]
