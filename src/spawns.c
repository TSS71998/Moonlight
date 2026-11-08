#include "enemy.h"
#include "spirit.h"
#include "trap.h"

const EnemySpawn forest_spawns[] = {
    { 300, 192, ENEMY_SKELETON },
    { 470, 176, ENEMY_BAT },
    { 640, 192, ENEMY_SKELETON },
    { 800, 207, ENEMY_SKELETON },
    { 910, 110, ENEMY_BAT },
    { 1360, 192, ENEMY_SKELETON },
    { 1472, 192, ENEMY_SKELETON },
    { 0, 0, ENEMY_NONE },
};

const SpiritSpawn forest_spirits[] = {
    { 176, 184, ITEM_POINTS },
    { 352, 184, ITEM_HEAL },
    { 456, 168, ITEM_POINTS_BIG },
    { 600, 184, ITEM_POINTS },
    { 984, 120, ITEM_HEAL_BIG },
    { 1224, 184, ITEM_POINTS },
    { 1392, 184, ITEM_POINTS_BIG },
    { 0, 0, ITEM_NONE },
};

const TrapSpawn forest_traps[] = {
    { 384, 192, 90, 40, 0 },
    { 560, 192, 80, 30, 0 },
    { 584, 192, 80, 30, 40 },
    { 1088, 200, 80, 30, 0 },
    { 1120, 200, 80, 30, 40 },
    { 1152, 200, 80, 30, 80 },
    { 0, 0, 0, 0, 0 },
};