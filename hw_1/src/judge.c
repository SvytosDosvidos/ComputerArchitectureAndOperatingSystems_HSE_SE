#include "../include/judge.h"
#include "../include/logger.h"
#include <stdio.h>
#include <string.h>

void judge_init(Judge *j, int max_rounds) {
    if (!j) return;
    j->max_rounds = (max_rounds > 0) ? max_rounds : DEFAULT_MAX_ROUNDS;
    j->current_round = 0;
    j->battle_over = false;

    memset(&j->stats, 0, sizeof(BattleStatistics));
    j->stats.winner = -1;
    j->stats.termination_reason = "Бой не завершен";
}

bool judge_verify_round_invariants(Judge *j, const Fleet *fA, const Fleet *fB) {
    if (!j || !fA || !fB) return false;

    for (int i = 0; i < fA->round_salvo_count; ++i) {
        int ship_id = fA->round_salvo[i].shooter_ship_id;
        for (int k = 0; k < fA->ship_count; ++k) {
            if (fA->ships[k].id == ship_id && !fA->ships[k].is_alive) {
                log_event("СУДЬЯ: ОШИБКА", "Корабль #%d флота A уничтожен до раунда, но сформировал выстрел!", ship_id);
                return false;
            }
        }
    }

    for (int i = 0; i < fB->round_salvo_count; ++i) {
        int ship_id = fB->round_salvo[i].shooter_ship_id;
        for (int k = 0; k < fB->ship_count; ++k) {
            if (fB->ships[k].id == ship_id && !fB->ships[k].is_alive) {
                log_event("СУДЬЯ: ОШИБКА", "Корабль #%d флота B уничтожен до раунда, но сформировал выстрел!", ship_id);
                return false;
            }
        }
    }

    return true;
}

static AutonomousShip *find_ship_by_id(Fleet *f, int ship_id) {
    for (int i = 0; i < f->ship_count; ++i) {
        if (f->ships[i].id == ship_id) return &f->ships[i];
    }
    return NULL;
}

static int get_deck_index_for_point(const AutonomousShip *ship, Point pt) {
    if (ship->orient == ORIENT_HORIZONTAL) {
        return pt.x - ship->x;
    } else {
        return pt.y - ship->y;
    }
}

void judge_process_simultaneous_salvos(Judge *j, Fleet *fA, Fleet *fB, int log_fd) {
    if (!j || !fA || !fB) return;
    (void)log_fd;

    j->current_round++;
    log_event_round_start(j->current_round);

    log_event_ship_readiness(fA);
    log_event_formed_targets(fA);
    log_event_ship_readiness(fB);
    log_event_formed_targets(fB);

    j->stats.total_shots_fired[0] += fA->round_salvo_count;
    j->stats.total_shots_fired[1] += fB->round_salvo_count;

    typedef struct {
        Point target;
        int shooter_id;
        ShotOutcome outcome;
        int victim_id;
        bool sunk;
    } ShotEffect;

    ShotEffect effectsA[MAX_SALVO_CAPACITY];
    int countA = fA->round_salvo_count;

    ShotEffect effectsB[MAX_SALVO_CAPACITY];
    int countB = fB->round_salvo_count;

    for (int i = 0; i < countA; ++i) {
        effectsA[i].target = fA->round_salvo[i].target;
        effectsA[i].shooter_id = fA->round_salvo[i].shooter_ship_id;
        effectsA[i].outcome = SHOT_MISS;
        effectsA[i].victim_id = -1;
        effectsA[i].sunk = false;

        int victim_ship_id = -1;
        int deck_idx = -1;
        ShotOutcome outcome = board_apply_shot(&fB->own_board, effectsA[i].target, &victim_ship_id, &deck_idx);
        effectsA[i].outcome = outcome;
        effectsA[i].victim_id = victim_ship_id;

        if (outcome == SHOT_HIT && victim_ship_id >= 0) {
            AutonomousShip *target_ship = find_ship_by_id(fB, victim_ship_id);
            if (target_ship) {
                int d_idx = get_deck_index_for_point(target_ship, effectsA[i].target);
                bool sunk = ship_receive_hit(target_ship, d_idx);
                effectsA[i].sunk = sunk;
                j->stats.total_hits[0]++;
                if (sunk) {
                    target_ship->died_in_current_round = true;
                    j->stats.total_ships_sunk[0]++;
                }
            }
        }
    }

    for (int i = 0; i < countB; ++i) {
        effectsB[i].target = fB->round_salvo[i].target;
        effectsB[i].shooter_id = fB->round_salvo[i].shooter_ship_id;
        effectsB[i].outcome = SHOT_MISS;
        effectsB[i].victim_id = -1;
        effectsB[i].sunk = false;

        int victim_ship_id = -1;
        int deck_idx = -1;
        ShotOutcome outcome = board_apply_shot(&fA->own_board, effectsB[i].target, &victim_ship_id, &deck_idx);
        effectsB[i].outcome = outcome;
        effectsB[i].victim_id = victim_ship_id;

        if (outcome == SHOT_HIT && victim_ship_id >= 0) {
            AutonomousShip *target_ship = find_ship_by_id(fA, victim_ship_id);
            if (target_ship) {
                int d_idx = get_deck_index_for_point(target_ship, effectsB[i].target);
                bool sunk = ship_receive_hit(target_ship, d_idx);
                effectsB[i].sunk = sunk;
                j->stats.total_hits[1]++;
                if (sunk) {
                    target_ship->died_in_current_round = true;
                    j->stats.total_ships_sunk[1]++;
                }
            }
        }
    }

    for (int i = 0; i < countA; ++i) {
        log_event_shot_application(fA->name, effectsA[i].shooter_id, effectsA[i].target, effectsA[i].outcome, effectsA[i].victim_id);
        fleet_record_shot_result(fA, effectsA[i].target, effectsA[i].outcome);
        if (effectsA[i].outcome == SHOT_HIT && effectsA[i].victim_id >= 0) {
            AutonomousShip *victim = find_ship_by_id(fB, effectsA[i].victim_id);
            if (victim) {
                log_event_combat_capacity_change(victim);
                if (effectsA[i].sunk) {
                    log_event_ship_destroyed(victim);
                }
            }
        }
    }

    for (int i = 0; i < countB; ++i) {
        log_event_shot_application(fB->name, effectsB[i].shooter_id, effectsB[i].target, effectsB[i].outcome, effectsB[i].victim_id);
        fleet_record_shot_result(fB, effectsB[i].target, effectsB[i].outcome);
        if (effectsB[i].outcome == SHOT_HIT && effectsB[i].victim_id >= 0) {
            AutonomousShip *victim = find_ship_by_id(fA, effectsB[i].victim_id);
            if (victim) {
                log_event_combat_capacity_change(victim);
                if (effectsB[i].sunk) {
                    log_event_ship_destroyed(victim);
                }
            }
        }
    }

    log_event_fleets_summary(fA, fB);
}

bool judge_check_battle_completion(Judge *j, const Fleet *fA, const Fleet *fB) {
    if (!j || !fA || !fB) return true;

    bool aliveA = fleet_is_alive(fA);
    bool aliveB = fleet_is_alive(fB);

    if (!aliveA && !aliveB) {
        j->battle_over = true;
        j->stats.winner = -1;
        j->stats.termination_reason = "Взаимное уничтожение флотов в одном раунде (Ничья)";
        return true;
    }

    if (!aliveA) {
        j->battle_over = true;
        j->stats.winner = 1;
        j->stats.termination_reason = "Флот B полностью уничтожил все корабли противника (Победа флота B)";
        return true;
    }

    if (!aliveB) {
        j->battle_over = true;
        j->stats.winner = 0;
        j->stats.termination_reason = "Флот A полностью уничтожил все корабли противника (Победа флота A)";
        return true;
    }

    if (j->current_round >= j->max_rounds) {
        j->battle_over = true;
        int decksA = fleet_total_alive_decks(fA);
        int decksB = fleet_total_alive_decks(fB);
        if (decksA > decksB) {
            j->stats.winner = 0;
            j->stats.termination_reason = "Достигнут лимит раундов (Победа флота A по числу уцелевших палуб)";
        } else if (decksB > decksA) {
            j->stats.winner = 1;
            j->stats.termination_reason = "Достигнут лимит раундов (Победа флота B по числу уцелевших палуб)";
        } else {
            j->stats.winner = -1;
            j->stats.termination_reason = "Достигнут лимит раундов при равном числе уцелевших палуб (Боевая ничья)";
        }
        return true;
    }

    return false;
}
