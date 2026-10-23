#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

typedef struct {
    int board_width;
    int board_height;
    int ship_counts[5];
    int total_ships;
    ShotRule shot_rule;
    CoordinationMode coord_mode_A;
    CoordinationMode coord_mode_B;
    TargetStrategy strategy_A;
    TargetStrategy strategy_B;
    bool allow_repeat_targets;
    int max_rounds;
    int step_delay_ms;
    unsigned int random_seed;
    bool seed_provided;
    bool use_colors;
    bool reveal_enemy_ships;
    char log_filename[256];
    char config_filename[256];
} SimulationConfig;

void config_set_defaults(SimulationConfig *cfg);
int  config_parse_args(SimulationConfig *cfg, int argc, char **argv);
int  config_load_file(SimulationConfig *cfg, const char *filepath);
void config_print_summary(const SimulationConfig *cfg);

#endif
