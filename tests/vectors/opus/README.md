# Opus test vectors

Two small self-contained Ogg Opus test vectors for dr_opus. Both are 0.5 s at
48 kHz, encoded with libopus (via ffmpeg) with 20 ms Ogg pages so that
packet-loss/resync behavior can be exercised page by page.

| File                  | Layout        | Content                                            | Bitrate |
|-----------------------|---------------|----------------------------------------------------|---------|
| `sine_48k_stereo.opus` | stereo 48 kHz | 440 Hz sine (L) + 880 Hz sine at -12 dB (R)        | 96 kbps |
| `voice_48k_mono.opus`  | mono 48 kHz   | speech-like harmonic stack, 4 Hz vibrato, 3 Hz AM  | 48 kbps |

## Regenerating

Requires ffmpeg with libopus (`ffmpeg -encoders | grep libopus`):

```sh
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.5" \
       -f lavfi -i "sine=frequency=880:sample_rate=48000:duration=0.5" \
       -filter_complex "[0:a][1:a]amerge=inputs=2,pan=stereo|c0=c0|c1=0.25*c1[a]" \
       -map "[a]" -c:a libopus -b:a 96k -page_duration 20000 sine_48k_stereo.opus

ffmpeg -f lavfi -i "aevalsrc=exprs=0.35*(sin(2*PI*(120+8*sin(2*PI*4*t))*t)+0.5*sin(2*PI*2*(120+8*sin(2*PI*4*t))*t)+0.3*sin(2*PI*3*(120+8*sin(2*PI*4*t))*t)+0.15*sin(2*PI*5*(120+8*sin(2*PI*4*t))*t))*(0.6+0.4*sin(2*PI*3*t)):s=48000:d=0.5" \
       -c:a libopus -b:a 48k -ac 1 -page_duration 20000 voice_48k_mono.opus
```

Keep the mono signal below full scale: ffmpeg's libopus decoder outputs s16 and
clips above 0 dBFS, which would dominate any decoder comparison.

## Verifying

Build with `-DDR_LIBS_BUILD_TESTS=ON`, then:

```sh
# Decode with dr_opus.
./build/dr_libs_test_opus tests/vectors/opus/sine_48k_stereo.opus --s16 /tmp/dr.s16 --f32 /tmp/dr.f32

# Reference decodes (ffmpeg's libopus wrapper outputs s16; the native "opus" decoder outputs float).
ffmpeg -c:a libopus -i tests/vectors/opus/sine_48k_stereo.opus -f s16le /tmp/ref.s16
ffmpeg -c:a opus    -i tests/vectors/opus/sine_48k_stereo.opus -f f32le /tmp/ref.f32

# Per-sample comparison.
python3 -I tests/opus_compare.py /tmp/dr.s16 /tmp/ref.s16 s16
python3 -I tests/opus_compare.py /tmp/dr.f32 /tmp/ref.f32 f32

# Packet-loss path: corrupt one mid-stream page (CRC becomes invalid), decode must continue.
python3 -I tests/opus_corrupt.py tests/vectors/opus/sine_48k_stereo.opus /tmp/corrupted.opus 0.5 32
./build/dr_libs_test_opus /tmp/corrupted.opus --f32 /tmp/corrupted.f32
```
