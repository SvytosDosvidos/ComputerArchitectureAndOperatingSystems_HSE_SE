#pragma once

#include "types.h"

int get_rand(unsigned int *seed, int mod);
void init_config(Config *cfg);
bool read_config(Config *cfg, const char *path);
void parse_args(int argc, char **argv, Config *cfg, char *log_path);

void print_msg(Game *game, const char *fmt, ...);
void print_fields(Game *game);
void print_stats(Game *game);
