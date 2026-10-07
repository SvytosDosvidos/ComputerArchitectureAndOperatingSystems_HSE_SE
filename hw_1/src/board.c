#include "../include/board.h"
#include <string.h>

void board_init(Board *b, int width, int height) {
    if (!b) return;
    b->width = (width >= MIN_DIM && width <= MAX_DIM) ? width : DEFAULT_BOARD_WIDTH;
    b->height = (height >= MIN_DIM && height <= MAX_DIM) ? height : DEFAULT_BOARD_HEIGHT;

    for (int y = 0; y < MAX_DIM; ++y) {
        for (int x = 0; x < MAX_DIM; ++x) {
            b->grid[y][x] = CELL_EMPTY;
            b->ship_id_grid[y][x] = -1;
        }
    }
}

bool board_is_valid_coord(const Board *b, int x, int y) {
    if (!b) return false;
    return (x >= 0 && x < b->width && y >= 0 && y < b->height);
}

bool board_can_place_ship(const Board *b, int x, int y, int length, Orientation orient, bool allow_touching) {
    if (!b) return false;

    if (orient == ORIENT_HORIZONTAL) {
        if (x < 0 || x + length > b->width || y < 0 || y >= b->height) return false;
    } else {
        if (x < 0 || x >= b->width || y < 0 || y + length > b->height) return false;
    }

    int margin = allow_touching ? 0 : 1;

    for (int i = 0; i < length; ++i) {
        int cur_x = (orient == ORIENT_HORIZONTAL) ? (x + i) : x;
        int cur_y = (orient == ORIENT_VERTICAL) ? (y + i) : y;

        for (int dy = -margin; dy <= margin; ++dy) {
            for (int dx = -margin; dx <= margin; ++dx) {
                int test_x = cur_x + dx;
                int test_y = cur_y + dy;
                if (test_x >= 0 && test_x < b->width && test_y >= 0 && test_y < b->height) {
                    if (b->grid[test_y][test_x] == CELL_SHIP) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

bool board_place_ship(Board *b, int ship_id, int x, int y, int length, Orientation orient) {
    if (!b) return false;
    if (!board_can_place_ship(b, x, y, length, orient, false)) {
        return false;
    }

    for (int i = 0; i < length; ++i) {
        int cx = (orient == ORIENT_HORIZONTAL) ? (x + i) : x;
        int cy = (orient == ORIENT_VERTICAL) ? (y + i) : y;
        b->grid[cy][cx] = CELL_SHIP;
        b->ship_id_grid[cy][cx] = ship_id;
    }

    return true;
}

ShotOutcome board_apply_shot(Board *b, Point pt, int *out_ship_id, int *out_deck_idx) {
    if (out_ship_id) *out_ship_id = -1;
    if (out_deck_idx) *out_deck_idx = -1;

    if (!b || !board_is_valid_coord(b, pt.x, pt.y)) {
        return SHOT_REPEAT;
    }

    CellState current = b->grid[pt.y][pt.x];
    if (current == CELL_HIT || current == CELL_MISS) {
        return SHOT_REPEAT;
    }

    if (current == CELL_SHIP) {
        int s_id = b->ship_id_grid[pt.y][pt.x];
        b->grid[pt.y][pt.x] = CELL_HIT;
        if (out_ship_id) *out_ship_id = s_id;
        return SHOT_HIT;
    }

    b->grid[pt.y][pt.x] = CELL_MISS;
    return SHOT_MISS;
}

void board_mark_miss(Board *b, Point pt) {
    if (b && board_is_valid_coord(b, pt.x, pt.y)) {
        if (b->grid[pt.y][pt.x] == CELL_EMPTY) {
            b->grid[pt.y][pt.x] = CELL_MISS;
        }
    }
}
