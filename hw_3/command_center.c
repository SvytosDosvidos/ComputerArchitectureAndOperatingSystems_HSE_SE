#include "command_center.h"

void CCInit(CommandCenter *cc, int fleet, unsigned int seed) {
    cc->fleet = fleet;
    cc->seed = seed;
    memset(cc->grid_fired, 0, sizeof(cc->grid_fired));
    srand(seed + fleet * 50);

    const char *cc_fifo = (fleet == 1) ? FIFO_CC1 : FIFO_CC2;
    const char *sem_rnd = (fleet == 1) ? SEM_ROUND1 : SEM_ROUND2;
    const char *sem_slv = (fleet == 1) ? SEM_SALVO1 : SEM_SALVO2;

    cc->fd_cc = open(cc_fifo, O_RDWR);
    cc->fd_judge = open(FIFO_JUDGE, O_RDWR);
    cc->sem_rnd = sem_open(sem_rnd, 0);
    cc->sem_slv = sem_open(sem_slv, 0);

    int default_sizes[SHIPS_CNT] = {3, 2, 1};
    for (int i = 0; i < SHIPS_CNT; ++i) {
        cc->ship_decks[i] = default_sizes[i];

        char s_fifo[64], s_sem[64], s_done[64];
        GetShipFifo(s_fifo, sizeof(s_fifo), fleet, i);
        GetShipSem(s_sem, sizeof(s_sem), fleet, i);
        GetShipDoneSem(s_done, sizeof(s_done), fleet, i);

        cc->sem_s[i] = sem_open(s_sem, 0);
        cc->sem_sd[i] = sem_open(s_done, 0);
        cc->fd_s[i] = open(s_fifo, O_RDWR);
    }
}

void CCRun(CommandCenter *cc) {
    Message msg;
    while (1) {
        sem_wait(cc->sem_rnd);
        read(cc->fd_cc, &msg, sizeof(Message));

        if (msg.type == MSG_GAME_OVER) {
            Message stop_msg = {.type = MSG_GAME_OVER};
            for (int i = 0; i < SHIPS_CNT; ++i) {
                write(cc->fd_s[i], &stop_msg, sizeof(Message));
                sem_post(cc->sem_s[i]);
            }
            break;
        }

        for (int i = 0; i < SHIPS_CNT; ++i) {
            if (cc->ship_decks[i] > 0) {
                Message req = {.type = MSG_ROUND_START};
                write(cc->fd_s[i], &req, sizeof(Message));
                sem_post(cc->sem_s[i]);
            }
        }

        Message salvo;
        memset(&salvo, 0, sizeof(Message));
        salvo.type = MSG_FLEET_SALVO;
        salvo.fleet = cc->fleet;
        salvo.shot_cnt = 0;

        for (int i = 0; i < SHIPS_CNT; ++i) {
            if (cc->ship_decks[i] <= 0) continue;

            sem_wait(cc->sem_sd[i]);
            Message s_msg;
            read(cc->fd_cc, &s_msg, sizeof(Message));

            for (int k = 0; k < s_msg.shot_cnt; ++k) {
                Point pt = s_msg.shots[k];
                if (cc->grid_fired[pt.r][pt.c]) {
                    for (int r = 0; r < GRID_SZ; ++r) {
                        for (int c = 0; c < GRID_SZ; ++c) {
                            if (!cc->grid_fired[r][c]) {
                                pt.r = r;
                                pt.c = c;
                                goto found;
                            }
                        }
                    }
                }
            found:
                cc->grid_fired[pt.r][pt.c] = true;
                if (salvo.shot_cnt < MAX_SHOTS) {
                    salvo.shots[salvo.shot_cnt++] = pt;
                }
            }
        }

        write(cc->fd_judge, &salvo, sizeof(Message));
        sem_post(cc->sem_slv);

        Message res;
        read(cc->fd_cc, &res, sizeof(Message));
        for (int i = 0; i < SHIPS_CNT; ++i) {
            cc->ship_decks[i] = res.shots[i].r;
            Message upd = {
                .type = MSG_ROUND_RESULT,
                .hits_cnt = res.shots[i].c
            };
            write(cc->fd_s[i], &upd, sizeof(Message));
            sem_post(cc->sem_s[i]);
        }
    }
}

void CCClose(CommandCenter *cc) {
    for (int i = 0; i < SHIPS_CNT; ++i) {
        if (cc->fd_s[i] >= 0) close(cc->fd_s[i]);
        if (cc->sem_s[i] != SEM_FAILED) sem_close(cc->sem_s[i]);
        if (cc->sem_sd[i] != SEM_FAILED) sem_close(cc->sem_sd[i]);
    }
    if (cc->fd_cc >= 0) close(cc->fd_cc);
    if (cc->fd_judge >= 0) close(cc->fd_judge);
    if (cc->sem_rnd != SEM_FAILED) sem_close(cc->sem_rnd);
    if (cc->sem_slv != SEM_FAILED) sem_close(cc->sem_slv);
}
