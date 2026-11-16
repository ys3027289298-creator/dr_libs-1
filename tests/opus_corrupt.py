#!/usr/bin/env python3
"""Deterministically corrupt one Ogg page's payload in an Opus file.

Parses the Ogg page structure, selects the audio page nearest to the given
fraction of the file (default 0.5) and XORs a run of payload bytes with 0xFF.
The page CRC is left untouched, so a conforming demuxer must detect the bad
page, skip it and resynchronize at the next page.

Usage:
    python3 -I opus_corrupt.py <in.opus> <out.opus> [fraction] [nbytes]
"""
import struct
import sys


def pages(data):
    off = 0
    result = []
    while off + 27 <= len(data):
        assert data[off:off + 4] == b"OggS", "bad capture at %d" % off
        nseg = data[off + 26]
        header_size = 27 + nseg
        body_size = sum(data[off + 27:off + 27 + nseg])
        result.append((off, header_size, body_size))
        off += header_size + body_size
    assert off == len(data), "trailing garbage"
    return result


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    src, dst = argv[1], argv[2]
    fraction = float(argv[3]) if len(argv) > 3 else 0.5
    nbytes = int(argv[4]) if len(argv) > 4 else 32

    data = bytearray(open(src, "rb").read())
    pgs = pages(data)
    # Skip the header pages (first two pages carry OpusHead/OpusTags).
    audio_pages = pgs[2:]
    index = min(int(fraction * len(audio_pages)), len(audio_pages) - 1)
    off, header_size, body_size = audio_pages[index]
    start = off + header_size + body_size // 3
    for i in range(start, min(start + nbytes, off + header_size + body_size)):
        data[i] ^= 0xFF
    open(dst, "wb").write(bytes(data))
    print("corrupted page %d of %d (file offset %d, %d payload bytes at offset %d)"
          % (index + 2, len(pgs), off, min(nbytes, body_size - body_size // 3), start))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
