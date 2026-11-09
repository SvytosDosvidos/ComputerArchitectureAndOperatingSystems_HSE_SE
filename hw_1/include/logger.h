#ifndef LOGGER_H
#define LOGGER_H

#include "common.h"
#include "fleet.h"
#include "judge.h"

void logger_init(const char *log_filepath, bool use_colors);
void logger_close(void);
int  logger_get_file_descriptor(void);

void log_msg(const char *fmt, ...);
void log_event(const char *category, const char *fmt, ...);

void log_event_ship_readiness(const Fleet *f);
void log_event_formed_targets(const Fleet *f);
void log_event_round_start(int round_num);
void log_event_shot_application(const char *attacker_name, int ship_id, Point target, ShotOutcome outcome, int victim_ship_id);
void log_event_combat_capacity_change(const AutonomousShip *ship);
void log_event_ship_destroyed(const AutonomousShip *ship);
void log_event_fleets_summary(const Fleet *fA, const Fleet *fB);

void logger_render_battlefields(const Fleet *fA, const Fleet *fB, bool reveal_ships);
void logger_render_final_report(const Judge *judge, const Fleet *fA, const Fleet *fB);

#endif
