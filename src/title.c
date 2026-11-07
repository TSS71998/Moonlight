#include "title.h"
#include "hud.h"
#include "audio.h"

extern const u8 title_nam[1024];
extern const u8 title_pal[16];

static const u8 title_sprite_pal[16] = {
    0x0F, 0x05, 0x16, 0x27,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x2D, 0x04, 0x16,
    0x0F, 0x16, 0x27, 0x37,
};

#define TITLE_EMPHASIS 0x00

#define START_BUTTONS (PAD_START | PAD_SELECT)

#define FADE_STEPS 4
#define FADE_DELAY 4

#define ROSE_PAL        2
#define ROSE_HOLD       60
#define ROSE_FADE_DELAY 8

#define ROSE_STEPS 5
static const u8 rose_ramp[ROSE_STEPS][3] = {
    { 0x05, 0x16, 0x27 },
    { 0x05, 0x16, 0x27 },
    { 0x06, 0x05, 0x16 },
    { 0x07, 0x06, 0x05 },
    { 0x08, 0x07, 0x06 },
};
static const u8 rose_emph[ROSE_STEPS] = { 0x00, 0xC0, 0xC0, 0xC0, 0xE0 };

#define PETAL_MAX      6
#define PETAL_TILE     0xA0
#define PETAL_FRAMES   4
#define PETAL_ANIM     12
#define PETAL_PAL      0
#define PETAL_BEHIND   1
#define PETAL_GAP_MIN  30
#define PETAL_GAP_RAND 63
#define PETAL_SPEED    0x60
#define PETAL_SPEED_RAND 0x3F

#if PETAL_BEHIND
#define PETAL_ATTR (PETAL_PAL | SPR_BEHIND_BG)
#else
#define PETAL_ATTR PETAL_PAL
#endif

static u8 full_pal[32];

static u8 p_on[PETAL_MAX];
static u8 p_x[PETAL_MAX];
static u8 p_y[PETAL_MAX];
static u8 p_sub[PETAL_MAX];
static u8 p_speed[PETAL_MAX];
static u8 p_phase[PETAL_MAX];
static u8 p_anim[PETAL_MAX];
static u8 p_frame[PETAL_MAX];
static u8 petal_timer;
static u8 rnd_seed;


static const s8 sway[32] = {
     0,  1,  2,  3,  4,  5,  6,  6,  6,  6,  6,  5,  4,  3,  2,  1,
     0, -1, -2, -3, -4, -5, -6, -6, -6, -6, -6, -5, -4, -3, -2, -1,
};

static u8 rnd(void) {
    static u8 c;
    c = rnd_seed & 0x80;
    rnd_seed <<= 1;
    if (c) rnd_seed ^= 0x1D;
    return rnd_seed;
}

static void petals_clear(void) {
    static u8 i;
    for (i = 0; i < PETAL_MAX; ++i) p_on[i] = 0;
    petal_timer = 20;
    if (!rnd_seed) rnd_seed = 0x5A;
}

static void petal_spawn(void) {
    static u8 i, x;
    for (i = 0; i < PETAL_MAX; ++i) {
        if (!p_on[i]) break;
    }
    if (i == PETAL_MAX) return;
    x = rnd();
    if (x < 8) x += 8;
    else if (x > 240) x -= 16;
    p_on[i] = 1;
    p_x[i] = x;
    p_y[i] = 1;
    p_sub[i] = 0;
    p_speed[i] = PETAL_SPEED + (rnd() & PETAL_SPEED_RAND);
    p_phase[i] = rnd();
    p_frame[i] = rnd() & (PETAL_FRAMES - 1);
    p_anim[i] = PETAL_ANIM;
}

static void petals_update(void) {
    static u8 i, idx, attr, sum;

    if (--petal_timer == 0) {
        petal_spawn();
        petal_timer = PETAL_GAP_MIN + (rnd() & PETAL_GAP_RAND);
    }

    oam_clear();
    for (i = 0; i < PETAL_MAX; ++i) {
        if (!p_on[i]) continue;

        sum = p_sub[i] + p_speed[i];
        if (sum < p_sub[i]) ++p_y[i];
        p_sub[i] = sum;
        if (p_y[i] >= 240) {
            p_on[i] = 0;
            continue;
        }

        if (--p_anim[i] == 0) {
            p_anim[i] = PETAL_ANIM;
            p_frame[i] = (p_frame[i] + 1) & (PETAL_FRAMES - 1);
        }

        ++p_phase[i];
        idx = (p_phase[i] >> 1) & 31;
        attr = PETAL_ATTR;
        if ((u8)(idx - 8) < 16) attr |= SPR_FLIP_H;

        oam_spr(p_x[i] + (u8)sway[idx], p_y[i],
                PETAL_TILE + (p_frame[i] << 1) + 1,
                attr);
    }
}

static void title_frame(void) {
    petals_update();
    ppu_wait_nmi();
}

static void __fastcall__ set_fade(u8 level, u8 rose_level) {
    static u8 i, c, k, lv;
    for (i = 0; i < 32; ++i) {
        c = full_pal[i];
        lv = (i >= ROSE_PAL * 4 + 1 && i <= ROSE_PAL * 4 + 3) ? rose_level : level;
        for (k = lv; k < FADE_STEPS; ++k) {
            c = c >= 0x10 ? c - 0x10 : 0x0F;
        }
        pal_buf[i] = c;
    }
    pal_update = 1;
}

static void __fastcall__ wait_frames(u8 n) {
    while (n--) title_frame();
}

void title_screen(void) {
    static u8 old_bank, i, pad, pad_prev;

    ppu_off();

    hud_split = 0;
    oam_top = 0;
    oam_clear();
    petals_clear();
    scroll_x = 0;
    scroll_y = 0;
    ppu_ctrl &= ~(CTRL_NT_2400 | 0x02);

    mmc1_set_chr0(TITLE_CHR_BANK);
    old_bank = prg_bank;
    mmc1_set_prg_bank(TITLE_BANK);
    vram_adr(NTADR_A(0,0));
    vram_write(title_nam, 1024);
    for (i = 0; i < 16; ++i) full_pal[i] = title_pal[i];
    mmc1_set_prg_bank(old_bank);
    for (i = 0; i < 16; ++i) full_pal[16 + i] = title_sprite_pal[i];

    set_fade(0, 0);
    music_play(SONG_TITLE);
    ppu_on();
    ppu_mask |= TITLE_EMPHASIS;
    for (i = 1; i <= FADE_STEPS; ++i) {
        wait_frames(FADE_DELAY);
        set_fade(i, i);
    }

    pad_prev = 0xFF;
    for (;;) {
        pad = pad_poll(0);
        if (pad & ~pad_prev & START_BUTTONS) break;    
        pad_prev = pad;
        title_frame();
    }

    music_stop();
    sfx_play(SFX_START, SFX_WORLD);
    wait_frames(30);

    for (i = FADE_STEPS; i; --i) {
        set_fade(i - 1, FADE_STEPS);
        wait_frames(FADE_DELAY);
    }

    wait_frames(ROSE_HOLD);
    for (i = 1; i < ROSE_STEPS; ++i) {
        pal_buf[ROSE_PAL * 4 + 1] = rose_ramp[i][0];
        pal_buf[ROSE_PAL * 4 + 2] = rose_ramp[i][1];
        pal_buf[ROSE_PAL * 4 + 3] = rose_ramp[i][2];
        pal_update = 1;
        ppu_mask = (ppu_mask & 0x1F) | rose_emph[i];
        wait_frames(ROSE_FADE_DELAY);
    }
    set_fade(0, 0);
    ppu_mask &= 0x1F;
    wait_frames(ROSE_FADE_DELAY);

    audio_silence();
    oam_clear();
    ppu_off();
    mmc1_set_chr0(0);
}