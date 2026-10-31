#include "player.h"
#include "level.h"
#include "hero_sprites.h"

#define HI(v) ((u8)((v) >> 8))

u16 player_x;
u16 player_y;
s16 player_vy;
s16 player_jump_vx;
u8  player_facing;
u8  player_on_ground;
u8  player_state;

static u16 spawn_x, spawn_y;

static u8 anim_frame;
static u8 anim_timer;

static const u8 *const walk_cycle[] = {
    hero_walk1, hero_walk2, hero_walk3, hero_walk2,
};
#define WALK_CYCLE_LEN (sizeof(walk_cycle) / sizeof(walk_cycle[0]))

static void __fastcall__ set_state(u8 state) {
    if (state != player_state) {
        player_state = state;
        anim_frame = 0;
        anim_timer = WALK_ANIM_DELAY;
    }
}

typedef struct {
    const u8 *body;
    const u8 *body_crouch;
    const u8 *sword;
    u8 frames;
    u8 sword_in_front;
    u8 hits;
} AttackPhase;

static const AttackPhase attack_phases[] = {
    { hero_attack1, hero_crouch_attack1, sword_raised,  5, 0, 0 },
    { hero_attack2, hero_crouch_attack2, sword_swing,   3, 1, 0 },
    { hero_attack2, hero_crouch_attack2, sword_thrust, 12, 1, 1 },
};
#define ATTACK_PHASES (sizeof(attack_phases) / sizeof(attack_phases[0]))

static u8 attack_phase;
static u8 attack_timer;
static u8 attack_crouch;
static u8 attack_air_state;

u8 player_attack_hit;
u8 player_hit_x1, player_hit_y1, player_hit_x2, player_hit_y2;

static void __fastcall__ start_attack(u8 pad) {
    attack_crouch = player_on_ground && (pad & PAD_DOWN);
    attack_air_state = player_state == PS_JUMP ? PS_JUMP : PS_FALL;
    attack_phase = 0;
    attack_timer = attack_phases[0].frames;
    player_state = PS_ATTACK;
}

static void __fastcall__ update_attack(u8 pad) {
    static u8 ax, ay;

    if (--attack_timer == 0) {
        if (++attack_phase == ATTACK_PHASES) {
            /* done: back to whatever the player is doing now */
            player_attack_hit = 0;
            if (!player_on_ground)   player_state = attack_air_state;
            else if (pad & PAD_DOWN) player_state = PS_CROUCH;
            else                     player_state = PS_STAND;
            anim_frame = 0;
            anim_timer = WALK_ANIM_DELAY;
            return;
        }
        attack_timer = attack_phases[attack_phase].frames;
    }

    player_attack_hit = attack_phases[attack_phase].hits;
    if (player_attack_hit) {
        ax = HI(player_x) + 8;                  /* feet anchor */
        ay = HI(player_y) + 32 + (attack_crouch ? CROUCH_DROP : 0);
        if (player_facing == FACE_RIGHT) {
            player_hit_x1 = ax + SWORD_HB_X1;
            player_hit_x2 = ax + SWORD_HB_X2;
        } else {
            /* mirror around the anchor: pixel +k becomes -1-k */
            player_hit_x1 = ax - 1 - SWORD_HB_X2;
            player_hit_x2 = ax - 1 - SWORD_HB_X1;
        }
        player_hit_y1 = ay + SWORD_HB_Y1;
        player_hit_y2 = ay + SWORD_HB_Y2;
    }
}

void __fastcall__ player_init(u8 x, u8 y) {
    spawn_x = player_x = (u16)x << 8;
    spawn_y = player_y = (u16)y << 8;
    player_vy = 0;
    player_jump_vx = 0;
    player_facing = FACE_RIGHT;
    player_on_ground = 0;
    player_state = PS_FALL;
    anim_frame = 0;
    anim_timer = WALK_ANIM_DELAY;
}

static u8 __fastcall__ wall_at(u8 edge_x, u8 top_y) {
    return level_solid_at(edge_x, top_y + HB_TOP)
        || level_solid_at(edge_x, top_y + 17)
        || level_solid_at(edge_x, top_y + HB_BOTTOM);
}

static u8 __fastcall__ floor_at(u8 left_x, u8 edge_y) {
    return level_solid_at(left_x + HB_LEFT, edge_y)
        || level_solid_at(left_x + HB_RIGHT, edge_y);
}

static void move_horizontal(s16 vx) {
    static u16 nx;
    static u8 edge;

    nx = player_x + vx;
    if (vx > 0) {
        edge = HI(nx) + HB_RIGHT;
        if (wall_at(edge, HI(player_y))) {
            nx = (u16)((edge & 0xF0) - 1 - HB_RIGHT) << 8;
        }
    } else {
        edge = HI(nx) + HB_LEFT;
        if (wall_at(edge, HI(player_y))) {
            nx = (u16)((edge & 0xF0) + 16 - HB_LEFT) << 8;
        }
    }
    player_x = nx;
}

static void move_vertical(void) {
    static u16 ny;
    static u8 py, edge;

    player_vy += GRAVITY;
    if (player_vy > MAX_FALL) player_vy = MAX_FALL;
    ny = player_y + player_vy;
    py = HI(ny);

    if (player_vy > 0) {
        if (py < 240 - HB_BOTTOM) {
            edge = py + HB_BOTTOM;
            if (floor_at(HI(player_x), edge)) {
                ny = (u16)((edge & 0xF0) - 1 - HB_BOTTOM) << 8;
                player_vy = 0;
                player_jump_vx = 0;
                player_on_ground = 1;
            }
        }
    } else {
        edge = py + HB_TOP;
        if (floor_at(HI(player_x), edge)) {
            ny = (u16)((edge & 0xF0) + 16 - HB_TOP) << 8;
            player_vy = 0;
        }
    }
    player_y = ny;

    if (HI(player_y) >= 240 && HI(player_y) < 250) {
        player_init(HI(spawn_x), HI(spawn_y));
    }
}

void __fastcall__ player_update(u8 pad, u8 pressed) {
    static s16 vx;

    if ((pressed & PAD_B) && player_state != PS_ATTACK) {
        start_attack(pad);
    }

    if (player_state == PS_ATTACK) {
        vx = player_on_ground ? 0 : player_jump_vx;
    } else if (player_on_ground) {
        vx = 0;
        if (pad & PAD_LEFT) {
            vx = -WALK_SPEED;
            player_facing = FACE_LEFT;
        } else if (pad & PAD_RIGHT) {
            vx = WALK_SPEED;
            player_facing = FACE_RIGHT;
        }

        if (pad & PAD_DOWN) {
            vx = 0;
        } else if (pressed & PAD_A) {
            player_vy = -JUMP_VELOCITY;
            player_jump_vx = vx > 0 ? JUMP_SPEED : vx < 0 ? -JUMP_SPEED : 0;
            player_on_ground = 0;
            set_state(PS_JUMP);
        }
    } else {
        vx = player_jump_vx;
    }

    if (vx) move_horizontal(vx);

    if (player_on_ground) {
        if (!floor_at(HI(player_x), HI(player_y) + HB_BOTTOM + 1)) {
            player_on_ground = 0;
            player_vy = 0;
            player_jump_vx = 0;
        }
    } else {
        move_vertical();
    }

    if (player_state == PS_ATTACK) {
        update_attack(pad);
    } else if (player_on_ground) {
        if (pad & PAD_DOWN) set_state(PS_CROUCH);
        else if (vx)        set_state(PS_WALK);
        else                set_state(PS_STAND);
    } else if (player_state != PS_JUMP) {
        set_state(PS_FALL);
    }

    if (player_state == PS_WALK) {
        if (--anim_timer == 0) {
            anim_timer = WALK_ANIM_DELAY;
            if (++anim_frame == WALK_CYCLE_LEN) anim_frame = 0;   /* walk fix: = not == */
        }
    }
}

void player_draw(void) {
    static const u8 *frame;
    static const AttackPhase *ph;
    static u8 ax, ay, flip;

    ax = HI(player_x) + 8;          /* anchor = middle of the feet */
    ay = HI(player_y) + 32;
    flip = player_facing == FACE_LEFT ? SPR_FLIP_H : 0;

    if (player_state == PS_ATTACK) {
        ph = &attack_phases[attack_phase];
        frame = attack_crouch ? ph->body_crouch : ph->body;
        /* sprites added first are drawn on top */
        if (ph->sword_in_front) {
            oam_meta_spr(ax, ay + (attack_crouch ? CROUCH_DROP : 0), ph->sword, flip);
            oam_meta_spr(ax, ay, frame, flip);
        } else {
            oam_meta_spr(ax, ay, frame, flip);
            oam_meta_spr(ax, ay + (attack_crouch ? CROUCH_DROP : 0), ph->sword, flip);
        }
        return;
    }

    switch (player_state) {
        case PS_WALK:
            frame = walk_cycle[anim_frame];
            break;
        case PS_CROUCH:
            frame = hero_crouch;
            break;
        case PS_JUMP:
            frame = player_vy < JUMP_UNTUCK_VY ? hero_crouch : hero_stand;
            break;
        default:
            frame = hero_stand;
            break;
    }

    oam_meta_spr(ax, ay, frame, flip);
}