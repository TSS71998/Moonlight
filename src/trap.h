#ifndef TRAP_H
#define TRAP_H

#include "nes.h"

#define MAX_TRAP_SPAWNS 16

typedef struct {
    u16 x;
    u8 y;
    u8 down;
    u8 up;
    u8 start;
} TrapSpawn;

void __fastcall__ traps_load(const TrapSpawn *list);

void traps_reset(void);

void traps_update(void);

void traps_draw(void);

extern const TrapSpawn forest_traps[];


#endif