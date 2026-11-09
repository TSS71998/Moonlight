#ifndef STAGE_H
#define STAGE_H

#include "nes.h"
#include "level.h"
#include "enemy.h"
#include "spirit.h"
#include "trap.h"
#include "boss.h"

typedef struct {
    const LevelDef *level;
    u8 chr;
    const EnemySpawn *enemies;
    const SpiritSpawn *spirits;
    const TrapSpawn *traps;
    u8 song;
    u16 start_x;
    u8 start_y;
    u16 exit_x;
    u8 boss;
} Stage;

extern const Stage stages[];
extern const u8 stage_count;

#endif