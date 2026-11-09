#ifndef JUDGE_H
#define JUDGE_H

#include "protocol.h"

typedef struct {
    int id;
    int size;
    int decks;
    Point pos[4];
} ShipInfo;

typedef struct {
    char board[GRID_SZ][GRID_SZ];
    ShipInfo ships[SHIPS_CNT];
} FleetState;

typedef struct {
    int max_rounds;
    int delay_ms;
    unsigned int seed;
    int fd_judge;
    int fd_cc1;
    int fd_cc2;
    int log_fd;
    sem_t *sem_r1;
    sem_t *sem_r2;
    sem_t *sem_s1;
    sem_t *sem_s2;
    FleetState f1;
    FleetState f2;
} Judge;

void JudgeCleanupIPC(void);
void JudgeInit(Judge *j, int max_rounds, int delay_ms, unsigned int seed);
void JudgeRun(Judge *j);
void JudgeClose(Judge *j);

#endif
