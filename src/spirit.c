#include "spirit.h"
#include "enemy_sprites.h"
#include "level.h"
#include "player.h"
#include "hud.h"
#include "audio.h"

#define HI(v) ((u8)((v) >> 8))

#define FRAME_16x16(t) \
    -8, -16, (t) + 0, 0, \
     0, -16, (t) + 2, 0, \
    MSPR_END
#define FRAME_8x16(t) \
    -4, -16, (t), 0, \
    MSPR_END

static const u8 spirit1[] = { FRAME_16x16(0xA8) };
static const u8 spirit2[] = { FRAME_16x16(0xAC) };
static const u8 item_points[]     = { FRAME_8x16(0xB0) };
static const u8 item_points_big[] = { FRAME_8x16(0xB2) };
static const u8 item_heal[]       = { FRAME_8x16(0xB4) };
static const u8 item_heal_big[]   = { FRAME_8x16(0xB6) };

#define SPIRIT_PAL    1
#define SPIRIT_HALF_W 6
#define SPIRIT_HEIGHT 14
#define SPIRIT_ANIM   16

typedef struct {
    const u8 *frame;
    u8 pal;
    u8 heal;
    u16 points;
} ItemType;

static const ItemType item_types[] = {
    { item_points,        2,   0,    100 },     /* ITEM_POINTS */
    { item_points_big,    2,   0,    500 },     /* ITEM_POINTS_BIG */
    { item_heal,          3,   2,      0 },     /* ITEM_HEAL */
    { item_heal_big,      3,   8,      0 },     /* ITEM_HEAL_BIG */
};

#define ITEM_LIFE      150
#define ITEM_BLINK     45
#define ITEM_MAX_FALL  12

#define BREAK_TIME     24
#define FLAME_PAL      3
static const u8 *const break_frames[3] = { flame3, flame2, flame1 };

static const s8 bob[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };

#define SP_WHOLE 0
#define SP_GONE  0xFF
static const SpiritSpawn *sp_list;
static u8  n_sp;
static u8  sp_state[MAX_SPIRIT_SPAWNS];
static u8  win_lo, win_hi;

static u8  it_type[MAX_ITEMS];
static u16 it_x[MAX_ITEMS];
static u8  it_y[MAX_ITEMS];
static u8  it_vy[MAX_ITEMS];
static u8  it_sub[MAX_ITEMS];
static u8  it_life[MAX_ITEMS];
static const u8 *it_frame[MAX_ITEMS];
static u8  it_pal[MAX_ITEMS]; 

void __fastcall__ spirits_load(const SpiritSpawn *list) {
    static u8 i;
    sp_list = list;
    for (n_sp = 0; n_sp < MAX_SPIRIT_SPAWNS && list->item != ITEM_NONE; ++n_sp, ++list) {}
    for (i = 0; i < n_sp; ++i) sp_state[i] = SP_WHOLE;
    win_lo = win_hi = 0;
    spirits_reset();
}

void spirits_reset(void) {
    static u8 i;
    for (i = 0; i < MAX_ITEMS; ++i) it_type[i] = ITEM_NONE;
    for (i = 0; i < n_sp; ++i) {
        if (sp_state[i] != SP_WHOLE) sp_state[i] = SP_GONE;
    }
}

static void __fastcall__ drop_item(const SpiritSpawn *p) {
    static u8 k;
    for (k = 0; k < MAX_ITEMS; ++k) {
        if (it_type[k] == ITEM_NONE) break;
    }
    if (k == MAX_ITEMS) k = 0;
    it_type[k] = p->item;
    it_frame[k] = item_types[p->item].frame;
    it_pal[k] = item_types[p->item].pal;
    it_x[k] = p->x;
    it_y[k] = p->y;
    it_vy[k] = 0;
    it_sub[k] = 0;
    it_life[k] = ITEM_LIFE;
}

static void __fastcall__ give(u8 type) {
    static const ItemType *t;
    t = &item_types[type];
    if (t->points) hud_add_score(t->points);
    if (t->heal) {
        player_hp = player_hp + t->heal >= PLAYER_MAX_HP ? PLAYER_MAX_HP : player_hp + t->heal;
    }
    sfx_play(SFX_PICKUP, SFX_WORLD);
}

static void items_update(void) {
    static u8 k, y, v;
    static u16 x;

    for (k = 0; k < MAX_ITEMS; ++k) {
        if (it_type[k] == ITEM_NONE) continue;
        x = it_x[k];
        y = it_y[k];

        if ((camera_x > 16 && x < camera_x - 16) || x > camera_x + 272) {
            it_type[k] = ITEM_NONE;
            continue;
        }

        v = it_vy[k];
        if (v != 0xFF) {
            if (level_solid_at(x - 3, y) || level_solid_at(x + 2, y)) {
                it_vy[k] = 0xFF;
            } else {
                if (v < ITEM_MAX_FALL) it_vy[k] = ++v;
                it_sub[k] += v;
                y += it_sub[k] >> 2;
                it_sub[k] &= 3;
                if (y >= 240) {
                    it_type[k] = ITEM_NONE;
                    continue;
                }
                if (level_solid_at(x - 3, y) || level_solid_at(x + 2, y)) {
                    y &= 0xF8;
                    it_vy[k] = 0xFF;
                }
                it_y[k] = y;
            }
        }

        if (player_x + HB_RIGHT >= x - 4 && player_x + HB_LEFT <= x + 3
            && HI(player_y) + HB_BOTTOM >= y - 12 && HI(player_y) + HB_TOP <= y - 1) {
            give(it_type[k]);
            it_type[k] = ITEM_NONE;
            continue;
        }

        if (frame_count & 1) {
            if (--it_life[k] == 0) it_type[k] = ITEM_NONE;
        }
    }
}

static void update_window(void) {
    static u16 lo, hi;
    lo = camera_x > 16 ? camera_x - 16 : 0;
    hi = camera_x + 272;
    while (win_lo < n_sp && sp_list[win_lo].x < lo) ++win_lo;
    while (win_lo && sp_list[win_lo - 1].x >= lo) --win_lo;
    if (win_hi < win_lo) win_hi = win_lo;
    while (win_hi < n_sp && sp_list[win_hi].x < hi) ++win_hi;
    while (win_hi > win_lo && sp_list[win_hi - 1].x >= hi) --win_hi;
}

void spirits_update(void) {
    static const SpiritSpawn *p;
    static u8 s, st;
    static u16 x1, x2;
    static u8 y1, y2;

    update_window();
    for (s = win_lo, p = sp_list + win_lo; s < win_hi; ++s, ++p) {
        st = sp_state[s];
        if (st == SP_GONE) continue;
        if (st != SP_WHOLE) {
            if (st == 1) {
                sp_state[s] = SP_GONE;
                drop_item(p);
            } else {
                sp_state[s] = st - 1;
            }
            continue;
        }
        if (!player_attack_hit) continue;
        x1 = p->x - SPIRIT_HALF_W;
        x2 = p->x + (SPIRIT_HALF_W - 1);
        y2 = p->y + 1;
        y1 = p->y - (SPIRIT_HEIGHT + 2);
        if (player_hit_x2 >= x1 && player_hit_x1 <= x2
            && player_hit_y2 >= y1 && player_hit_y1 <= y2) {
            sp_state[s] = BREAK_TIME;
            sfx_play(SFX_BREAK, SFX_WORLD);
        }
    }

    items_update();
}

void spirits_draw(void) {
    static const SpiritSpawn *p;
    static u8 s, st, k, y, anim, phase;

    anim = frame_count / SPIRIT_ANIM + win_lo;
    phase = (frame_count >> 3) + win_lo + win_lo + win_lo;
    for (s = win_lo, p = sp_list + win_lo; s < win_hi; ++s, ++p, ++anim, phase += 3)  {
        st = sp_state[s];
        if (st == SP_GONE) continue;
        if (st == SP_WHOLE) {
            y = p->y + bob[phase & 7];
            oam_meta_spr((s16)(p->x - camera_x), y, anim & 1 ? spirit2 : spirit1, SPIRIT_PAL);
        } else {
            oam_meta_spr((s16)(p->x - camera_x), p->y,
                         break_frames[(st - 1) >> 3], FLAME_PAL);
        }
    }

    for (k = 0; k < MAX_ITEMS; ++k) {
        if (it_type[k] == ITEM_NONE) continue;
        if (it_life[k] < ITEM_BLINK && (frame_count & 4)) continue;
        oam_meta_spr((s16)(it_x[k] - camera_x), it_y[k], it_frame[k], it_pal[k]);
    }
}