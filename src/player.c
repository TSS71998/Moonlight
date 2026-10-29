#include "player.h"
#include "level.h"

#define HI(v) ((u8)((v) >> 8))

u16 player_x;
u16 player_y;
s16 player_vy;
s16 player_jump_vx;
u8  player_facing;
u8  player_on_ground;

static u16 spawn_x, spawn_y;

void __fastcall__ player_init(u8 x, u8 y) {
    spawn_x = player_x = (u16)x << 8;
    spawn_y = player_y = (u16)y << 8;
    player_vy = 0;
    player_jump_vx = 0;
    player_facing = FACE_RIGHT;
    player_on_ground = 0;
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

    if (player_on_ground) {
        vx = 0;
        if (pad & PAD_LEFT) {
            vx = -WALK_SPEED;
            player_facing = FACE_LEFT;
        } else if (pad & PAD_RIGHT) {
            vx = WALK_SPEED;
            player_facing = FACE_RIGHT;
        }

        if (pressed & PAD_A) {
            player_vy = -JUMP_VELOCITY;
            player_jump_vx = vx > 0 ? JUMP_SPEED : vx < 0 ? -JUMP_SPEED : 0;
            player_on_ground = 0;
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
}

void player_draw(void) {
    static u8 x, y;

    x = HI(player_x);
    y = HI(player_y);
    if (player_facing == FACE_RIGHT) {
        oam_spr(x,     y,      0x01, 0);
        oam_spr(x + 8, y,      0x03, 0);
        oam_spr(x,     y + 16, 0x05, 0);
        oam_spr(x + 8, y + 16, 0x07, 0);
    } else {
        oam_spr(x,     y,      0x03, SPR_FLIP_H);
        oam_spr(x + 8, y,      0x01, SPR_FLIP_H);
        oam_spr(x,     y + 16, 0x07, SPR_FLIP_H);
        oam_spr(x + 8, y + 16, 0x05, SPR_FLIP_H);
    }
}