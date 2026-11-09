#include "boss.h"
#include "level.h"
#include "player.h"
#include "hud.h"
#include "audio.h"
#include "enemy_sprites.h"

#define BOSS_BANK 3

u8 boss_phase;
u16 boss_arena_x;

#pragma code-name(push, "BANK3")
#pragma rodata-name(push, "BANK3")

#define FRAME_32x32(t) \
    -16, -32, (t) + 0x0, 0,  -8, -32, (t) + 0x2, 0, \
      0, -32, (t) + 0x4, 0,   8, -32, (t) + 0x6, 0, \
    -16, -16, (t) + 0x8, 0,  -8, -16, (t) + 0xA, 0, \
      0, -16, (t) + 0xC, 0,   8, -16, (t) + 0xE, 0, \
    MSPR_END

static const u8 phantom1[] = { FRAME_32x32(0xD0) };
static const u8 phantom2[] = { FRAME_32x32(0xE0) };

#define BOSS_PAL      2
#define BOSS_HP       16
#define BOSS_DAMAGE   3
#define HALF_W        12
#define HEIGHT        24
#define INVULN        24

#define ARENA_START   48
#define HOME_Y        80
#define LEFT_X        40
#define RIGHT_X       216
#define HOVER_TIME    120
#define HOVER_FAST    70
#define WARN_TIME     30
#define SWOOP_SPEED   4
#define RISE_SPEED    2
#define REST_TIME     60

#define REST_FAST     40
#define REST_HURTS    0
#define DYING_TIME    96
#define BOSS_POINTS   3000

#define B_WAIT    0
#define B_PAN     1
#define B_APPEAR  2
#define B_HOVER   3
#define B_WARN    4
#define B_SWOOP   5
#define B_REST    6 
#define B_RISE    7
#define B_DYING   8
#define B_GONE    9

static u8 b_type, b_state, b_timer, b_hp, b_inv, b_left;
static u16 b_x, b_y;
static s16 b_vx, b_vy;
static u8 tx, ty;

#define BX ((u8)(b_x >> 8))
#define BY ((u8)(b_y >> 8))

static const s8 bob[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };

static const s8 flame_dx[4] = { -8, 8, -4, 6 };
static const s8 flame_dy[4] = { 0, -10, -18, -4 };

static void __fastcall__ fly_to(u8 speed) {
    static s16 dx, dy;
    static u8 adx, ady, frames;
    dx = (s16)tx - BX;
    dy = (s16)ty - BY;
    adx = (u8)(dx < 0 ? -dx : dx);
    ady = (u8)(dy < 0 ? -dy : dy);
    frames = (adx > ady ? adx : ady) / speed;
    if (frames < 4) frames = 4;
    b_vx = (dx * 64 / (s16)frames) << 2;
    b_vy = (dy * 64 / (s16)frames) << 2;
    b_timer = frames;
}

static void banked_load(u8 type) {
    b_type = type;
    boss_phase = BP_OFF;
    boss_arena_x = level_width - 256;
    b_state = type ? B_WAIT : B_GONE;
    b_hp = BOSS_HP;
    b_inv = 0;
    b_x = (u16)200 << 8;
    b_y = (u16)8 << 8;
}

static void banked_update(void) {
    static u16 px, wx;
    static u8 py;

    if (b_state == B_GONE) return;
    if (b_inv) --b_inv;

    switch (b_state) {
    case B_WAIT:
        if (player_x + PLAYER_WIDTH / 2 >= boss_arena_x + ARENA_START) {
            boss_phase = BP_FIGHT;
            music_play(SONG_BOSS);
            b_state = B_PAN;
        }
        return;

    case B_PAN:
        if (camera_x == boss_arena_x) {
            b_state = B_APPEAR;
            tx = 200;
            ty = HOME_Y;
            fly_to(1);
        }
        return;

    case B_APPEAR:
    case B_RISE:
        b_x += b_vx;
        b_y += b_vy;
        if (--b_timer == 0) {
            b_state = B_HOVER;
            b_timer = b_hp > BOSS_HP / 2 ? HOVER_TIME : HOVER_FAST;
        }
        break;

    case B_HOVER:
        if (b_left) {
            if (BX <= LEFT_X) b_left = 0;
            else b_x -= 0x100;
        } else {
            if (BX >= RIGHT_X) b_left = 1;
            else b_x += 0x100;
        }
        b_y = (u16)(HOME_Y + bob[(b_timer >> 3) & 7]) << 8;
        if (--b_timer == 0) {
            b_state = B_WARN;
            b_timer = WARN_TIME;
        }
        break;

    case B_WARN:
        if (--b_timer == 0) {
            px = player_x + PLAYER_WIDTH / 2 - boss_arena_x;
            tx = px > 248 ? 248 : (u8)px;
            ty = (u8)(player_y >> 8) + 24;
            while (ty < 232 && !level_solid_at(boss_arena_x + tx, ty)) ty += 4;
            ty &= 0xF8;
            b_state = B_SWOOP;
            fly_to(SWOOP_SPEED);
            sfx_play(SFX_SWING, SFX_WORLD);
        }
        break;

    case B_SWOOP:
        b_x += b_vx;
        b_y += b_vy;
        if (--b_timer == 0) {
            b_y = (u16)ty << 8;             /* exactly on the ground */
            b_state = B_REST;
            b_timer = b_hp > BOSS_HP / 2 ? REST_TIME : REST_FAST;
        }
        break;

    case B_REST:
        if (--b_timer == 0) {
            tx = BX < 128 ? RIGHT_X - 24 : LEFT_X + 24;
            ty = HOME_Y;
            b_state = B_RISE;
            fly_to(RISE_SPEED);
        }
        break;

    case B_DYING:
        if (--b_timer == 0) {
            b_state = B_GONE;
            boss_phase = BP_DONE;
        }
        return;

    }

    wx = boss_arena_x + BX;
    py = BY;
    if (!player_invuln && player_state != PS_HURT && (REST_HURTS || b_state != B_REST)
        && player_x + HB_RIGHT >= wx - HALF_W && player_x + HB_LEFT <= wx + HALF_W - 1
        && (u8)(player_y >> 8) + HB_BOTTOM >= py - HEIGHT && (u8)(player_y >> 8) + HB_TOP < py) {
        player_hurt(BOSS_DAMAGE, wx > player_x + PLAYER_WIDTH / 2);
    }

    if (player_attack_hit && !b_inv && b_state != B_APPEAR
        && player_hit_x2 >= wx - HALF_W && player_hit_x1 <= wx + HALF_W - 1
        && player_hit_y2 >= py - HEIGHT && player_hit_y1 < py) {
        b_inv = INVULN;
        if (--b_hp == 0) {
            b_state = B_DYING;
            b_timer = DYING_TIME;
            music_stop();
            sfx_play(SFX_ENEMY_DIE, SFX_WORLD);
            hud_add_score(BOSS_POINTS);
        } else {
            sfx_play(SFX_HIT, SFX_WORLD);
        }
    }
    hud_set_enemy_hp(b_hp);
}

static void banked_draw(void) {
    static s16 sx;
    static u8 y, attr, i;

    if (b_state == B_GONE || b_state <= B_PAN) return;
    sx = (s16)(boss_arena_x + BX - camera_x);
    y = BY;

    if (b_state == B_DYING) {
        i = (b_timer >> 2) & 3;
        oam_meta_spr(sx + flame_dx[i], y + flame_dy[i], (b_timer & 4) ? flame2 : flame1, 3);
        i = (i + 2) & 3;
        oam_meta_spr(sx + flame_dx[i], y + flame_dy[i], (b_timer & 4) ? flame3 : flame2, 3);
        if (b_timer < 48 && (b_timer & 2)) return;
    } else if (b_inv && (b_inv & 2)) {
        return;
    }

    if (b_state == B_WARN && (b_timer & 2)) ++sx;
    attr = BOSS_PAL;
    if (player_x + PLAYER_WIDTH / 2 > boss_arena_x + BX) attr |= SPR_FLIP_H;
    oam_meta_spr(sx, y, (b_state == B_REST || (frame_count & 8)) ? phantom2 : phantom1, attr);
}

#pragma rodata-name(pop)
#pragma code-name(pop)

static u8 old_bank;

void __fastcall__ boss_load(u8 type) {
    old_bank = prg_bank;
    mmc1_set_prg_bank(BOSS_BANK);
    banked_load(type);
    mmc1_set_prg_bank(old_bank);
}

void boss_update(void) {
    old_bank = prg_bank;
    mmc1_set_prg_bank(BOSS_BANK);
    banked_update();
    mmc1_set_prg_bank(old_bank);
}

void boss_draw(void) {
    old_bank = prg_bank;
    mmc1_set_prg_bank(BOSS_BANK);
    banked_draw();
    mmc1_set_prg_bank(old_bank);
}