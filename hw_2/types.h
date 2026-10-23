#pragma once

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum {
    MAX_SIZE = 15,
    MAX_SHIPS = 10,
    MAX_SHOTS = 40,
    CELL_EMPTY = 0,
    CELL_SHIP = 1,
    CELL_HIT = 2,
    CELL_MISS = 3
};

struct Game;

typedef struct {
    int id_ship;
    int side;
    int ind_i;
    int ind_j;
} Shot;

typedef struct {
    int id;
    int side;
    int start_i;
    int start_j;
    int len;
    int dir;
    int cnt_decks;
    int deck_hit[4];
    int flag_active;
    unsigned int seed;
    pthread_t thread;
    struct Game *game;
} Ship;

typedef struct {
    int side;
    int cnt_fixed;
    unsigned int seed;
    pthread_t thread;
    struct Game *game;
} Center;

typedef struct {
    int h;
    int w;
    int cnt_ships;
    int max_rounds;
    int mode_shots;
    int mode_strat;
    int mode_center;
    int flag_repeat;
    int delay_ms;
    unsigned int seed;
} Config;

typedef struct Game {
    Config cfg;
    int field[2][MAX_SIZE][MAX_SIZE];
    int map_id[2][MAX_SIZE][MAX_SIZE];
    int map_deck[2][MAX_SIZE][MAX_SIZE];
    int used_shots[2][MAX_SIZE][MAX_SIZE];

    Ship ships[2][MAX_SHIPS];
    Center centers[2];
    pthread_t judge_thread;

    Shot shots[2][MAX_SHOTS];
    int cnt_shots[2];

    int cnt_alive[2];
    int cnt_decks[2];
    int cnt_active[2];
    int total_shots[2];
    int total_hits[2];

    int cur_round;
    int flag_end;
    int fd_log;

    pthread_mutex_t mtx_state;
    pthread_mutex_t mtx_print;

    sem_t sem_ship_start[2][MAX_SHIPS];
    sem_t sem_ship_done[2];
    sem_t sem_center_start[2];
    sem_t sem_center_done[2];
} Game;

extern volatile sig_atomic_t flag_stop;