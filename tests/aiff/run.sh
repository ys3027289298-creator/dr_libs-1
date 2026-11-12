#!/bin/bash
set -e
cd "$(dirname "$0")/../.."
ROOT=$PWD
mkdir -p output
stage() { echo; echo "===== 阶段 $1: $2 ====="; sleep 3; }

stage 1 "导出改动前 dr_wav.h 作为 WAV 回归基准"
git show HEAD:dr_wav.h > output/dr_wav_orig.h

stage 2 "编译 AIFF 生成器"
cc -O1 -Wall tests/aiff/gen_aiff.c -o output/gen_aiff -lm

stage 3 "生成 单声道16位 AIFF"
./output/gen_aiff 0

stage 4 "生成 立体声24位 AIFF"
./output/gen_aiff 1

stage 5 "生成 立体声32位浮点 AIFF"
./output/gen_aiff 2

stage 6 "生成 0帧空 AIFF"
./output/gen_aiff 3

stage 7 "Python aifc 读回校验"
python3 tests/aiff/verify_aiff.py

stage 8 "编译 WAV 回归写入器（原版与新版头文件）"
cc -O1 -DDRWAV_H="\"$ROOT/output/dr_wav_orig.h\"" tests/aiff/wav_gen.c -o output/wav_gen_old
cc -O1 -DDRWAV_H="\"$ROOT/dr_wav.h\""          tests/aiff/wav_gen.c -o output/wav_gen_new

stage 9 "WAV 输出逐字节对比"
./output/wav_gen_old output/reg_old.wav
./output/wav_gen_new output/reg_new.wav
cmp output/reg_old.wav output/reg_new.wav && echo "WAV 输出逐字节一致"

stage 10 "汇总"
ls -l output/ && sleep 2 && echo "全部阶段完成"
