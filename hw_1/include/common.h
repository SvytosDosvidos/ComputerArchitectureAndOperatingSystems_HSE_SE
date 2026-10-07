#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_DIM 30
#define MIN_DIM 5
#define DEFAULT_BOARD_WIDTH 10
#define DEFAULT_BOARD_HEIGHT 10

#define MAX_SHIPS 32
#define MAX_SHIP_LENGTH 8
#define MAX_SALVO_CAPACITY (MAX_SHIPS * MAX_SHIP_LENGTH)

#define DEFAULT_MAX_ROUNDS 100
#define DEFAULT_STEP_DELAY_MS 250

typedef enum {
    FLEET_A = 0,
    FLEET_B = 1
} FleetId;

typedef enum {
    CELL_EMPTY = 0,
    CELL_SHIP = 1,
    CELL_HIT = 2,
    CELL_MISS = 3
} CellState;

typedef enum {
    SHOT_MISS = 0,
    SHOT_HIT = 1,
    SHOT_SUNK = 2,
    SHOT_REPEAT = 3
} ShotOutcome;

typedef struct {
    int x;
    int y;
} Point;

typedef enum {
    ORIENT_HORIZONTAL = 0,
    ORIENT_VERTICAL = 1
} Orientation;

typedef enum {
    STRATEGY_RANDOM = 0,
    STRATEGY_HUNT_TARGET = 1,
    STRATEGY_CHECKERBOARD = 2
} TargetStrategy;

typedef enum {
    SHOT_RULE_DECKS_ALIVE = 0,
    SHOT_RULE_FIXED_ONE = 1,
    SHOT_RULE_MAX_TWO = 2
} ShotRule;

typedef enum {
    COORD_NONE = 0,
    COORD_DECONFLICT = 1
} CoordinationMode;

#endif
