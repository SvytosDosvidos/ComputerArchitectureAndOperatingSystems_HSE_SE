#include "../include/fleet.h"
#include <string.h>
#include <stdlib.h>

void fleet_init(Fleet *f, FleetId id, const char *name, int board_w, int board_h,
                CoordinationMode coord_mode, ShotRule shot_rule, bool allow_repeat) {
    if (!f) return;
    f->id = id;
    strncpy(f->name, name ? name : "Unknown Fleet", sizeof(f->name) - 1);
    f->name[sizeof(f->name) - 1] = 0;
    f->ship_count = 0;
    f->coord_mode = coord_mode;
    f->shot_rule = shot_rule;
    f->allow_repeat_targets = allow_repeat;
    f->round_salvo_count = 0;

    board_init(&f->own_board, board_w, board_h);

    for (int y = 0; y < MAX_DIM; ++y) {
        for (int x = 0; x < MAX_DIM; ++x) {
            f->enemy_revealed[y][x] = false;
            f->enemy_hits[y][x] = false;
        }
    }
}

bool fleet_is_alive(const Fleet *f) {
    if (!f) return false;
    for (int i = 0; i < f->ship_count; ++i) {
        if (f->ships[i].is_alive) return true;
    }
    return false;
}

int fleet_alive_ship_count(const Fleet *f) {
    if (!f) return 0;
    int count = 0;
    for (int i = 0; i < f->ship_count; ++i) {
        if (f->ships[i].is_alive) count++;
    }
    return count;
}

int fleet_total_alive_decks(const Fleet *f) {
    if (!f) return 0;
    int count = 0;
    for (int i = 0; i < f->ship_count; ++i) {
        if (f->ships[i].is_alive) {
            count += f->ships[i].decks_alive;
        }
    }
    return count;
}

int fleet_prepare_round_salvo(Fleet *f) {
    if (!f) return 0;
    f->round_salvo_count = 0;

    for (int i = 0; i < f->ship_count; ++i) {
        f->ships[i].died_in_current_round = false;
    }

    Point temp_salvo[MAX_SHIP_LENGTH];

    for (int i = 0; i < f->ship_count; ++i) {
        AutonomousShip *ship = &f->ships[i];
        if (!ship->is_alive) continue;

        int shots = ship_get_shot_count(ship, f->shot_rule);
        int generated = ship_generate_salvo(ship, f->own_board.width, f->own_board.height,
                                            shots, f->enemy_hits,
                                            temp_salvo, MAX_SHIP_LENGTH,
                                            f->allow_repeat_targets);

        for (int s = 0; s < generated; ++s) {
            if (f->round_salvo_count >= MAX_SALVO_CAPACITY) break;
            f->round_salvo[f->round_salvo_count].target = temp_salvo[s];
            f->round_salvo[f->round_salvo_count].shooter_ship_id = ship->id;
            f->round_salvo_count++;
        }
    }

    if (f->coord_mode == COORD_DECONFLICT && f->round_salvo_count > 1) {
        for (int i = 0; i < f->round_salvo_count; ++i) {
            for (int j = i + 1; j < f->round_salvo_count; ++j) {
                if (f->round_salvo[i].target.x == f->round_salvo[j].target.x &&
                    f->round_salvo[i].target.y == f->round_salvo[j].target.y) {
                    for (int dy = 0; dy < f->own_board.height; ++dy) {
                        bool found_new = false;
                        for (int dx = 0; dx < f->own_board.width; ++dx) {
                            Point candidate = {dx, dy};
                            bool taken = false;
                            for (int k = 0; k < f->round_salvo_count; ++k) {
                                if (f->round_salvo[k].target.x == candidate.x &&
                                    f->round_salvo[k].target.y == candidate.y) {
                                    taken = true;
                                    break;
                                }
                            }
                            if (!taken && !f->enemy_revealed[candidate.y][candidate.x]) {
                                f->round_salvo[j].target = candidate;
                                found_new = true;
                                break;
                            }
                        }
                        if (found_new) break;
                    }
                }
            }
        }
    }

    return f->round_salvo_count;
}

void fleet_record_shot_result(Fleet *f, Point pt, ShotOutcome outcome) {
    if (!f) return;
    if (board_is_valid_coord(&f->own_board, pt.x, pt.y)) {
        f->enemy_revealed[pt.y][pt.x] = true;
        if (outcome == SHOT_HIT || outcome == SHOT_SUNK) {
            f->enemy_hits[pt.y][pt.x] = true;
        }
    }
}
