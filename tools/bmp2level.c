/* bmp2level.c - turns levels drawn in Aseprite (levels/NAME.bmp, with
 * NAME_collision.bmp and NAME.pal) into screens for nam2level, and each
 * level's own background tiles: one 4 KB CHR bank per level, with the
 * status bar font from chr/bg.chr at $C0-$FF. Run by the Makefile:
 *
 *   bmp2level -font chr/bg.chr -chr build/bg.chr -chr-more build/bg_more.chr
 *             -h build/level_chr.h -o build/lvl levels/NAME.bmp ...
 *
 * build/bg.chr is the first level's tiles (CHR bank 0), bg_more.chr the
 * others' (CHR banks 3 on), and level_chr.h says which bank is whose:
 * #define CHR_forest 0. In build/lvl: NAME_0.nam..., NAME.pal and
 * NAME_collision.txt for each level. See docs/DEVELOPMENT.md, "Levels".
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W    256
#define SCREEN_H    240
#define MAX_SCREENS 32
#define HUD_H       32              /* must match hud.h: HUD_ROWS * 8 */
#define LEVEL_TILES 0xC0            /* tiles $00-$BF are the level's */
#define MAX_W       (SCREEN_W * MAX_SCREENS)
#define MAX_BLOCKS  256             /* must match nam2level.c: MAX_METATILES */

typedef struct {
    int w, h;
    unsigned char *px;              /* palette index per pixel */
    unsigned rgb[256];              /* palette: 0xRRGGBB */
    int n_colours;
    int indexed;
} Image;

typedef struct {
    unsigned char chr[16];          /* NES tile: 2 bit planes */
    unsigned char solid;
} Tile;

static Tile tiles[4096];         /* more than fit, so the error can say how many */
static int n_tiles;              /* in the level being made: each level has its own */

/* Each level gets its own 4 KB CHR bank of background tiles. The first is
   CHR bank 0; the others go after the sprite and title banks, from bank 3:
   up to MAX_LEVELS in all (CHR-ROM is 8 banks of 4 KB). */
#define MAX_LEVELS  6
static unsigned char chr_banks[MAX_LEVELS][4096];
static unsigned char font_chr[4096];
static char level_names[MAX_LEVELS][256];
static int n_levels;
static int level_blocks;
static int scr_new_blocks[MAX_SCREENS];

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "bmp2level: ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static unsigned rd16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned long rd32(const unsigned char *p) {
    return (unsigned long)p[0] | (unsigned long)p[1] << 8 |
           (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}

/* Reads an uncompressed BMP: 1, 4 or 8 bits (indexed) or 24/32 bits.
   For 24/32-bit pictures px is 0 for black (or transparent) and 1 for
   anything else, which is all a collision picture needs. */
static int load_bmp(const char *path, Image *im) {
    FILE *f = fopen(path, "rb");
    unsigned char hdr[54], *row;
    unsigned long off, hsize, comp, ncol;
    long w, h;
    int bpp, flip, x, y, rowbytes, i;

    if (!f) return 0;
    if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[1] != 'M')
        die("%s: not a BMP file", path);
    off = rd32(hdr + 10);
    hsize = rd32(hdr + 14);
    w = (long)rd32(hdr + 18);
    h = (long)rd32(hdr + 22);
    if (h & 0x80000000L) h -= 0x100000000L;
    bpp = (int)rd16(hdr + 28);
    comp = rd32(hdr + 30);
    ncol = rd32(hdr + 46);
    if (hsize < 40) die("%s: unsupported BMP (old OS/2 format)", path);
    if (comp != 0 && !(comp == 3 && bpp == 32))
        die("%s: compressed BMP; save it uncompressed (no RLE)", path);
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)
        die("%s: %d bits per pixel isn't supported; save as an indexed "
            "(4- or 8-bit) BMP", path, bpp);

    flip = h > 0;                   /* normal BMPs are stored bottom-up */
    if (h < 0) h = -h;
    if (w <= 0 || h <= 0 || w > MAX_W || h > 4096) die("%s: bad size %ldx%ld", path, w, h);
    im->w = (int)w;
    im->h = (int)h;
    im->indexed = bpp <= 8;
    memset(im->rgb, 0, sizeof(im->rgb));
    im->n_colours = 0;

    if (im->indexed) {
        unsigned char q[4];
        if (ncol == 0 || ncol > (1UL << bpp)) ncol = 1UL << bpp;
        fseek(f, 14 + (long)hsize, SEEK_SET);
        for (i = 0; i < (int)ncol; ++i) {
            if (fread(q, 1, 4, f) != 4) die("%s: palette cut short", path);
            im->rgb[i] = (unsigned)q[2] << 16 | (unsigned)q[1] << 8 | q[0];
        }
        im->n_colours = (int)ncol;
    }

    rowbytes = (int)(((long)w * bpp + 31) / 32 * 4);
    row = malloc((size_t)rowbytes);
    im->px = malloc((size_t)(w * h));
    if (!row || !im->px) die("out of memory");
    fseek(f, (long)off, SEEK_SET);
    for (y = 0; y < h; ++y) {
        unsigned char *dst = im->px + (size_t)(flip ? h - 1 - y : y) * (size_t)w;
        if (fread(row, 1, (size_t)rowbytes, f) != (size_t)rowbytes)
            die("%s: picture data cut short", path);
        for (x = 0; x < w; ++x) {
            switch (bpp) {
            case 1: dst[x] = (row[x >> 3] >> (7 - (x & 7))) & 1; break;
            case 4: dst[x] = (x & 1) ? row[x >> 1] & 15 : row[x >> 1] >> 4; break;
            case 8: dst[x] = row[x]; break;
            case 24: {
                const unsigned char *p = row + x * 3;
                dst[x] = (p[0] | p[1] | p[2]) ? 1 : 0;
                break;
            }
            default: {              /* 32: BGRA; alpha 0 counts as empty */
                const unsigned char *p = row + x * 4;
                dst[x] = ((p[0] | p[1] | p[2]) && p[3]) ? 1 : 0;
                break;
            }
            }
        }
    }
    free(row);
    fclose(f);
    return 1;
}

/* "levels/forest.bmp" -> name "forest", base "levels/forest" */
static void split_path(const char *path, char *base, size_t bsize, char *name, size_t nsize) {
    const char *slash = strrchr(path, '/'), *bslash = strrchr(path, '\\');
    const char *start = slash > bslash ? slash : bslash;
    const char *dot = strrchr(path, '.');
    size_t len = strlen(path);
    start = start ? start + 1 : path;
    if (dot && dot > start) len = (size_t)(dot - path);
    snprintf(base, bsize, "%.*s", (int)len, path);
    snprintf(name, nsize, "%.*s", (int)(path + len - start), start);
}

static long read_file(const char *path, unsigned char *buf, long size) {
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, (size_t)size, f);
    fclose(f);
    return n;
}

static void write_file(const char *path, const unsigned char *buf, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(buf, 1, size, f) != size) die("%s: can't write", path);
    fclose(f);
}


/* a 24-bit BMP, for the block report; rgb is rows of `stride` pixels */
static void write_bmp24(const char *path, int w, int h, const unsigned char *rgb, int stride) {
    FILE *f = fopen(path, "wb");
    int rowbytes = (w * 3 + 3) & ~3, x, y;
    unsigned long size = 54UL + (unsigned long)rowbytes * (unsigned long)h;
    unsigned char hdr[54], pad[3] = { 0, 0, 0 };
    if (!f) die("%s: can't write", path);
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = size & 255; hdr[3] = size >> 8 & 255; hdr[4] = size >> 16 & 255; hdr[5] = size >> 24 & 255;
    hdr[10] = 54; hdr[14] = 40;
    hdr[18] = w & 255; hdr[19] = w >> 8 & 255;
    hdr[22] = h & 255; hdr[23] = h >> 8 & 255;
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    for (y = h - 1; y >= 0; --y) {
        for (x = 0; x < w; ++x) {
            const unsigned char *p = rgb + ((size_t)y * (size_t)stride + (size_t)x) * 3;
            unsigned char bgr[3];
            bgr[0] = p[2]; bgr[1] = p[1]; bgr[2] = p[0];
            fwrite(bgr, 1, 3, f);
        }
        fwrite(pad, 1, (size_t)(rowbytes - w * 3), f);
    }
    fclose(f);
}

static int add_tile(const Tile *t) {
    int i;
    for (i = 0; i < n_tiles; ++i)
        if (!memcmp(&tiles[i], t, sizeof(Tile))) return i;
    if (n_tiles == 4096) die("the levels need over 4096 different 8x8 tiles; only %d fit", LEVEL_TILES);
    tiles[n_tiles] = *t;
    return n_tiles++;
}

static void convert_level(const char *path, const char *out_dir) {
    static unsigned char nam[MAX_SCREENS][1024];
    static unsigned char solid_cell[SCREEN_H / 8][MAX_W / 8];
    char base[1024], name[256], cpath[1100], fpath[1200];
    unsigned char pal[16];
    Image im, col;
    int n_scr, used_scr, sx, bx, by, x, y, i, hidden = 0;
    Tile blank;
    long pal_size;

    split_path(path, base, sizeof(base), name, sizeof(name));
    if (n_levels == MAX_LEVELS) die("%s: more than %d levels; CHR-ROM has room for %d", path, MAX_LEVELS, MAX_LEVELS);
    snprintf(level_names[n_levels], sizeof(level_names[0]), "%s", name);
    for (i = 0; level_names[n_levels][i]; ++i)          /* (a C name for level_chr.h) */
        if (!isalnum((unsigned char)level_names[n_levels][i])) level_names[n_levels][i] = '_';

    /* this level's tiles start empty: tile $00 blank, not solid */
    n_tiles = 0;
    memset(&blank, 0, sizeof(blank));
    add_tile(&blank);
    if (!load_bmp(path, &im)) die("%s: can't open", path);
    if (!im.indexed)
        die("%s: the level picture must be indexed (4- or 8-bit, 16 colours "
            "in 4 palettes of 4). In Aseprite: Sprite > Color Mode > Indexed.", path);
    if (im.h != SCREEN_H) die("%s: %d pixels tall, a level must be %d", path, im.h, SCREEN_H);
    if (im.w % SCREEN_W) die("%s: %d pixels wide, must be a multiple of %d (one screen each)",
                             path, im.w, SCREEN_W);
    n_scr = im.w / SCREEN_W;
    if (n_scr > MAX_SCREENS) die("%s: %d screens, the limit is %d", path, n_scr, MAX_SCREENS);
    for (i = 0; i < im.w * im.h; ++i)
        if (im.px[i] >= 16)
            die("%s: uses colour %d; only the first 16 (4 palettes of 4) can be used",
                path, im.px[i]);

    /* collision picture */
    snprintf(cpath, sizeof(cpath), "%s_collision.bmp", base);
    if (!load_bmp(cpath, &col))
        die("%s: missing. Draw what's solid (any colour on black) at the same "
            "size as %s and save it there.", cpath, path);
    if (col.w != im.w || col.h != im.h)
        die("%s: %dx%d, but %s is %dx%d; they must be the same size",
            cpath, col.w, col.h, path, im.w, im.h);
    memset(solid_cell, 0, sizeof(solid_cell));
    for (y = 0; y < im.h; ++y)
        for (x = 0; x < im.w; ++x) {
            unsigned char c = col.px[(size_t)y * (size_t)col.w + (size_t)x];
            int on = col.indexed ? (c != 0 && col.rgb[c] != 0) : c;
            if (on) solid_cell[y >> 3][x >> 3] = 1;
        }

    /* the status bar hides the top: leave it out */
    for (y = 0; y < HUD_H; ++y)
        for (x = 0; x < im.w; ++x) {
            if (im.px[(size_t)y * (size_t)im.w + (size_t)x] & 3) hidden = 1;
            im.px[(size_t)y * (size_t)im.w + (size_t)x] = 0;
        }
    for (y = 0; y < HUD_H / 8; ++y)
        for (x = 0; x < im.w / 8; ++x) solid_cell[y][x] = 0;

    /* empty screens at the end are left out */
    used_scr = n_scr;
    while (used_scr > 1) {
        int any = 0;
        for (y = 0; y < im.h && !any; ++y)
            for (x = (used_scr - 1) * SCREEN_W; x < used_scr * SCREEN_W; ++x)
                if ((im.px[(size_t)y * (size_t)im.w + (size_t)x] & 3) ||
                    solid_cell[y >> 3][x >> 3]) { any = 1; break; }
        if (any) break;
        --used_scr;
    }

    memset(nam, 0, sizeof(nam));
    for (by = 0; by < SCREEN_H / 16; ++by) {
        for (bx = 0; bx < used_scr * 16; ++bx) {
            /* the colours this 16x16 square uses, and the palettes of the
               colour numbers it was drawn with */
            unsigned cols[64];
            int n_cols = 0, votes[4] = {0, 0, 0, 0}, p, best = -1, k, j;
            for (y = 0; y < 16; ++y)
                for (x = 0; x < 16; ++x) {
                    unsigned char c = im.px[(size_t)(by * 16 + y) * (size_t)im.w + (size_t)(bx * 16 + x)];
                    if (!(c & 3)) continue;
                    ++votes[c >> 2];
                    for (k = 0; k < n_cols; ++k)
                        if (cols[k] == im.rgb[c]) break;
                    if (k == n_cols && n_cols < 64) cols[n_cols++] = im.rgb[c];
                }
            /* the palette (with the most votes) that has all of them */
            for (p = 0; p < 4; ++p) {
                int fits = 1;
                for (k = 0; k < n_cols && fits; ++k) {
                    int in = 0;
                    for (j = 1; j < 4; ++j)
                        if (im.rgb[p * 4 + j] == cols[k]) in = 1;
                    if (!in) fits = 0;
                }
                if (fits && (best < 0 || votes[p] > votes[best])) best = p;
            }
            if (best < 0) {
                fprintf(stderr, "bmp2level: %s: the 16x16 square at x=%d y=%d (screen %d) uses "
                        "colours from more than one palette:", path, bx * 16, by * 16, bx / 16);
                for (k = 0; k < n_cols; ++k) fprintf(stderr, " #%06X", cols[k]);
                fputc('\n', stderr);
                exit(1);
            }

            /* its four 8x8 tiles */
            for (j = 0; j < 4; ++j) {
                int tx = bx * 2 + (j & 1), ty = by * 2 + (j >> 1), t;
                Tile tile;
                memset(&tile, 0, sizeof(tile));
                for (y = 0; y < 8; ++y)
                    for (x = 0; x < 8; ++x) {
                        unsigned char c = im.px[(size_t)(ty * 8 + y) * (size_t)im.w + (size_t)(tx * 8 + x)];
                        int v = 0;
                        if (c & 3) {
                            for (v = 1; v < 4; ++v)     /* its own slot if it's this palette's */
                                if ((c >> 2) == best && (c & 3) == v) break;
                            if (v == 4)
                                for (v = 1; v < 4; ++v)
                                    if (im.rgb[best * 4 + v] == im.rgb[c]) break;
                        }
                        if (v & 1) tile.chr[y] |= (unsigned char)(0x80 >> x);
                        if (v & 2) tile.chr[y + 8] |= (unsigned char)(0x80 >> x);
                    }
                tile.solid = solid_cell[ty][tx];
                t = add_tile(&tile);
                nam[tx / 32][ty * 32 + (tx & 31)] = (unsigned char)t;
            }

            /* attribute: 2 bits per 16x16 square */
            sx = bx / 16;
            nam[sx][960 + (by >> 1) * 8 + ((bx & 15) >> 1)] |=
                (unsigned char)(best << (((by & 1) << 2) | ((bx & 1) << 1)));
        }
    }

    if (n_tiles > LEVEL_TILES)
        die("%s needs %d different 8x8 tiles; a level can have %d (tiles $C0-$FF "
            "are the status bar font). Reuse more of the same pieces.",
            path, n_tiles, LEVEL_TILES);

    /* 16x16 blocks: nam2level makes one per different block (its 4 tiles
       and palette), and a level can have at most MAX_BLOCKS */
    {
        static unsigned char blk[MAX_SCREENS * 16 * 15][5];
        static unsigned short blk_of[MAX_SCREENS][15][16];
        static unsigned short uses[MAX_SCREENS * 16 * 15];
        int n_blk = 0, k, once = 0;
        for (sx = 0; sx < used_scr; ++sx) {
            int before = n_blk;
            for (by = 0; by < 15; ++by)
                for (bx = 0; bx < 16; ++bx) {
                    const unsigned char *n = nam[sx];
                    unsigned char b[5];
                    b[0] = n[by * 64 + bx * 2];
                    b[1] = n[by * 64 + bx * 2 + 1];
                    b[2] = n[by * 64 + 32 + bx * 2];
                    b[3] = n[by * 64 + 32 + bx * 2 + 1];
                    b[4] = (unsigned char)((n[960 + (by >> 1) * 8 + (bx >> 1)] >>
                                            (((by & 1) << 2) | ((bx & 1) << 1))) & 3);
                    for (k = 0; k < n_blk; ++k)
                        if (!memcmp(blk[k], b, 5)) break;
                    if (k == n_blk) { memcpy(blk[n_blk], b, 5); uses[n_blk] = 0; ++n_blk; }
                    ++uses[k];
                    blk_of[sx][by][bx] = (unsigned short)k;
                }
            scr_new_blocks[sx] = n_blk - before;
        }
        for (k = 0; k < n_blk; ++k)
            if (uses[k] == 1) ++once;
        level_blocks = n_blk;

        if (n_blk > MAX_BLOCKS) {
            /* a copy of the picture with the blocks used only once in red */
            static unsigned char out[SCREEN_H][MAX_W][3];
            for (y = 0; y < SCREEN_H; ++y)
                for (x = 0; x < used_scr * SCREEN_W; ++x) {
                    unsigned c = im.rgb[im.px[(size_t)y * (size_t)im.w + (size_t)x]];
                    unsigned r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255;
                    if (uses[blk_of[x / SCREEN_W][y >> 4][(x & 255) >> 4]] == 1) {
                        r = (r + 3 * 255) / 4; g = g / 3; b = b / 3;
                    }
                    out[y][x][0] = (unsigned char)r;
                    out[y][x][1] = (unsigned char)g;
                    out[y][x][2] = (unsigned char)b;
                }
            snprintf(fpath, sizeof(fpath), "%s/%s_blocks.bmp", out_dir, name);
            write_bmp24(fpath, used_scr * SCREEN_W, SCREEN_H, &out[0][0][0], MAX_W);

            fprintf(stderr, "bmp2level: %s has %d different 16x16 blocks; a level can "
                    "have %d.\n  New blocks per screen:", path, n_blk, MAX_BLOCKS);
            for (sx = 0; sx < used_scr; ++sx) fprintf(stderr, " %d", scr_new_blocks[sx]);
            fprintf(stderr, "\n  %d blocks are used only once; %s shows them in red.\n"
                    "  Reuse more of the same 16x16 pieces (on the 16x16 grid) there.\n",
                    once, fpath);
            exit(1);
        }
    }

    pal_size = read_file(strcat(strcpy(fpath, base), ".pal"), pal, 16);
    if (pal_size != 16)
        die("%s: missing or not 16 bytes. Save the level's palette from NEXXT "
            "(Palettes > Save palettes).", fpath);
    snprintf(fpath, sizeof(fpath), "%s/%s.pal", out_dir, name);
    write_file(fpath, pal, 16);

    for (sx = 0; sx < used_scr; ++sx) {
        snprintf(fpath, sizeof(fpath), "%s/%s_%d.nam", out_dir, name, sx);
        write_file(fpath, nam[sx], 1024);
    }

    printf("%s: %d screen%s", path, used_scr, used_scr == 1 ? "" : "s");
    if (used_scr < n_scr) printf(" (%d empty one%s at the end left out)",
                                 n_scr - used_scr, n_scr - used_scr == 1 ? "" : "s");
    printf(", %d of %d tiles, %d of %d blocks\n", n_tiles, LEVEL_TILES, level_blocks, MAX_BLOCKS);
    if (hidden)
        printf("  note: there's art in the top %d pixels, which are under the status "
               "bar and left out\n", HUD_H);

    /* its tile sheet: its tiles, then the font */
    {
        unsigned char *chr = chr_banks[n_levels];
        FILE *coll;
        memset(chr, 0, 4096);
        for (i = 0; i < n_tiles; ++i) memcpy(chr + i * 16, tiles[i].chr, 16);
        memcpy(chr + LEVEL_TILES * 16, font_chr + LEVEL_TILES * 16, 4096 - LEVEL_TILES * 16);

        /* and which of them are solid, for nam2level */
        snprintf(fpath, sizeof(fpath), "%s/%s_collision.txt", out_dir, name);
        coll = fopen(fpath, "w");
        if (!coll) die("%s: can't write", fpath);
        fprintf(coll, "# made by tools/bmp2level.c from %s\n", cpath);
        for (i = 0; i < n_tiles; ++i)
            if (tiles[i].solid) fprintf(coll, "solid %02X\n", i);
        fclose(coll);
    }
    ++n_levels;
}

int main(int argc, char **argv) {
    const char *font = 0, *chr_out = 0, *chr_more = 0, *header = 0, *out_dir = 0;
    char path[1100];
    FILE *f;
    int i;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-font") && i + 1 < argc) font = argv[++i];
        else if (!strcmp(argv[i], "-chr") && i + 1 < argc) chr_out = argv[++i];
        else if (!strcmp(argv[i], "-chr-more") && i + 1 < argc) chr_more = argv[++i];
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) header = argv[++i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out_dir = argv[++i];
    }
    if (!font || !chr_out || !chr_more || !header || !out_dir)
        die("usage: bmp2level -font chr/bg.chr -chr build/bg.chr -chr-more build/bg_more.chr "
            "-h build/level_chr.h -o build/lvl levels/NAME.bmp ...");
    if (read_file(font, font_chr, 4096) != 4096) die("%s: expected a 4096-byte tile sheet", font);

    /* nam2level wants a collision list; each level has its own as well */
    snprintf(path, sizeof(path), "%s/collision.txt", out_dir);
    f = fopen(path, "w");
    if (!f) die("%s: can't write (does the folder exist?)", path);
    fprintf(f, "# each level's solid tiles are in NAME_collision.txt\n");
    fclose(f);

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-font") || !strcmp(argv[i], "-chr") || !strcmp(argv[i], "-chr-more")
            || !strcmp(argv[i], "-h") || !strcmp(argv[i], "-o")) {
            ++i;
            continue;
        }
        {   /* NAME_collision.bmp is not a level of its own */
            size_t len = strlen(argv[i]);
            if (len >= 14 && !strcmp(argv[i] + len - 14, "_collision.bmp")) continue;
        }
        convert_level(argv[i], out_dir);
    }
    if (!n_levels) die("no levels/*.bmp given");

    /* the first level's tiles are CHR bank 0; the rest go from bank 3 */
    write_file(chr_out, chr_banks[0], 4096);
    f = fopen(chr_more, "wb");
    if (!f) die("%s: can't write", chr_more);
    for (i = 1; i < n_levels; ++i) fwrite(chr_banks[i], 1, 4096, f);
    fclose(f);

    f = fopen(header, "w");
    if (!f) die("%s: can't write", header);
    fprintf(f, "/* Made by tools/bmp2level.c: the CHR bank of each level's background\n"
               "   tiles (for mmc1_set_chr0). Do not edit. */\n"
               "#ifndef LEVEL_CHR_H\n#define LEVEL_CHR_H\n\n");
    for (i = 0; i < n_levels; ++i)
        fprintf(f, "#define CHR_%s %d\n", level_names[i], i == 0 ? 0 : 2 + i);
    fprintf(f, "\n#endif\n");
    fclose(f);
    return 0;
}