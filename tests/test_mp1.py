#!/usr/bin/env python3
"""MPEG-1 Layer I conformance for dr_mp3: 44.1k stereo, 48k mono, joint stereo, bad frame."""
import math, os, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "output")
BR = {32:1,64:2,96:3,128:4,160:5,192:6,224:7,256:8,288:9,320:10,352:11,384:12,416:13,448:14}; SR = {44100:0, 48000:1, 32000:2}
BA = {v: k for k, v in enumerate([0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16])}

class BW:
    def __init__(self): self.b = []
    def w(self, v, n): self.b += [(v >> k) & 1 for k in range(n-1, -1, -1)]
    def bytes(self):
        x = self.b + [0] * (-len(self.b) % 8)
        return bytes(sum(x[i+k] << (7-k) for k in range(8)) for i in range(0, len(x), 8))
def quant(s, n):
    h = 1 << (n-1)
    return max(0, min(2*h-1, int(round(s*h + h - 1))))
def frame(pcm, sr, mode, ext=0, bitrate=256, pad=0, bands=8, nbits=6):
    nch = 1 if mode == 3 else 2
    size = ((384*bitrate*125)//sr & ~3) + 4*pad
    hdr = bytes([0xFF, 0xFF, (BR[bitrate] << 4) | (SR[sr] << 2) | (pad << 1), (mode << 6) | (ext << 4)])
    joint = 0 if mode == 3 else (ext*4 + 4 if mode == 1 else 32)
    on = lambda sb, ch: sb < bands and not (ch == 1 and sb >= joint)
    bw = BW()
    for sb in range(32):
        for ch in range(nch):
            if not (ch == 1 and sb >= joint): bw.w(BA[nbits if sb < bands else 0], 4)
            if on(sb, ch): bw.w(0, 6)
    for s in range(12):
        for sb in range(32):
            for ch in range(nch):
                if on(sb, ch): bw.w(quant(pcm[ch][sb*12+s], nbits), nbits)
    body = bw.bytes()
    return hdr + body + b"\x00"*(size - 4 - len(body))
def make(path, sr, mode, ext, corrupt=False, n=6):
    nch = 1 if mode == 3 else 2
    out = bytearray()
    for f in range(n):
        pcm = [[0.3*math.sin(2*math.pi*440*(f+1)*i/sr) for i in range(384)] for _ in range(nch)]
        fr = frame(pcm, sr, mode, ext, pad=(f % 3 == 2))
        if corrupt and f == n//2: fr = fr[:4] + b"\xff"*(len(fr)-4)
        out += fr
    open(path, "wb").write(out)
CASES = [("stereo_44100",44100,0,0,False), ("mono_48000",48000,3,0,False), ("joint_stereo",44100,1,1,False), ("bad_frame",44100,0,0,True)]
def main():
    os.makedirs(OUT, exist_ok=True)
    driver = os.path.join(OUT, "mp1_driver")
    if not os.path.exists(driver):
        subprocess.run(["gcc", os.path.join(ROOT, "tests", "mp1_decode_driver.c"), "-o", driver, "-lm"], check=True)
    report = []
    for name, sr, mode, ext, corrupt in CASES:
        path = os.path.join(OUT, name + ".mp1")
        make(path, sr, mode, ext, corrupt)
        out = subprocess.run([driver, path], capture_output=True, text=True, check=True).stdout.strip()
        report.append(f"== {name} ==\n{out}")
    open(os.path.join(OUT, "results.txt"), "w").write("\n".join(report) + "\n")
    print("\n".join(report))
if __name__ == "__main__":
    main()
