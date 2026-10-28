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

TEST_SPRITE = split16("""
.....111111.....
....12222221....
...1222222221...
...1223223221...
...1223223221...
...1222222221...
...1221111221...
....12222221....
.....111111.....
....13333331....
...1333333331...
...1333333331...
...1333333331...
....11111111....
....12.....21...
....11.....11...
""")

bg_tiles = [BLANK] + BRICK + STONE          # tiles 0, 1-4, 5-8
sprite_tiles = TEST_SPRITE                  # tiles 0-3

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, "..", "chr")
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, "bg.chr"), "wb") as f:
    f.write(build(bg_tiles))
with open(os.path.join(out, "sprites.chr"), "wb") as f:
    f.write(build(sprite_tiles))
print("wrote chr/bg.chr and chr/sprites.chr")