#ifndef COMMAND_CENTER_H
#define COMMAND_CENTER_H

#include "protocol.h"

typedef struct {
    int fleet;
    unsigned int seed;
    int fd_cc;
    int fd_judge;
    sem_t *sem_rnd;
    sem_t *sem_slv;
    sem_t *sem_s[SHIPS_CNT];
    sem_t *sem_sd[SHIPS_CNT];
    int fd_s[SHIPS_CNT];
    int ship_decks[SHIPS_CNT];
    bool grid_fired[GRID_SZ][GRID_SZ];
} CommandCenter;

void CCInit(CommandCenter *cc, int fleet, unsigned int seed);
void CCRun(CommandCenter *cc);
void CCClose(CommandCenter *cc);

#endif
