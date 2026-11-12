#!/usr/bin/env python3
"""Generates the 4 ID3v2 test MP3 files into the output directory (argv[1], default "output")."""
import os, struct, sys

def syncsafe(n):
    return bytes([(n >> 21) & 0x7F, (n >> 14) & 0x7F, (n >> 7) & 0x7F, n & 0x7F])

def frame(fid, data, ver, size_override=None):
    n = len(data) if size_override is None else size_override
    size = syncsafe(n) if ver == 4 else struct.pack(">I", n & 0xFFFFFFFF)
    return fid.encode() + size + b"\x00\x00" + data

def text(fid, s, enc, ver):
    if enc == 1:
        body = b"\x01\xff\xfe" + s.encode("utf-16-le") + b"\x00\x00"
    else:
        body = b"\x00" + s.encode("latin-1") + b"\x00"
    return frame(fid, body, ver)

def apic(mime, pic, ver):
    return frame("APIC", b"\x00" + mime + b"\x00\x03cover\x00" + pic, ver)

def tag(frames, ver):
    body = b"".join(frames)
    return b"ID3" + bytes([ver, 0, 0]) + syncsafe(len(body)) + body

FRAME = bytes([0xFF, 0xFB, 0x90, 0x00]) + bytes(413)  # MPEG1 L3 128kbps 44.1kHz, silent payload
AUDIO = FRAME * 4
PNG = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]) + b"drmp3test"

out = sys.argv[1] if len(sys.argv) > 1 else "output"
os.makedirs(out, exist_ok=True)
cases = {
    "v23_utf16.mp3": tag([text("TIT2", "歌 Song", 1, 3), text("TPE1", "Artist A", 1, 3),
                          text("TALB", "Album X", 1, 3)], 3) + AUDIO,
    "v24_cover.mp3": tag([text("TIT2", "Cover Song", 0, 4), text("TPE1", "Artist B", 0, 4),
                          text("TALB", "Album Y", 0, 4), apic(b"image/png", PNG, 4)], 4) + AUDIO,
    "corrupt.mp3":   tag([text("TIT2", "Survivor", 0, 3), text("TPE1", "Artist C", 0, 3),
                          text("TALB", "Album Z", 0, 3),
                          frame("TXXX", b"\x00junk", 3, size_override=0x0FFFFFF0)], 3) + AUDIO,
    "notag.mp3":     AUDIO,
}
for name, data in cases.items():
    with open(os.path.join(out, name), "wb") as f:
        f.write(data)
    print("wrote %s (%d bytes)" % (name, len(data)))
