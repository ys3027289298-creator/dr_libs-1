#!/bin/bash
# Staged verification for the smpl loop-point write API. Results go to output/.
set -e
cd "$(dirname "$0")/../.."
OUT=output; BIN=tests/smpl/smpl_write_read; REPORT=$OUT/smpl_report.txt
mkdir -p "$OUT"
rm -f "$REPORT" tests/smpl/*.wav
stage() { echo; echo "===== $1 ====="; }
pause() { sleep 3; }

stage "1/12 环境检查"
${CC:-cc} --version | head -1
pause
stage "2/12 清理旧产物"
rm -f tests/smpl/*.wav && echo "clean ok"
pause
stage "3/12 编译测试程序"
${CC:-cc} -O0 -Wall -o "$BIN" tests/smpl/smpl_write_read.c -lm && echo "build ok"
pause
stage "4/12 用例1: 单循环"
"./$BIN" single tests/smpl/single_loop.wav | tee -a "$REPORT"
pause
stage "5/12 用例2: 双循环"
"./$BIN" dual tests/smpl/dual_loop.wav | tee -a "$REPORT"
pause
stage "6/12 用例3: 循环点数量越界(应拒绝)"
"./$BIN" overflow tests/smpl/overflow_loop.wav | tee -a "$REPORT"
pause
stage "7/12 用例4: 不带循环点"
"./$BIN" none tests/smpl/no_loop.wav | tee -a "$REPORT"
pause
stage "8/12 生成基准文件(旧接口)"
"./$BIN" baseline tests/smpl/baseline.wav | tee -a "$REPORT"
pause
stage "9/12 逐字节一致性比对(不带循环点 vs 旧接口)"
if cmp -s tests/smpl/no_loop.wav tests/smpl/baseline.wav; then
    echo "case=none byte_identical=yes" | tee -a "$REPORT"
else
    echo "case=none byte_identical=no" | tee -a "$REPORT"; exit 1
fi
pause
stage "10/12 校验读回结果"
if grep -q "match=no" "$REPORT" || ! grep -q "case=overflow write=rejected" "$REPORT"; then
    echo "RESULT: FAIL"; exit 1
fi
echo "readback ok"
pause
stage "11/12 写入汇总报告"
grep -c "match=yes" "$REPORT" | xargs echo "matched_cases:" >> "$REPORT"
echo "report written"
pause
stage "12/12 完成"
echo "RESULT: PASS"
echo "report: $REPORT"
