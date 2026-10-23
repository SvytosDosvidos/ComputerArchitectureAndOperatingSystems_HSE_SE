#pragma once

#include "types.h"

void init_game(Game *game);
void destroy_game(Game *game);

void *run_ship(void *arg);
void *run_center(void *arg);
void *run_judge(void *arg);