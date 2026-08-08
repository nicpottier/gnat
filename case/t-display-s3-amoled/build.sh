#!/bin/sh
# regenerate every printable stl and the assembly render from the scad
# source
set -e
cd "$(dirname "$0")"

mkdir -p stl
for part in tray face foot foot45 plug; do
  echo "building stl/$part.stl"
  openscad -o "stl/$part.stl" -D "layout=\"$part\"" model.scad
done

# isometric render of the assembled case. openscad can't emit a
# transparent background, so key out the solid backdrop color afterward
echo "building assembly.png"
openscad -o assembly.png --imgsize=1600,1200 --projection=o \
  --autocenter --viewall --camera=0,0,0,65,0,125,120 \
  -D 'layout="assembly"' model.scad

python3 - assembly.png <<'EOF'
# make the backdrop color transparent, stdlib only
import struct, sys, zlib

path = sys.argv[1]
data = open(path, "rb").read()
assert data[:8] == b"\x89PNG\r\n\x1a\n"

pos, idat, w = 8, b"", None
while pos < len(data):
    length, kind = struct.unpack(">I4s", data[pos:pos + 8])
    body = data[pos + 8:pos + 8 + length]
    if kind == b"IHDR":
        w, h, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", body)
        assert depth == 8 and color in (2, 6) and interlace == 0
        bpp = 3 if color == 2 else 4
    elif kind == b"IDAT":
        idat += body
    pos += 12 + length

raw = zlib.decompress(idat)
stride = w * bpp
prior = bytearray(stride)
rows = []
for y in range(h):
    at = y * (stride + 1)
    f, line = raw[at], bytearray(raw[at + 1:at + 1 + stride])
    for i in range(stride):
        a = line[i - bpp] if i >= bpp else 0
        b = prior[i]
        if f == 1:
            line[i] = (line[i] + a) & 255
        elif f == 2:
            line[i] = (line[i] + b) & 255
        elif f == 3:
            line[i] = (line[i] + (a + b) // 2) & 255
        elif f == 4:
            c = prior[i - bpp] if i >= bpp else 0
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
            line[i] = (line[i] + pr) & 255
    rows.append(line)
    prior = line

bg = rows[0][0:3]
out = bytearray()
for line in rows:
    out.append(0)
    for x in range(w):
        px = line[x * bpp:x * bpp + 3]
        alpha = 0 if max(abs(px[i] - bg[i]) for i in range(3)) <= 6 else 255
        out += px + bytes([alpha])

def chunk(kind, body):
    return (struct.pack(">I", len(body)) + kind + body +
            struct.pack(">I", zlib.crc32(kind + body) & 0xffffffff))

open(path, "wb").write(
    b"\x89PNG\r\n\x1a\n" +
    chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
    chunk(b"IDAT", zlib.compress(bytes(out), 9)) +
    chunk(b"IEND", b""))
EOF
