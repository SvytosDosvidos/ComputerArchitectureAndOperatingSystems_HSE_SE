#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>

#define GRID_SZ 8
#define SHIPS_CNT 3
#define MAX_SHOTS 8
#define LOG_FILE "battle.log"

#define FIFO_JUDGE "/tmp/sb22_fifo_judge"
#define FIFO_CC1   "/tmp/sb22_fifo_cc1"
#define FIFO_CC2   "/tmp/sb22_fifo_cc2"

#define SEM_ROUND1 "/sb22_sem_rnd1"
#define SEM_ROUND2 "/sb22_sem_rnd2"
#define SEM_SALVO1 "/sb22_sem_slv1"
#define SEM_SALVO2 "/sb22_sem_slv2"

typedef struct {
    int r;
    int c;
} Point;

typedef enum {
    MSG_ROUND_START,
    MSG_SHIP_SHOTS,
    MSG_FLEET_SALVO,
    MSG_ROUND_RESULT,
    MSG_GAME_OVER
} MsgType;

typedef struct {
    MsgType type;
    int fleet;
    int ship_id;
    int round_num;
    int shot_cnt;
    Point shots[MAX_SHOTS];
    int hits_cnt;
    bool is_sunk;
    int winner;
} Message;

static inline void GetShipFifo(char *buf, size_t sz, int fleet, int id) {
    snprintf(buf, sz, "/tmp/sb22_fifo_s_%d_%d", fleet, id);
}

static inline void GetShipSem(char *buf, size_t sz, int fleet, int id) {
    snprintf(buf, sz, "/sb22_sem_s_%d_%d", fleet, id);
}

static inline void GetShipDoneSem(char *buf, size_t sz, int fleet, int id) {
    snprintf(buf, sz, "/sb22_sem_sd_%d_%d", fleet, id);
}

#endif
