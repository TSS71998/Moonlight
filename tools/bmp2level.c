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
static int n_tiles;

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
    int n_scr, used_scr, sx, bx, by, x, y, i, hidden = 0, first_tile = n_tiles;
    long pal_size;

    split_path(path, base, sizeof(base), name, sizeof(name));
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
    printf(", %d new tiles\n", n_tiles - first_tile);
    if (hidden)
        printf("  note: there's art in the top %d pixels, which are under the status "
               "bar and left out\n", HUD_H);
}

int main(int argc, char **argv) {
    const char *font = 0, *chr_out = 0, *out_dir = 0;
    static unsigned char chr[4096], font_chr[4096];
    char path[1100];
    FILE *coll;
    int i, n_levels = 0, n_solid = 0;
    Tile blank;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-font") && i + 1 < argc) font = argv[++i];
        else if (!strcmp(argv[i], "-chr") && i + 1 < argc) chr_out = argv[++i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out_dir = argv[++i];
    }
    if (!font || !chr_out || !out_dir)
        die("usage: bmp2level -font chr/bg.chr -chr build/bg.chr -o build/lvl levels/NAME.bmp ...");
    if (read_file(font, font_chr, 4096) != 4096) die("%s: expected a 4096-byte tile sheet", font);

    memset(&blank, 0, sizeof(blank));
    add_tile(&blank);                   /* tile $00: empty, not solid */

    snprintf(path, sizeof(path), "%s/collision.txt", out_dir);
    coll = fopen(path, "w");
    if (!coll) die("%s: can't write (does the folder exist?)", path);

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-font") || !strcmp(argv[i], "-chr") || !strcmp(argv[i], "-o")) {
            ++i;
            continue;
        }
        {   /* NAME_collision.bmp is not a level of its own */
            size_t len = strlen(argv[i]);
            if (len >= 14 && !strcmp(argv[i] + len - 14, "_collision.bmp")) continue;
        }
        convert_level(argv[i], out_dir);
        ++n_levels;
    }
    if (!n_levels) die("no levels/*.bmp given");

    if (n_tiles > LEVEL_TILES)
        die("the levels need %d different 8x8 tiles; only %d fit (tiles $C0-$FF "
            "are the status bar font). Reuse more of the same pieces.",
            n_tiles, LEVEL_TILES);

    /* the tile sheet: level tiles, then the font */
    memset(chr, 0, sizeof(chr));
    for (i = 0; i < n_tiles; ++i) memcpy(chr + i * 16, tiles[i].chr, 16);
    memcpy(chr + LEVEL_TILES * 16, font_chr + LEVEL_TILES * 16, 4096 - LEVEL_TILES * 16);
    write_file(chr_out, chr, 4096);

    fprintf(coll, "# made by tools/bmp2level.c from levels/*_collision.bmp\n");
    for (i = 0; i < n_tiles; ++i)
        if (tiles[i].solid) {
            fprintf(coll, "solid %02X\n", i);
            ++n_solid;
        }
    fclose(coll);

    printf("background tiles: %d of %d used (%d solid)\n", n_tiles, LEVEL_TILES, n_solid);
    return 0;
}