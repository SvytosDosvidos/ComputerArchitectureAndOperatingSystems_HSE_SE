#ifndef SHIP_H
#define SHIP_H

#include "protocol.h"

typedef struct {
    int fleet;
    int id;
    int size;
    int decks;
    unsigned int seed;
    int fd_self;
    int fd_cc;
    sem_t *sem;
    sem_t *sem_done;
    bool tried[GRID_SZ][GRID_SZ];
} Ship;

void ShipInit(Ship *s, int fleet, int id, int size, unsigned int seed);
void ShipRun(Ship *s);
void ShipClose(Ship *s);

#endif
