#!/usr/bin/env python3
"""Per-sample comparison of two raw PCM files (s16le or f32le).

Usage:
    python3 -I opus_compare.py <a.raw> <b.raw> <s16|f32> [--offset-a N] [--offset-b N]

Prints sample count compared, max absolute error, RMS error, and the max error
expressed in 16-bit LSBs. For f32 input, errors are in full-scale units where
1.0 = full scale; 1 LSB = 1/32768.
"""
import struct
import sys
import math


def read_samples(path, fmt):
    with open(path, "rb") as f:
        data = f.read()
    if fmt == "s16":
        count = len(data) // 2
        return list(struct.unpack("<%dh" % count, data[: count * 2])), 32768.0
    else:
        count = len(data) // 4
        return list(struct.unpack("<%df" % count, data[: count * 4])), 1.0


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    opts = {k: int(v) for k, v in (a[2:].split("=") for a in argv[1:] if a.startswith("--"))}
    if len(args) != 3:
        print(__doc__)
        return 1
    a, scale_a = read_samples(args[0], args[2])
    b, scale_b = read_samples(args[1], args[2])
    off_a = opts.get("offset-a", 0)
    off_b = opts.get("offset-b", 0)
    a = a[off_a:]
    b = b[off_b:]
    n = min(len(a), len(b))
    if n == 0:
        print("no samples to compare")
        return 1

    max_err = 0.0
    sum_sq = 0.0
    max_at = -1
    for i in range(n):
        e = abs(a[i] / scale_a - b[i] / scale_b)
        if e > max_err:
            max_err = e
            max_at = i
        sum_sq += e * e
    rms = math.sqrt(sum_sq / n)

    print("samples=%d (lenA=%d lenB=%d)" % (n, len(a), len(b)))
    print("max_abs_err=%.9f (%.4f LSB16) at sample %d" % (max_err, max_err * 32768.0, max_at))
    print("rms_err=%.9f (%.4f LSB16)" % (rms, rms * 32768.0))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
