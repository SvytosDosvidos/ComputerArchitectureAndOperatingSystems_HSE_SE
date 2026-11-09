#include "ship.h"

void ShipInit(Ship *s, int fleet, int id, int size, unsigned int seed) {
    s->fleet = fleet;
    s->id = id;
    s->size = size;
    s->decks = size;
    s->seed = seed;
    memset(s->tried, 0, sizeof(s->tried));
    srand(seed + fleet * 100 + id * 10);

    char fifo_self[64], sem_name[64], sem_done[64];
    GetShipFifo(fifo_self, sizeof(fifo_self), fleet, id);
    GetShipSem(sem_name, sizeof(sem_name), fleet, id);
    GetShipDoneSem(sem_done, sizeof(sem_done), fleet, id);

    s->sem = sem_open(sem_name, 0);
    s->sem_done = sem_open(sem_done, 0);
    s->fd_self = open(fifo_self, O_RDWR);

    char fifo_cc[64];
    snprintf(fifo_cc, sizeof(fifo_cc), fleet == 1 ? FIFO_CC1 : FIFO_CC2);
    s->fd_cc = open(fifo_cc, O_RDWR);

    if (s->fd_self < 0 || s->fd_cc < 0 || s->sem == SEM_FAILED || s->sem_done == SEM_FAILED) {
        dprintf(2, "[Ship %d:%d] Init error\n", fleet, id);
        exit(1);
    }
}

void ShipRun(Ship *s) {
    Message msg;
    while (1) {
        sem_wait(s->sem);
        read(s->fd_self, &msg, sizeof(Message));

        if (msg.type == MSG_GAME_OVER) {
            break;
        }

        if (msg.type == MSG_ROUND_RESULT) {
            s->decks -= msg.hits_cnt;
            if (s->decks < 0) s->decks = 0;
            continue;
        }

        Message out_msg;
        memset(&out_msg, 0, sizeof(Message));
        out_msg.type = MSG_SHIP_SHOTS;
        out_msg.fleet = s->fleet;
        out_msg.ship_id = s->id;
        out_msg.shot_cnt = s->decks;

        for (int i = 0; i < s->decks; ++i) {
            int r, c, tries = 0;
            do {
                r = rand() % GRID_SZ;
                c = rand() % GRID_SZ;
                tries++;
            } while (s->tried[r][c] && tries < 200);

            s->tried[r][c] = true;
            out_msg.shots[i].r = r;
            out_msg.shots[i].c = c;
        }

        write(s->fd_cc, &out_msg, sizeof(Message));
        sem_post(s->sem_done);
    }
}

void ShipClose(Ship *s) {
    if (s->fd_self >= 0) close(s->fd_self);
    if (s->fd_cc >= 0) close(s->fd_cc);
    if (s->sem != SEM_FAILED) sem_close(s->sem);
    if (s->sem_done != SEM_FAILED) sem_close(s->sem_done);
}
