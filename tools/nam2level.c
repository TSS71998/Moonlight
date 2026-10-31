/* nam2level.c - converts screens drawn in NEXXT into level data for the game.
 *
 * Built and run by the Makefile with your normal C compiler (gcc/clang/cc),
 * not cc65: it runs on your computer at build time, not on the NES.
 *
 * Input, for each screen:
 *   levels/NAME.nam   the screen, saved from NEXXT's nametable editor as a
 *                     1024-byte nametable WITH attributes and WITHOUT RLE
 *   levels/NAME.pal   optional: the 16-byte background palette (Palettes >
 *                     Save palettes). Without it the game keeps its current one.
 * plus levels/collision.txt, which says which background tiles are solid.
 *
 * Output: build/levels.c and build/levels.h. Each screen becomes:
 *   - a set of metatiles: every distinct 16x16 block (4 tiles + its palette,
 *     and which of its four 8x8 tiles are solid)
 *   - a 16x15 room map of metatile numbers
 *   - the palette, if there is one
 * all stored in PRG bank LEVEL_BANK and loaded with level_load().
 *
 * Usage: nam2level -o build/levels -c levels/collision.txt levels/NAME.nam ...
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEVEL_BANK  0       /* PRG bank the data is stored in (segment BANK0) */
#define ROOM_W      16
#define ROOM_H      15
#define COLL_TL     0x10    /* must match level.h: bits 4-7 = solid quarters */
#define COLL_MASK   0xF0    /* must match level.h (collision in bits 4-7) */

typedef struct {
    unsigned char tiles[4]; /* TL, TR, BL, BR */
    unsigned char info;     /* palette | collision */
} Metatile;

static unsigned char solid[256];    /* 1 = this background tile is solid */

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

/* collision.txt: lines like "solid 01-08 0A 20-2F"; # starts a comment */
static void parse_collision(const char *path) {
    FILE *f = fopen(path, "r");
    char line[1024], *tok, *end;
    long lo, hi, t;
    int n = 0;

    if (!f) die("%s: can't open", path);
    while (fgets(line, sizeof(line), f)) {
        ++n;
        if ((end = strchr(line, '#')) != NULL) *end = 0;
        tok = strtok(line, " \t\r\n");
        if (!tok) continue;
        if (strcmp(tok, "solid") != 0)
            die("%s:%d: unknown keyword '%s' (expected 'solid')", path, n, tok);
        while ((tok = strtok(NULL, " \t\r\n")) != NULL) {
            lo = hi = strtol(tok, &end, 16);
            if (end == tok) die("%s:%d: '%s' is not a hex tile number", path, n, tok);
            if (*end == '-') {
                char *start = end + 1;
                hi = strtol(start, &end, 16);
                if (end == start) die("%s:%d: bad range '%s'", path, n, tok);
            }
            if (*end) die("%s:%d: bad tile number '%s'", path, n, tok);
            if (lo < 0 || hi > 255 || lo > hi)
                die("%s:%d: '%s' must be tiles 00-FF, low to high", path, n, tok);
            for (t = lo; t <= hi; ++t) solid[t] = 1;
        }
    }
    fclose(f);
}

/* Reads a whole file; returns its size, or -1 if it doesn't exist */
static long read_file(const char *path, unsigned char *buf, long max) {
    FILE *f = fopen(path, "rb");
    long size;
    if (!f) return -1;
    size = (long)fread(buf, 1, (size_t)max, f);
    fclose(f);
    return size;
}

/* "levels/test.nam" -> "test" (non-identifier characters become '_') */
static void screen_name(const char *path, char *name, size_t size) {
    const char *base = path, *p;
    size_t i = 0;
    for (p = path; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    if (isdigit((unsigned char)*base) && i + 1 < size) name[i++] = '_';
    for (p = base; *p && *p != '.' && i + 1 < size; ++p)
        name[i++] = (isalnum((unsigned char)*p) || *p == '_') ? *p : '_';
    name[i] = 0;
}

/* "levels/test.nam" -> "levels/test.pal" */
static void pal_path_for(const char *nam, char *out, size_t size) {
    const char *dot = strrchr(nam, '.'), *slash1 = strrchr(nam, '/'),
               *slash2 = strrchr(nam, '\\');
    size_t len = strlen(nam);
    if (dot && dot > slash1 && dot > slash2) len = (size_t)(dot - nam);
    snprintf(out, size, "%.*s.pal", (int)len, nam);
}

/* Prints values as C bytes, `per_line` per line: "    0x01, 0x02," */
static void c_bytes(FILE *f, const unsigned char *v, int count, int per_line) {
    int i;
    for (i = 0; i < count; ++i) {
        fprintf(f, "%s0x%02X%s", i % per_line == 0 ? "    " : ", ", v[i],
                (i % per_line == per_line - 1 || i == count - 1) ? ",\n" : "");
    }
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int main(int argc, char **argv) {
    const char *out = NULL, *coll = NULL;
    char **screens;
    int n_screens = 0, i;
    char path[1024];
    FILE *fc, *fh;

    screens = malloc(sizeof(char *) * (size_t)argc);
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-c") && i + 1 < argc) coll = argv[++i];
        else screens[n_screens++] = argv[i];
    }
    if (!out || !coll || n_screens == 0)
        die("usage: nam2level -o build/levels -c levels/collision.txt levels/*.nam");

    parse_collision(coll);
    qsort(screens, (size_t)n_screens, sizeof(char *), cmp_str);

    snprintf(path, sizeof(path), "%s.c", out);
    if (!(fc = fopen(path, "w"))) die("%s: can't write", path);
    snprintf(path, sizeof(path), "%s.h", out);
    if (!(fh = fopen(path, "w"))) die("%s: can't write", path);

    fprintf(fc, "/* Generated by tools/nam2level.c from levels/. Do not edit:\n"
                "   edit the .nam files in NEXXT and rebuild. */\n"
                "#include \"level.h\"\n#include \"levels.h\"\n\n"
                "#pragma rodata-name(push, \"BANK%d\")\n\n", LEVEL_BANK);
    fprintf(fh, "/* Generated by tools/nam2level.c. Do not edit. */\n"
                "#ifndef LEVELS_H\n#define LEVELS_H\n\n#include \"level.h\"\n\n"
                "#define LEVEL_BANK %d   /* PRG bank holding all level data */\n\n",
            LEVEL_BANK);

    for (i = 0; i < n_screens; ++i) {
        static unsigned char data[1025], pal[17];
        static Metatile mts[ROOM_W * ROOM_H];   /* at most one per cell */
        static unsigned char room[ROOM_W * ROOM_H], buf[4 * ROOM_W * ROOM_H];
        const char *nam = screens[i];
        char name[256], pal_path[1024];
        int n_mts = 0, n_solid = 0, mx, my, k, j;
        long size, pal_size;

        size = read_file(nam, data, sizeof(data));
        if (size < 0) die("%s: can't open", nam);
        if (size != 1024)
            die("%s: %ld bytes, expected 1024. In NEXXT, save the nametable "
                "with attributes and with RLE turned off.", nam, size);

        for (my = 0; my < ROOM_H; ++my) {
            for (mx = 0; mx < ROOM_W; ++mx) {
                const unsigned char *t = data, *attr = data + 960;
                int tx = mx * 2, ty = my * 2;
                Metatile m;
                m.tiles[0] = t[ty * 32 + tx];
                m.tiles[1] = t[ty * 32 + tx + 1];
                m.tiles[2] = t[(ty + 1) * 32 + tx];
                m.tiles[3] = t[(ty + 1) * 32 + tx + 1];
                m.info = (attr[(my >> 1) * 8 + (mx >> 1)]
                          >> (((my & 1) << 2) | ((mx & 1) << 1))) & 3;
                for (k = 0; k < 4; ++k)
                    if (solid[m.tiles[k]]) m.info |= (unsigned char)(COLL_TL << k);

                for (j = 0; j < n_mts; ++j)
                    if (!memcmp(&mts[j], &m, sizeof(m))) break;
                if (j == n_mts) mts[n_mts++] = m;
                room[my * ROOM_W + mx] = (unsigned char)j;
            }
        }

        pal_path_for(nam, pal_path, sizeof(pal_path));
        pal_size = read_file(pal_path, pal, 16);
        if (pal_size >= 0 && pal_size < 16)
            die("%s: expected a 16-byte background palette", pal_path);

        screen_name(nam, name, sizeof(name));
        for (j = 0; j < n_mts; ++j)
            if (mts[j].info & COLL_MASK) ++n_solid;
        if (pal_size >= 0)
            printf("%s: %d metatiles (%d with solid tiles), palette from %s\n", nam, n_mts, n_solid, pal_path);
        else
            printf("%s: %d metatiles (%d with solid tiles), palette unchanged\n", nam, n_mts, n_solid);

        fprintf(fc, "/* %s */\n", nam);
        fprintf(fc, "static const u8 %s_mt_tiles[] = {  /* TL, TR, BL, BR */\n", name);
        for (j = 0; j < n_mts; ++j) memcpy(buf + j * 4, mts[j].tiles, 4);
        c_bytes(fc, buf, n_mts * 4, 16);
        fprintf(fc, "};\n");
        fprintf(fc, "static const u8 %s_mt_info[] = {  /* palette | collision */\n", name);
        for (j = 0; j < n_mts; ++j) buf[j] = mts[j].info;
        c_bytes(fc, buf, n_mts, 16);
        fprintf(fc, "};\n");
        fprintf(fc, "static const u8 %s_room[ROOM_W * ROOM_H] = {\n", name);
        c_bytes(fc, room, ROOM_W * ROOM_H, 16);
        fprintf(fc, "};\n");
        if (pal_size >= 0) {
            fprintf(fc, "static const u8 %s_pal[16] = {\n", name);
            c_bytes(fc, pal, 16, 16);
            fprintf(fc, "};\n");
        }
        fprintf(fc, "const LevelDef lvl_%s = {\n", name);
        if (pal_size >= 0)
            fprintf(fc, "    %s_mt_tiles, %s_mt_info, %s_room, %s_pal, %d\n", name, name, name, name, n_mts);
        else
            fprintf(fc, "    %s_mt_tiles, %s_mt_info, %s_room, 0, %d\n", name, name, name, n_mts);
        fprintf(fc, "};\n\n");
        fprintf(fh, "extern const LevelDef lvl_%s;   /* from %s */\n", name, nam);
    }

    fprintf(fc, "#pragma rodata-name(pop)\n");
    fprintf(fh, "\n#endif\n");
    fclose(fc);
    fclose(fh);
    free(screens);
    return 0;
}