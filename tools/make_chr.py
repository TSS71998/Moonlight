#!/usr/bin/env python3
"""Builds chr/bg.chr and chr/sprites.chr (4 KB each) from text tile art.

Each tile is 8 rows of 8 characters: '.' = color 0, '1', '2', '3' = colors 1-3
of the palette the tile is drawn with. Tiles are numbered in list order.

These are placeholders. Once you have real art, draw it in NEXXT or YY-CHR
and save straight to chr/*.chr (then stop running this script).
"""
import os

def tile_bytes(rows):
    assert len(rows) == 8 and all(len(r) == 8 for r in rows), rows
    lo, hi = [], []
    for r in rows:
        l = h = 0
        for ch in r:
            v = 0 if ch == "." else int(ch)
            l = (l << 1) | (v & 1)
            h = (h << 1) | (v >> 1)
        lo.append(l)
        hi.append(h)
    return bytes(lo + hi)        # NES format: 8 bytes plane 0, 8 bytes plane 1

def build(tiles, size=4096):
    data = b"".join(tile_bytes(t) for t in tiles)
    assert len(data) <= size
    return data + bytes(size - len(data))

def split16(art):
    """16x16 art (16 strings of 16 chars) -> 4 tiles: TL, TR, BL, BR."""
    rows = art.split()
    assert len(rows) == 16
    return [[r[:8] for r in rows[:8]], [r[8:] for r in rows[:8]],
            [r[:8] for r in rows[8:]], [r[8:] for r in rows[8:]]]

BLANK = ["........"] * 8

def split_8x16(art, width=16):
    """Art for 8x16 sprite mode -> tiles in the order the PPU wants them.

    In 8x16 mode each hardware sprite is a pair of tiles (top, bottom), so the
    art is cut into 8x16 strips: column by column within each 16-row band.
    For a 16x32 character that gives 8 tiles:
      0-1 top left, 2-3 top right, 4-5 bottom left, 6-7 bottom right.
    """
    rows = art.split()
    assert len(rows) % 16 == 0 and all(len(r) == width for r in rows), \
        [len(r) for r in rows]
    tiles = []
    for band in range(0, len(rows), 16):
        for col in range(0, width, 8):
            strip = [r[col:col + 8] for r in rows[band:band + 16]]
            tiles += [strip[:8], strip[8:]]
    return tiles



HERO = split_8x16("""
.....111111.....
....11111111....
...1111111111...
...1113333111...
...1133333311...
...1133313331...
...1133333331...
....11333331....
.....133331.....
......1331......
....11222211....
...1222222221...
..122222222221..
..123222222321..
..133222222331..
..133222222331..
..133111111331..
...3122222213...
....12222221....
....12222221....
....12211221....
....1221.1221...
....1221.1221...
....1221.1221...
....1221.1221...
....1221.1221...
....1221.1221...
....1111.1111...
....1111.1111...
...11111.11111..
...11111.11111..
...11111.11111..
""")

BLANK = ["........"] * 8

BRICK = split16("""
3333333313333333
2222222212222222
2222222212222222
2222222212222222
2222222212222222
2222222212222222
2222222212222222
1111111111111111
3333133333333333
2222122222222222
2222122222222222
2222122222222222
2222122222222222
2222122222222222
2222122222222222
1111111111111111
""")

STONE = split16("""
3333333333333332
3222222222222221
3222222222222221
3222222222222221
3222212222222221
3222221222222221
3222222222222221
3222222222222221
3222222222222221
3222222222222221
3222222222122221
3222222222212221
3222222222222221
3222222222222221
3222222222222221
2111111111111111
""")

bg_tiles = [BLANK] + BRICK + STONE          # tiles 0, 1-4, 5-8
sprite_tiles = HERO            # tiles 0-3

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, "..", "chr")
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, "bg.chr"), "wb") as f:
    f.write(build(bg_tiles))
with open(os.path.join(out, "sprites.chr"), "wb") as f:
    f.write(build(sprite_tiles))
print("wrote chr/bg.chr and chr/sprites.chr")