#include "../include/ship.h"
#include <stdlib.h>
#include <string.h>

void ship_init(AutonomousShip *ship, int id, FleetId fleet, int x, int y, int length, Orientation orient, TargetStrategy strat) {
    if (!ship) return;
    ship->id = id;
    ship->fleet_id = fleet;
    ship->x = x;
    ship->y = y;
    ship->length = (length > 0 && length <= MAX_SHIP_LENGTH) ? length : 1;
    ship->orient = orient;
    ship->decks_total = ship->length;
    ship->decks_alive = ship->length;
    ship->is_alive = true;
    ship->died_in_current_round = false;
    ship->strategy = strat;

    for (int i = 0; i < MAX_SHIP_LENGTH; ++i) {
        ship->deck_hit[i] = false;
    }
}

int ship_get_shot_count(const AutonomousShip *ship, ShotRule rule) {
    if (!ship) return 0;
    if (!ship->is_alive) return 0;

    switch (rule) {
        case SHOT_RULE_DECKS_ALIVE:
            return ship->decks_alive;
        case SHOT_RULE_FIXED_ONE:
            return 1;
        case SHOT_RULE_MAX_TWO:
            return (ship->decks_alive >= 2) ? 2 : 1;
        default:
            return ship->decks_alive;
    }
}

static bool is_target_already_in_salvo(Point pt, const Point *salvo, int count) {
    for (int i = 0; i < count; ++i) {
        if (salvo[i].x == pt.x && salvo[i].y == pt.y) return true;
    }
    return false;
}

int ship_generate_salvo(AutonomousShip *ship, int board_w, int board_h,
                        int shots_count, bool known_board[MAX_DIM][MAX_DIM],
                        Point *out_salvo, int max_salvo, bool allow_repeat_target) {
    if (!ship || !out_salvo || shots_count <= 0) return 0;
    if (!ship->is_alive) return 0;

    int formed = 0;
    int max_attempts = board_w * board_h * 4;
    int attempts = 0;

    while (formed < shots_count && formed < max_salvo && attempts < max_attempts) {
        attempts++;
        Point candidate;
        candidate.x = rand() % board_w;
        candidate.y = rand() % board_h;

        if (ship->strategy == STRATEGY_HUNT_TARGET) {
            bool hunt_found = false;
            for (int y = 0; y < board_h && !hunt_found; ++y) {
                for (int x = 0; x < board_w && !hunt_found; ++x) {
                    if (known_board[y][x]) {
                        int dx[] = {0, 0, -1, 1};
                        int dy[] = {-1, 1, 0, 0};
                        int dir_idx = rand() % 4;
                        for (int k = 0; k < 4; ++k) {
                            int nx = x + dx[(dir_idx + k) % 4];
                            int ny = y + dy[(dir_idx + k) % 4];
                            if (nx >= 0 && nx < board_w && ny >= 0 && ny < board_h) {
                                if (!known_board[ny][nx]) {
                                    candidate.x = nx;
                                    candidate.y = ny;
                                    hunt_found = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        } else if (ship->strategy == STRATEGY_CHECKERBOARD) {
            if ((candidate.x + candidate.y) % 2 != 0) {
                candidate.x = (candidate.x + 1) % board_w;
            }
        }

        if (!allow_repeat_target && is_target_already_in_salvo(candidate, out_salvo, formed)) {
            continue;
        }

        out_salvo[formed++] = candidate;
    }

    while (formed < shots_count && formed < max_salvo) {
        Point fallback;
        fallback.x = rand() % board_w;
        fallback.y = rand() % board_h;
        if (!allow_repeat_target && is_target_already_in_salvo(fallback, out_salvo, formed)) {
            if (formed >= board_w * board_h) {
                out_salvo[formed++] = fallback;
                break;
            }
            continue;
        }
        out_salvo[formed++] = fallback;
    }

    return formed;
}

bool ship_receive_hit(AutonomousShip *ship, int deck_index) {
    if (!ship || deck_index < 0 || deck_index >= ship->length) return false;

    if (ship->deck_hit[deck_index]) {
        return false;
    }

    ship->deck_hit[deck_index] = true;
    if (ship->decks_alive > 0) {
        ship->decks_alive--;
    }

    if (ship->decks_alive == 0) {
        ship->is_alive = false;
        return true;
    }

    return false;
}
