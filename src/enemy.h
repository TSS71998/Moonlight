#ifndef ENEMY_H
#define ENEMY_H

#include "nes.h"

#define MAX_ENEMIES     4
#define MAX_SPAWNS      32
#define SPAWN_MARGIN    16
#define DESPAWN_MARGIN  48

#define ENEMY_SKELETON  0
#define ENEMY_BAT       1
#define ENEMY_NONE      0xFF

typedef struct {
    u16 x;
    u8 y;
    u8 type;
} EnemySpawn;

void __fastcall__ enemies_load(const EnemySpawn *list);

void enemies_reset(void);

void eneimes_update(void);

void enemies_draw(void);

extern const EnemySpawn forest_spawns[];

#endif