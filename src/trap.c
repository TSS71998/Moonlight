#include "trap.h"
#include "level.h"
#include "player.h"
#include "audio.h"

#define HI(v) ((u8)((v) >> 8))

#define FRAME_16x16(t) \
    -8, -16, (t) + 0, 0, \
     0, -16, (t) + 2, 0, \
    MSPR_END

static const u8 spike_tip[]  = { FRAME_16x16(0xC0) };
static const u8 spike_half[] = { FRAME_16x16(0xC4) };
static const u8 spike_full[] = {
    -8, -32, 0xC8, 0,
     0, -32, 0xCA, 0,
    -8, -16, 0xCC, 0,
     0, -16, 0xCE, 0,
    MSPR_END
};

#define TRAP_PAL      1
#define TRAP_DAMAGE   4
#define WARN_TIME     30
#define RISE_TIME     4
#define FALL_TIME     8
#define HALF_W        5
#define HALF_H        14
#define FULL_H        30

#define T_START 0
#define T_DOWN  1
#define T_RISE  2
#define T_UP    3
#define T_FALL  4

static const TrapSpawn *tr_list, *lo_p, *hi_p;
static u8  n_tr;
static u8  tr_state[MAX_TRAP_SPAWNS];
static u8  tr_timer[MAX_TRAP_SPAWNS];
static u8  win_lo, win_hi;

void __fastcall__ traps_load(const TrapSpawn *list) {
    tr_list = list;
    for (n_tr = 0; n_tr < MAX_TRAP_SPAWNS && (list->x | list->y); ++n_tr, ++list) {}
    win_lo = win_hi = 0;
    lo_p = hi_p = tr_list;
    traps_reset(); 
}

void traps_reset(void) {
    static u8 i;
    static const TrapSpawn *p;
    for (i = 0, p = tr_list; i < n_tr; ++i, ++p) {
        tr_state[i] = T_START;
        tr_timer[i] = p->start;
    }
}

static void update_window(void) {
    static u16 lo, hi;
    lo = camera_x > 16 ? camera_x - 16 : 0;
    hi = camera_x + 272;
    while (win_lo < n_tr && lo_p->x < lo) { ++win_lo; ++lo_p; }
    while (win_lo && lo_p[-1].x >= lo) { --win_lo; --lo_p; }
    if (win_hi < win_lo) { win_hi = win_lo; hi_p = lo_p; }
    while (win_hi < n_tr && hi_p->x < hi) { ++win_hi; ++hi_p; }
    while (win_hi > win_lo && hi_p[-1].x >= hi) { --win_hi; --hi_p; }
}

void traps_update(void) {
    static u8 s, st, top, d;
    static u16 x;
    static const TrapSpawn *p;

    update_window();
    for (s = win_lo, p = lo_p; s < win_hi; ++s, ++p) {
        if (tr_timer[s]) {
            --tr_timer[s];
        } else {
            switch (tr_state[s]) {
            case T_START:
            case T_FALL:
                tr_state[s] = T_DOWN;
                d = p->down;
                tr_timer[s] = (d > WARN_TIME ? d : WARN_TIME + 1) - 1;
                break;
            case T_DOWN:
                tr_state[s] = T_RISE;
                tr_timer[s] = RISE_TIME - 1;
                sfx_play(SFX_SPIKE, SFX_WORLD);
                break;
            case T_RISE:
                tr_state[s] = T_UP;
                d = p->up;
                tr_timer[s] = d ? d - 1 : 0;
                break;
            default:
                tr_state[s] = T_FALL;
                tr_timer[s] = FALL_TIME - 1;
                break;
            }
        }

        st = tr_state[s];
        if (st < T_RISE) continue;
        if (player_invuln || player_state == PS_HURT) continue;
        x = p->x;
        top = p->y - (st == T_UP ? FULL_H : HALF_H);
        if (player_x + HB_RIGHT >= x - HALF_W && player_x + HB_LEFT <= x + (HALF_W - 1)
            && HI(player_y) + HB_BOTTOM >= top && HI(player_y) + HB_TOP < p->y) {
            player_hurt(TRAP_DAMAGE, x > player_x + PLAYER_WIDTH / 2);
        }
    }
}

void traps_draw(void) {
    static u8 s, st;
    static s16 sx;
    static const u8 *frame;
    static const TrapSpawn *p;

    for (s = win_lo, p = lo_p; s < win_hi; ++s, ++p) {
        st = tr_state[s];
        if (st == T_UP) {
            frame = spike_full;
        } else if (st >= T_RISE) {
            frame = spike_half;
        } else if (st == T_DOWN && tr_timer[s] < WARN_TIME) {
            frame = spike_tip;
        } else {
            continue;
        }
        sx = (s16)(p->x - camera_x);
        if (frame == spike_tip && (frame_count & 2)) ++sx;     /* shakes */
        oam_meta_spr(sx, p->y, frame, TRAP_PAL);
    }
}