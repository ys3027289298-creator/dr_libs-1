#!/usr/bin/env python3
"""Generate 64-bit float (IEEE float, 64-bit) WAV test files into output/."""
import os
import struct

SR = 44100        # sample rate for all generated files
NFRAMES = 16      # frames per file
OUT = "output"

def sample(k):
    """Theoretical value of interleaved sample index k (shared with read_f64.c)."""
    return (k % 17) * 0.1 - 0.8

def fmt_chunk(ch):
    align = ch * 8  # 64-bit samples
    return struct.pack("<HHIIHH", 3, ch, SR, SR * align, align, 64)  # tag 3 = IEEE float

def wav(ch, extra=b""):
    data = b"".join(struct.pack("<d", sample(k)) for k in range(NFRAMES * ch)) + extra
    body = b"fmt " + struct.pack("<I", 16) + fmt_chunk(ch)
    body += b"data" + struct.pack("<I", len(data)) + data
    return b"RIFF" + struct.pack("<I", len(body) + 4) + b"WAVE" + body

def write(name, blob):
    path = os.path.join(OUT, name)
    with open(path, "wb") as f:
        f.write(blob)
    print("wrote %s (%d bytes)" % (path, len(blob)))

def main():
    os.makedirs(OUT, exist_ok=True)
    write("f64_stereo.wav", wav(2))                 # stereo
    write("f64_mono.wav", wav(1))                   # mono
    write("f64_odd_tail.wav", wav(1, b"\xAA"))      # odd trailing byte in data chunk
    bad = bytearray(wav(1))
    bad[12:16] = b"fmtX"                            # corrupted header: no fmt chunk
    write("f64_bad_header.wav", bytes(bad))

if __name__ == "__main__":
    main()
