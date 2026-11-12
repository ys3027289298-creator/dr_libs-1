import warnings, struct, sys
warnings.filterwarnings("ignore", category=DeprecationWarning)
import aifc

CASES = [
    ("output/mono16.aiff",    1, 16, 512, lambda i: i * 10 - 500),
    ("output/stereo24.aiff",  2, 24, 512, lambda i: i * 1000 - 500000),
    ("output/stereo32f.aiff", 2, 32, 512, lambda i: i * 0.25 - 3.0),
    ("output/empty.aiff",     1, 16, 0,   lambda i: 0),
]

def decode(raw, bits):
    if bits == 16:
        return list(struct.unpack(">%dh" % (len(raw) // 2), raw))
    if bits == 24:
        out = []
        for i in range(0, len(raw), 3):
            v = (raw[i] << 16) | (raw[i+1] << 8) | raw[i+2]
            out.append(v - (1 << 24) if v & 0x800000 else v)
        return out
    return list(struct.unpack(">%df" % (len(raw) // 4), raw))

ok = True
for path, ch, bits, frames, expect in CASES:
    with aifc.open(path, "rb") as f:
        nch, sw, nf, rate = f.getnchannels(), f.getsampwidth(), f.getnframes(), f.getframerate()
        got = decode(f.readframes(nf), bits)
    diffs = [abs(got[i] - expect(i)) for i in range(min(8, len(got)))]
    good = (nch, sw * 8, nf, rate) == (ch, bits, frames, 44100) and max(diffs or [0]) == 0
    ok &= good
    print("%s: ch=%d bits=%d frames=%d rate=%d first8_diff=%s [%s]"
          % (path, nch, sw * 8, nf, rate, diffs, "OK" if good else "FAIL"))
print("ALL OK" if ok else "FAILED")
sys.exit(0 if ok else 1)
