#!/usr/bin/env python3
"""PAKET 33: Prozedurale Demo-Windowskin im XP-Stil (128x128, Nine-Patch
links oben 96x96, Rand 16 px). Ausgabe ohne Abhaengigkeiten (zlib)."""
import struct, zlib, os, sys

W = H = 128
FRAME = 96   # Nine-Patch-Quellflaeche
BORDER = 16  # Randdicke

def px(x, y):
    if x < FRAME and y < FRAME:
        inner = BORDER <= x < FRAME - BORDER and BORDER <= y < FRAME - BORDER
        if inner:
            # Face: dunkelblauer Verlauf + dezentes Gewebe
            t = (y - BORDER) / (FRAME - 2 * BORDER)
            r = 16 + int(10 * t)
            g = 22 + int(14 * t)
            b = 46 + int(34 * t)
            v = 6 if ((x // 4 + y // 4) % 2 == 0) else 0
            return (r + v, g + v, b + v, 232)
        # Rahmen: heller Rand, abgerundet wirkende Ecken
        d = min(x, y, FRAME - 1 - x, FRAME - 1 - y)  # Abstand zur Aussenkante
        if d < 2:
            return (236, 240, 252, 255)
        glow = max(0, (d - 2)) / (BORDER - 2)
        r = 70 + int(90 * (1 - glow))
        g = 96 + int(100 * (1 - glow))
        b = 160 + int(80 * (1 - glow))
        return (r, g, b, 255)
    # Rest vom Sheet: Cursor-Streifen (32x32 @ 96..128) fuer spaeter
    if 96 <= x < 128 and 0 <= y < 32:
        t = (x - 96) / 32.0
        return (40 + int(30 * t), 60 + int(50 * t), 120 + int(70 * t), 200)
    return (0, 0, 0, 0)

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "SampleProject/Graphics/System/windowskin.png"
    os.makedirs(os.path.dirname(out), exist_ok=True)
    raw = b""
    for y in range(H):
        raw += b"\x00"  # Filter: None
        for x in range(W):
            raw += bytes(px(x, y))
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(out, "wb") as f:
        f.write(png)
    print("ok ->", out, len(png), "B")

if __name__ == "__main__":
    main()
