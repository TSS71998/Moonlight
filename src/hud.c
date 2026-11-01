#include "hud.h"

#define SCORE_X  2
#define SCORE_Y  1
#define BAR_X    9
#define PLAYER_Y 2
#define ENEMY_Y  3

u8 hud_player_hp = HUD_BAR_LEN;
u8 hud_enemy_hp = HUD_BAR_LEN;

static u8 score[6];
static u8 dirty;
#define DIRTY_SCORE  1
#define DIRTY_PLAYER 2
#define DIRTY_ENEMY  4

static u8 buf[HUD_BAR_LEN];

static u8 __fastcall__ char_tile(char c) {
    if (c >= '0' && c <= '9') return HUD_TILE_0 + (c - '0');
    if (c >= 'A' && c <= 'Z') return HUD_TILE_A + (c - 'A');
    if (c == '-') return HUD_TILE_DASH;
    return HUD_TILE_BLANK;
}

static void __fastcall__ put_text(u16 adr, const char *s) {
    vram_adr(adr);
    while (*s) vram_put(char_tile(*s++));
}

static void score_tiles(void) {
    static u8 i;
    for (i = 0; i < 6; ++i) buf[i] = HUD_TILE_0 + score[i];
}

static void __fastcall__ bar_tiles(u8 hp, u8 full) {
    static u8 i;
    for (i = 0; i < HUD_BAR_LEN; ++i) buf[i] = i < hp ? full : HUD_TILE_EMPTY;
}

void hud_init(void) {
    vram_adr(NTADR_A(0, 0));
    vram_fill(HUD_TILE_BLANK, HUD_ROWS * 32);
    vram_adr(0x23C0);
    vram_fill(HUD_PAL * 0x55, 8);

    put_text(NTADR_A(SCORE_X, SCORE_Y), "SCORE-");
    put_text(NTADR_A(SCORE_X, PLAYER_Y), "PLAYER");
    put_text(NTADR_A(SCORE_X, ENEMY_Y), "ENEMY");
    vram_adr(NTADR_A(SCORE_X, ENEMY_Y));
    vram_put(HUD_TILE_SPLIT);

    score_tiles();
    vram_adr(NTADR_A(SCORE_X + 6, SCORE_Y));
    vram_write(buf, 6);
    bar_tiles(hud_player_hp, HUD_TILE_PLAYER);
    vram_adr(NTADR_A(BAR_X, PLAYER_Y));
    vram_write(buf, HUD_BAR_LEN);
    bar_tiles(hud_enemy_hp, HUD_TILE_ENEMY);
    vram_adr(NTADR_A(BAR_X, ENEMY_Y));
    vram_write(buf, HUD_BAR_LEN);

    dirty = 0;
    oam_top = HUD_HEIGHTS;
    hud_split = 1;
}

void __fastcall__ hud_add_score(u16 points) {
    static const u16 places[5] = { 10000, 1000, 100, 10, 1 };
    static u8 add[6], i, d;

    add[0] = 0;
    for (i = 0; i < 5; ++i) {
        d = 0;
        while (points >= places[i]) { points -= places[i]; ++d; }
        add[i + 1] = d;
    }

    d = 0;
    i = 6;

    while (i) {
        --i;
        d += score[i] + add[i];
        if (d >= 10) { score[i] = d - 10; d = 1; }
        else         { score[i] = d     ; d = 0; } 
    }
    if (d) {
        for (i = 0; i < 5; ++i) score[i] = 9;
    }
    dirty |= DIRTY_SCORE;
}

void __fastcall__ hud_set_player_hp(u8 hp) {
    if (hp > HUD_BAR_LEN) hp = HUD_BAR_LEN;
    if (hp != hud_player_hp) { hud_player_hp = hp; dirty |= DIRTY_PLAYER; }
}

void __fastcall__ hud_set_enemy_hp(u8 hp) {
    if (hp > HUD_BAR_LEN) hp = HUD_BAR_LEN;
    if (hp != hud_enemy_hp) { hud_enemy_hp = hp; dirty |= DIRTY_ENEMY; }
}

void hud_update(void) {
    if (!dirty || vram_buf_len) return;
    if (dirty & DIRTY_SCORE) {
        score_tiles();
        vram_buf_put(NTADR_A(SCORE_X + 6, SCORE_Y), buf, 6);
    }
    if (dirty & DIRTY_PLAYER) {
        bar_tiles(hud_player_hp, HUD_TILE_PLAYER);
        vram_buf_put(NTADR_A(BAR_X, PLAYER_Y), buf, HUD_BAR_LEN);
    }
    if (dirty & DIRTY_ENEMY) {
        bar_tiles(hud_enemy_hp, HUD_TILE_ENEMY);
        vram_buf_put(NTADR_A(BAR_X, ENEMY_Y), buf, HUD_BAR_LEN);
    }
    dirty = 0;
}

void hud_sprite0(void) {
    oam_spr(SCORE_X * 8, HUD_HEIGHTS - 1, HUD_SPRITE0_TILE, SPR_BEHIND_BG);
}