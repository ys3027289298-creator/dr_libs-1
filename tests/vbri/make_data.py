#!/usr/bin/env python3
# Generates the four VBRI test vectors with ffmpeg. Requires ffmpeg on PATH.
import os, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
BR = {1: [0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0],
      2: [0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0]}
SR = {1: [44100, 48000, 32000], 2: [22050, 24000, 16000], 25: [11025, 12000, 8000]}

def frame_size(h):
    ver = {3: 1, 2: 2, 0: 25}[(h[1] >> 3) & 3]
    layer = 4 - ((h[1] >> 1) & 3)
    kbps, sr = BR[1 if ver == 1 else 2][h[2] >> 4], SR[ver][(h[2] >> 2) & 3]
    samples = 384 if layer == 1 else (576 if ver != 1 else 1152)
    size = samples * kbps * 125 // sr
    return (size & ~3) + (4 * ((h[2] >> 1) & 1)) if layer == 1 else size + ((h[2] >> 1) & 1)

def first_frame(data):
    i = 0
    while i + 4 <= len(data) and not (data[i] == 0xFF and (data[i+1] & 0xE0) == 0xE0):
        i += 1
    return i

def walk(data, start):
    i, frames = start, 0
    while i + 4 <= len(data) and data[i] == 0xFF and (data[i+1] & 0xE0) == 0xE0:
        size = frame_size(data[i:i+4])
        if size <= 0 or i + size > len(data): break
        i, frames = i + size, frames + 1
    return frames, i

def make_src(name, channels, rate, kbps):
    src = os.path.join(HERE, name + ".src.mp3")
    subprocess.run(["ffmpeg", "-y", "-f", "lavfi", "-i",
                    "sine=frequency=440:duration=0.35", "-ac", str(channels),
                    "-ar", str(rate), "-write_xing", "0", "-b:a", str(kbps) + "k",
                    src, "-loglevel", "error"], check=True)
    return src

def write_vbri(src, dst, corrupt=False):
    data = bytearray(open(src, "rb").read())
    start = first_frame(data)
    frames, end = walk(data, start)
    assert frames > 1 and start + 62 <= len(data)
    tag = struct.pack(">4sHHHIIHH", b"VBRI", 1, 2, 100, end - start, frames, 0, 0)
    data[start + 36:start + 36 + len(tag)] = tag
    if corrupt:
        data[start + 36:start + 40] = b"XXXX"      # break magic; both sides fall back to file-length estimate
    open(os.path.join(HERE, dst), "wb").write(data)

if __name__ == "__main__":
    s = make_src("stereo", 2, 44100, 128)
    write_vbri(s, "vbri_stereo.mp3")
    write_vbri(s, "vbri_corrupt.mp3", corrupt=True)
    os.remove(s)
    m = make_src("mono", 1, 44100, 64)
    write_vbri(m, "vbri_mono.mp3")
    os.remove(m)
    os.rename(make_src("plain", 2, 44100, 128), os.path.join(HERE, "no_vbr_header.mp3"))
    print("generated:", sorted(f for f in os.listdir(HERE) if f.endswith(".mp3")))
