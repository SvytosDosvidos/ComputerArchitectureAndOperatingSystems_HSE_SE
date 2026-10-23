#ifndef BOARD_H
#define BOARD_H

#include "common.h"

typedef struct {
    int width;
    int height;
    CellState grid[MAX_DIM][MAX_DIM];
    int ship_id_grid[MAX_DIM][MAX_DIM];
} Board;

void board_init(Board *b, int width, int height);
bool board_is_valid_coord(const Board *b, int x, int y);
bool board_can_place_ship(const Board *b, int x, int y, int length, Orientation orient, bool allow_touching);
bool board_place_ship(Board *b, int ship_id, int x, int y, int length, Orientation orient);
ShotOutcome board_apply_shot(Board *b, Point pt, int *out_ship_id, int *out_deck_idx);
void board_mark_miss(Board *b, Point pt);

#endif
