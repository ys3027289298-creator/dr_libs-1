# MPEG-1 Layer II tests

Frame-level verification of the Layer II decoder in `dr_mp3.h` against ffmpeg
reference waveforms.

## Vectors

| File | Format | Notes |
| --- | --- | --- |
| `stereo44100.mp2` | MPEG-1 Layer II, 44.1 kHz, stereo | 128 kbps simple stereo |
| `mono48000.mp2` | MPEG-1 Layer II, 48 kHz, mono | single channel |
| `joint_stereo.mp2` | MPEG-1 Layer II, 44.1 kHz, joint stereo | 32 kbps, intensity-stereo boundary active |
| `corrupt_frame.mp2` | MPEG-1 Layer II, 44.1 kHz, stereo | payload of one frame corrupted; decoder must skip and resync |

## Running

```
./tests/layer2/run_layer2.sh
```

The script builds `layer2_test.c`, renders f32le references with ffmpeg into
`output/ref/`, decodes every vector one frame at a time, prints file name,
sample rate, channels, frame count and the first 8 samples, then reports the
maximum absolute error versus the reference. Results are mirrored to
`output/layer2_log.txt`. A pass requires errors within float32 tolerance and,
for the corrupt vector, at least one skipped frame with successful resync.
