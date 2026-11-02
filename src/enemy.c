#include "enemy.h"
#include "enemy_sprites.h"
#include "level.h"
#include "player.h"
#include "hud.h"

#define HI(v) ((u8)((v) >> 8))

#define BEH_WALK 0
#define BEH_FLY  1

typedef struct {
    const u8 *frame1;
    const u8 *frame2;
    u8 behavior;
    u8 speed;
    u8 anim_delay;
    u8 half_w;
    u8 height;
    u8 hp;
    u8 bar_step;
    u8 damage;
    u8 pal;
    u16 points;
} EnemyType;

static const EnemyType enemy_types[] = {
    { skele_walk1,  skele_walk2,   BEH_WALK,   8,  16, 5, 30, 2,  8,  2,  2,  100 },
    { bat_fly1,     bat_fly2,       BEH_FLY,   20,   6, 6, 12, 1, 16,  2,  3,  200 },
};

#define FLAME_FRAME_TIME 6
#define FLAME_PAL        1
static const u8 *const flame_frames[3] = { flame3, flame2, flame1 };

static const s8 wave[16] = { 0, 4, 7, 9, 10, 9, 7, 4, 0, -4, -7, -9, -10, -9, -7, -4 };

#define ENEMY_HURT_TIME 12
#define ENEMY_MAX_FALL  8

#define ES_FREE  0
#define ES_ALIVE 1
#define ES_DYING 2

static u8 e_state[MAX_ENEMIES];
static const EnemyType *e_def[MAX_ENEMIES];

static u8   e_spawn[MAX_ENEMIES];
static u16  e_x[MAX_ENEMIES];
static u8   e_y[MAX_ENEMIES];
static u8   e_left[MAX_ENEMIES];
static u8   e_hp[MAX_ENEMIES];
static u8   e_timer[MAX_ENEMIES];
static u8   e_frame[MAX_ENEMIES];
static u8   e_hurt[MAX_ENEMIES];
static u8   e_hit_id[MAX_ENEMIES];
static u8   e_frac[MAX_ENEMIES];
static u8   e_fall[MAX_ENEMIES];
static u8   e_base_y[MAX_ENEMIES];
static u8   e_phase[MAX_ENEMIES];

#define SF_IN_VIEW 1
#define SF_ALIVE   2
#define SF_KILLED  4
static u16 sp_x[MAX_SPAWNS];
static u8  sp_y[MAX_SPAWNS];
static u8  sp_type[MAX_SPAWNS];
static u8  sp_flags[MAX_SPAWNS];
static u8  n_spawns;

static u8 draw_start;

void __fastcall__ enemies_load(const EnemySpawn *list) {
    static u8 i;
    for (i = 0; i < MAX_ENEMIES; ++i) e_state[i] = ES_FREE;
    for (n_spawns = 0; n_spawns < MAX_SPAWNS && list->type != ENEMY_NONE; ++n_spawns, ++list) {
        sp_x[n_spawns] = list->x;
        sp_y[n_spawns] = list->y;
        sp_type[n_spawns] = list->type;
        sp_flags[n_spawns] = 0;
    } 
}

void enemies_reset(void) {
    static u8 i;
    for (i = 0; i < MAX_ENEMIES; ++i) e_state[i] = ES_FREE;
    for (i = 0; i < n_spawns; ++i) sp_flags[i] &= SF_KILLED;
}

static u8 __fastcall__ spawn(u8 s) {
    static u8 i;
    static const EnemyType *t;
    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (e_state[i] == ES_FREE) break;
    }
    if (i == MAX_ENEMIES) return 0;

    t = &enemy_types[sp_type[s]];
    e_state[i] = ES_ALIVE;
    e_def[i] = t;
    e_spawn[i] = s;
    e_x[i] = sp_x[s];
    e_y[i] = sp_y[s];
    e_base_y[i] = sp_y[s];
    e_left[i] = sp_x[s] > player_x + PLAYER_WIDTH / 2;    /* face the player */
    e_hp[i] = t->hp;
    e_timer[i] = t->anim_delay;
    e_frame[i] = 0;
    e_hurt[i] = 0;
    e_hit_id[i] = player_attack_id ^ 0x80;
    e_frac[i] = 0;
    e_fall[i] = 0;
    e_phase[i] = 0;
    sp_flags[s] |= SF_ALIVE;
    return 1;
}

static void __fastcall__ remove_enemy(u8 i) {
    e_state[i] = ES_FREE;
    sp_flags[e_spawn[i]] &= ~SF_ALIVE;
}

static void check_spawns(void) {
    static u16 lo, hi, x;
    static u8 s, f;
    lo = camera_x > SPAWN_MARGIN ? camera_x - SPAWN_MARGIN : 0;
    hi = camera_x + (256 + SPAWN_MARGIN);
    for (s = 0; s < n_spawns; ++s) {
        x = sp_x[s];
        f = sp_flags[s];
        if (x >= lo && x < hi) {
            if (!(f & SF_IN_VIEW)) {
                if ((f & (SF_ALIVE | SF_KILLED)) || spawn(s)) {
                    sp_flags[s] |= SF_IN_VIEW;
                }
            }
        } else {
            sp_flags[s] = f & ~SF_IN_VIEW;
        }
    }
}

static u8 i;
static const EnemyType *t;
static u8 step;

static void walk(void) {
    static u16 probe;
    static u8 y, top;

    y = e_y[i];
    if (step) {
        if (e_left[i]) probe = e_x[i] - t->half_w - step;
        else           probe = e_x[i] + t->half_w - 1 + step;
        top = y - t->height;
        if (level_solid_at(probe, top + 2) || level_solid_at(probe, top + (t->height >> 1))) {
            e_left[i] ^= 1;
            step = 0;
        } else if (level_solid_at(probe, y - 4)) {
            if (!e_fall[i] && !level_solid_at(probe, y - 12)) {
                y -= 8;
                e_y[i] = y;
            } else {
                e_left[i] ^= 1;
                step = 0;
            }
        } 
        if (e_left[i]) e_x[i] -= step;
        else           e_x[i] += step;
    }

    if (!e_fall[i]) {
        if (step && !level_solid_at(e_x[i] - t->half_w, y)
                 && !level_solid_at(e_x[i] + t->half_w - 1, y)) e_fall[i] = 1;
    } else {
        y += e_fall[i];
        if (e_fall[i] < ENEMY_MAX_FALL) ++e_fall[i];
        if (y >= 240) {
            remove_enemy(i);
            return;
        }
        if (level_solid_at(e_x[i] - t->half_w, y - 1)
            || level_solid_at(e_x[i] + t->half_w - 1, y - 1)) {
            y = (y - 1) & 0xF8;
             e_fall[i] = 0;
         }
        e_y[i] = y;
    }
}

static void fly(void) {
    if (e_left[i]) e_x[i] -= step;
    else           e_x[i] += step;
    e_phase[i] += 2;
    e_y[i] = e_base_y[i] + wave[(e_phase[i] >> 2) & 15];
}

static void kill(void) {
    e_state[i] = ES_DYING;
    e_timer[i] = FLAME_FRAME_TIME * 3;
    sp_flags[e_spawn[i]] |= SF_KILLED;
    hud_add_score(t->points);
}

static void fight(void) {
    static u16 x1, x2;
    static u8 y1, y2, n, top;

    x1 = e_x[i] - player_x + 40;
    if (x1 > 96) return;

    x1 = e_x[i] - t->half_w;
    x2 = e_x[i] + t->half_w - 1;
    y2 = e_y[i] - 1;
    y1 = e_y[i] - t->height;

    if (player_attack_hit && e_hit_id[i] != player_attack_id
        && player_hit_x2 >= x1 && player_hit_x1 <= x2
        && player_hit_y2 >= y1 && player_hit_y1 <= y2) {
        e_hit_id[i] = player_attack_id;
        --e_hp[i];
        for (n = 0, top = e_hp[i]; top; --top) n += t->bar_step;
        hud_set_enemy_hp(n);
        if (!e_hp[i]) {
            kill();
            return;
        }
        e_hurt[i] = ENEMY_HURT_TIME;
    }

    if (!player_invuln && player_state != PS_HURT
        && player_x + HB_RIGHT >= x1 && player_x + HB_LEFT <= x2
        && HI(player_y) + HB_BOTTOM >= y1 && HI(player_y) + HB_TOP <= y2) {
        player_hurt(t->damage, e_x[i] > player_x + PLAYER_WIDTH / 2);
    }
}

void enemies_update(void) {
    if (frame_count & 1) check_spawns();

    for (i = frame_count & 1; i < MAX_ENEMIES; i += 2) {
        if (e_state[i] == ES_FREE) continue;
        t = e_def[i];

        if (e_state[i] == ES_DYING) {
            if (e_timer[i] <= 2) remove_enemy(i);
            else e_timer[i] -= 2;
            continue;
        }

        if (e_hurt[i]) {
            e_hurt[i] = e_hurt[i] > 2 ? e_hurt[i] - 2: 0;
        } else {
            e_frac[i] += t->speed << 1;
            step = e_frac[i] >> 4;
            e_frac[i] &= 15;
            if (t->behavior == BEH_WALK) walk();
            else                         fly();
            if (e_state[i] == ES_FREE) continue;

            if (e_timer[i] <= 2) {
                e_timer[i] = t->anim_delay;
                e_frame[i] ^= 1;
            } else {
                e_timer[i] -= 2;
            }
        }

        if ((camera_x > DESPAWN_MARGIN && e_x[i] < camera_x - DESPAWN_MARGIN)
            || e_x[i] > camera_x + (256 + DESPAWN_MARGIN)) {
            remove_enemy(i);
            continue;
        }

        fight();
    }
}

void enemies_draw(void) {
    static u8 k, j, attr;
    static const u8 *frame;

    j = draw_start = (draw_start + 1) & (MAX_ENEMIES - 1);
    for (k = 0; k < MAX_ENEMIES; ++k, j = (j + 1) & (MAX_ENEMIES - 1)) {
        if (e_state[j] == ES_FREE) continue;
        if (e_state[j] == ES_DYING) {
            frame = flame_frames[(e_timer[j] - 1) / FLAME_FRAME_TIME];
            attr = FLAME_PAL;
        } else {
            if (e_hurt[j] & 2) continue;
            t = e_def[j];
            frame = e_frame[j] ? t->frame2 : t->frame1;
            attr = t->pal | (e_left[j] ? SPR_FLIP_H : 0);
        }
        oam_meta_spr((s16)(e_x[j] - camera_x), e_y[j], frame, attr);
    }
}