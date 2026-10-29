"""Analyse a Glide AA screenshot (TGA) of the qxpattern probe (key 0)."""
import sys, struct

def load_tga(path):
    d = open(path, 'rb').read()
    idlen, cmtype, itype = d[0], d[1], d[2]
    w, h = struct.unpack('<HH', d[12:16]); bpp = d[16]; desc = d[17]
    assert itype == 2 and bpp in (24, 32), (itype, bpp)
    n = bpp // 8; off = 18 + idlen
    rows = [d[off + y * w * n: off + (y + 1) * w * n] for y in range(h)]
    if not (desc & 0x20):          # bottom-up
        rows = rows[::-1]
    def px(x, y):
        r = rows[y]; b, g, rr = r[x*n], r[x*n+1], r[x*n+2]
        return (rr + g + b) / 3.0
    return w, h, px

def level(v):
    return 2 if v > 190 else (1 if v > 64 else 0)   # chips covering the pixel

def main(path):
    w, h, px = load_tga(path)
    print("TGA %dx%d" % (w, h))
    for axis in ('X', 'Y'):
        print("\n%s probe: strip edge offset k (pixels) -> chips covering pixel 98..102" % axis)
        cols = range(98, 103)
        for j in range(48):
            k = (j - 16) / 16.0
            if axis == 'X':
                vals = [level(px(c, 41 + 4 * j)) for c in cols]
                raw = [px(c, 41 + 4 * j) for c in cols]
            else:
                vals = [level(px(301 + 4 * j, c + 200)) for c in cols]
                raw = [px(301 + 4 * j, c + 200) for c in cols]
            print(" k=%+7.4f  %s   %s" % (k, ' '.join(str(v) for v in vals),
                                          ' '.join('%3d' % r for r in raw)))
if __name__ == '__main__':
    main(sys.argv[1])
