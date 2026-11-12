#!/usr/bin/env python3
"""Builds four small MP3 files carrying ID3v2.3 TXXX Replay Gain tags."""
import os, subprocess, struct, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "tests/gain"
BASE = OUT + "/base.mp3"
subprocess.run(["ffmpeg", "-y", "-f", "lavfi", "-i", "sine=frequency=440:duration=0.3",
                "-q:a", "9", "-map_metadata", "-1", "-write_xing", "0",
                "-fflags", "-bitexact", "-flags", "-bitexact", BASE], check=True,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def sync(n):
    return bytes([(n >> 21) & 0x7F, (n >> 14) & 0x7F, (n >> 7) & 0x7F, n & 0x7F])

def txxx(desc, val):
    body = b"\x00" + desc.encode() + b"\x00" + val.encode()
    return b"TXXX" + struct.pack(">I", len(body)) + b"\x00\x00" + body

def tag(frames):
    body = b"".join(frames)
    return b"ID3\x03\x00\x00" + sync(len(body)) + body

with open(BASE, "rb") as f:
    mp3 = f.read()

if mp3[:3] == b"ID3":  # Strip any tag ffmpeg insisted on writing so no_tag.mp3 is genuinely tagless.
    body_size = sum(b << (7 * (3 - k)) for k, b in enumerate(mp3[6:10]))
    mp3 = mp3[10 + body_size + (10 if mp3[5] & 0x10 else 0):]

jobs = {
    "track_gain.mp3":  [txxx("REPLAYGAIN_TRACK_GAIN", "-3.45 dB")],
    "album_gain_peak.mp3": [txxx("REPLAYGAIN_ALBUM_GAIN", "+1.20 dB"),
                            txxx("REPLAYGAIN_ALBUM_PEAK", "0.876543")],
    "mixed_case.mp3":  [txxx("ReplayGain_Track_Gain", "-7.50 dB"),
                        txxx("replaygain album peak", "0.998001"),
                        txxx("Replaygain_Album_Gain: 2.25 dB", ""),
                        txxx("REPLAYGAIN_TRACK_PEAK=0.555", ""),
                        txxx("REPLAYGAIN_ALBUM_PEAK", "not-a-number")],
}
for name, frames in jobs.items():
    with open(OUT + "/" + name, "wb") as f:
        f.write(tag(frames) + mp3)

with open(OUT + "/no_tag.mp3", "wb") as f:
    f.write(mp3)

os.remove(BASE)
print("generated:", ", ".join(sorted(jobs)) + ", no_tag.mp3")
