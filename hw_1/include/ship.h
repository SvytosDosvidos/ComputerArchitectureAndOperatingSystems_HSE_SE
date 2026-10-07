#ifndef SHIP_H
#define SHIP_H

#include "common.h"

typedef struct {
    int id;
    FleetId fleet_id;
    int x;
    int y;
    int length;
    Orientation orient;
    int decks_total;
    int decks_alive;
    bool deck_hit[MAX_SHIP_LENGTH];
    bool is_alive;
    bool died_in_current_round;
    TargetStrategy strategy;
} AutonomousShip;

void ship_init(AutonomousShip *ship, int id, FleetId fleet, int x, int y, int length, Orientation orient, TargetStrategy strat);
int ship_get_shot_count(const AutonomousShip *ship, ShotRule rule);
int ship_generate_salvo(AutonomousShip *ship, int board_w, int board_h,
                        int shots_count, bool known_board[MAX_DIM][MAX_DIM],
                        Point *out_salvo, int max_salvo, bool allow_repeat_target);
bool ship_receive_hit(AutonomousShip *ship, int deck_index);

#endif
