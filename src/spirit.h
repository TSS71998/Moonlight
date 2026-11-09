#ifndef SPIRIT_H
#define SPIRIT_H

#include "nes.h"

#define MAX_SPIRIT_SPAWNS 16
#define MAX_ITEMS         4

#define ITEM_POINTS      0
#define ITEM_POINTS_BIG  1
#define ITEM_HEAL        2
#define ITEM_HEAL_BIG    3
#define ITEM_NONE        0xFF

typedef struct {
    u16 x;
    u8 y;
    u8 item;
} SpiritSpawn;

void __fastcall__ spirits_load(const SpiritSpawn *list);

void spirits_reset(void);

void spirits_update(void);

void spirits_draw(void);

extern const SpiritSpawn forest_spirits[];

#endif