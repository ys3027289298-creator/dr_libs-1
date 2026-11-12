# dr_mp3 gapless 解码演示

本目录包含 dr_mp3 无间隙（gapless）解码补丁的可复现演示与全部产物。所有路径均为仓库相对路径，
脚本不等待任何键盘输入。

## 目录结构

- `../dr_mp3.h`            打补丁后的单文件库（含 LAME 解析、无 LAME 退化估算、查询 API）
- `src/gapless_demo.c`      演示程序（阶段 3/4/5 的实现）
- `bin/gapless_demo`        已构建的演示二进制
- `generate_media.sh`       用 ffmpeg/libmp3lame 生成三个测试 MP3
- `run_demo.sh`             一键执行五个阶段并把整屏输出写入 `log/demo_run.log`
- `media/`                  测试向量
  - `lame_cbr.mp3`          CBR，带 Info/LAME 标签（含编码延迟与末尾填充）
  - `lame_vbr.mp3`          VBR，带 Xing/LAME 标签
  - `nolame_cbr.mp3`        剥掉 Xing/Info/LAME 头帧的 CBR，触发退化估算
  - `source_tone.wav`       生成 MP3 用的干净音源（音频顶到文件首尾）
- `pcm/`                    每个 MP3 的两份解码结果：`*.raw.s16.pcm`（未裁剪）与
                            `*.trimmed.s16.pcm`（gapless 裁剪后），交错双声道 s16 LE
- `log/demo_run.log`        完整录屏文本

## 一键运行

```
./output/generate_media.sh   # 仅在需要重新生成测试向量时
./output/run_demo.sh
```

## 五个阶段

1. 环境确认：工作目录、目录列表、`gcc --version`、`dr_mp3.h` 绝对路径、ffmpeg 版本
2. 构建演示：打印完整编译命令与退出码
3. 逐文件解码：文件名、采样率、声道、原始 PCM 帧数、头部/尾部裁剪帧数、实际输出帧数；
   第四个用例在中点 seek 后继续解码并打印 seek 后首个样本序号
4. 波形校验：裁剪前后首尾各 512 个 PCM 帧的最大振幅、精确零静音跨度、裁剪总时长（毫秒）
5. 回归：两次完整解码的 FNV-1a 哈希（应一致）与中点 seek 后的首个样本序号

## 库侧新增/修改的公开接口

- `drmp3_get_gapless_info(drmp3*, drmp3_gapless_info*)`
  一次性查询：是否来自 LAME 标签、头部延迟、尾部填充、原始总帧数、裁剪后输出帧数
- `drmp3_get_cursor_pcm_frame(drmp3*)`
  下一次读取将返回的样本序号（裁剪后输出空间，从 0 开始）
- `drmp3_get_pcm_frame_count()` 语义明确为“裁剪后输出帧数”

## 退化策略（无 LAME 标签）

不使用任何硬编码编码器常量。初始化时若流可 seek，则把每一 MP3 帧解码一遍：
按 `drmp3_hdr_frame_samples()`（384/576/1152）累计原始 PCM 帧数，并实测解码 PCM
首尾的数字静音跨度作为裁剪点；随后回卷到流开头，逐帧解码与 seek 行为不变。
不可 seek 的流（如管道）跳过估算，保持零裁剪并正常解码到自然结尾。
