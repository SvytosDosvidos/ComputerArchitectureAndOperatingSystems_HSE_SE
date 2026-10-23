#ifndef JUDGE_H
#define JUDGE_H

#include "fleet.h"

typedef struct {
    int total_rounds;
    int total_shots_fired[2];
    int total_hits[2];
    int total_ships_sunk[2];
    int winner;
    const char *termination_reason;
} BattleStatistics;

typedef struct {
    int max_rounds;
    int current_round;
    bool battle_over;
    BattleStatistics stats;
} Judge;

void judge_init(Judge *j, int max_rounds);
bool judge_verify_round_invariants(Judge *j, const Fleet *fA, const Fleet *fB);
void judge_process_simultaneous_salvos(Judge *j, Fleet *fA, Fleet *fB, int log_fd);
bool judge_check_battle_completion(Judge *j, const Fleet *fA, const Fleet *fB);

#endif
