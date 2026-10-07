#ifndef FLEET_H
#define FLEET_H

#include "common.h"
#include "ship.h"
#include "board.h"

typedef struct {
    Point target;
    int shooter_ship_id;
} PlannedShot;

typedef struct {
    FleetId id;
    char name[32];
    AutonomousShip ships[MAX_SHIPS];
    int ship_count;
    Board own_board;
    bool enemy_revealed[MAX_DIM][MAX_DIM];
    bool enemy_hits[MAX_DIM][MAX_DIM];
    CoordinationMode coord_mode;
    ShotRule shot_rule;
    bool allow_repeat_targets;

    PlannedShot round_salvo[MAX_SALVO_CAPACITY];
    int round_salvo_count;
} Fleet;

void fleet_init(Fleet *f, FleetId id, const char *name, int board_w, int board_h,
                CoordinationMode coord_mode, ShotRule shot_rule, bool allow_repeat);
bool fleet_is_alive(const Fleet *f);
int fleet_alive_ship_count(const Fleet *f);
int fleet_total_alive_decks(const Fleet *f);
int fleet_prepare_round_salvo(Fleet *f);
void fleet_record_shot_result(Fleet *f, Point pt, ShotOutcome outcome);

#endif
