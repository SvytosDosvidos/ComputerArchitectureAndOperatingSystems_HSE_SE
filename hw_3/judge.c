#include "judge.h"

void JudgeCleanupIPC(void) {
    unlink(FIFO_JUDGE);
    unlink(FIFO_CC1);
    unlink(FIFO_CC2);

    sem_unlink(SEM_ROUND1);
    sem_unlink(SEM_ROUND2);
    sem_unlink(SEM_SALVO1);
    sem_unlink(SEM_SALVO2);

    for (int f = 1; f <= 2; ++f) {
        for (int s = 0; s < SHIPS_CNT; ++s) {
            char f_name[64], s_name[64], sd_name[64];
            GetShipFifo(f_name, sizeof(f_name), f, s);
            GetShipSem(s_name, sizeof(s_name), f, s);
            GetShipDoneSem(sd_name, sizeof(sd_name), f, s);
            unlink(f_name);
            sem_unlink(s_name);
            sem_unlink(sd_name);
        }
    }
}

static void InitFleet(FleetState *fs, int fleet_id) {
    memset(fs->board, '.', sizeof(fs->board));
    int sizes[SHIPS_CNT] = {3, 2, 1};
    int rows[SHIPS_CNT] = {fleet_id == 1 ? 1 : 2, fleet_id == 1 ? 4 : 5, 7};
    int cols[SHIPS_CNT] = {1, 3, 5};

    for (int i = 0; i < SHIPS_CNT; ++i) {
        fs->ships[i].id = i;
        fs->ships[i].size = sizes[i];
        fs->ships[i].decks = sizes[i];
        for (int d = 0; d < sizes[i]; ++d) {
            int r = rows[i];
            int c = cols[i] + d;
            fs->ships[i].pos[d].r = r;
            fs->ships[i].pos[d].c = c;
            fs->board[r][c] = 'S';
        }
    }
}

static void DrawWorld(FleetState *f1, FleetState *f2, int rnd) {
    dprintf(1, "\033[2J\033[0;0f");
    dprintf(1, "\x1b[1;33m=== ROUND %d ===\x1b[0m\n", rnd);
    dprintf(1, "      FLEET 1                      FLEET 2\n");
    dprintf(1, "    0 1 2 3 4 5 6 7              0 1 2 3 4 5 6 7\n");

    for (int r = 0; r < GRID_SZ; ++r) {
        dprintf(1, "  %d ", r);
        for (int c = 0; c < GRID_SZ; ++c) {
            char ch = f1->board[r][c];
            if (ch == 'S') dprintf(1, "\x1b[1;32mS\x1b[0m ");
            else if (ch == 'X') dprintf(1, "\x1b[1;31mX\x1b[0m ");
            else if (ch == 'o') dprintf(1, "\x1b[1;30mo\x1b[0m ");
            else dprintf(1, ". ");
        }
        dprintf(1, "         %d ", r);
        for (int c = 0; c < GRID_SZ; ++c) {
            char ch = f2->board[r][c];
            if (ch == 'S') dprintf(1, "\x1b[1;32mS\x1b[0m ");
            else if (ch == 'X') dprintf(1, "\x1b[1;31mX\x1b[0m ");
            else if (ch == 'o') dprintf(1, "\x1b[1;30mo\x1b[0m ");
            else dprintf(1, ". ");
        }
        dprintf(1, "\n");
    }
}

void JudgeInit(Judge *j, int max_rounds, int delay_ms, unsigned int seed) {
    j->max_rounds = max_rounds;
    j->delay_ms = delay_ms;
    j->seed = seed;

    JudgeCleanupIPC();

    mkfifo(FIFO_JUDGE, 0666);
    mkfifo(FIFO_CC1, 0666);
    mkfifo(FIFO_CC2, 0666);

    j->sem_r1 = sem_open(SEM_ROUND1, O_CREAT, 0666, 0);
    j->sem_r2 = sem_open(SEM_ROUND2, O_CREAT, 0666, 0);
    j->sem_s1 = sem_open(SEM_SALVO1, O_CREAT, 0666, 0);
    j->sem_s2 = sem_open(SEM_SALVO2, O_CREAT, 0666, 0);

    for (int f = 1; f <= 2; ++f) {
        for (int s = 0; s < SHIPS_CNT; ++s) {
            char f_name[64], s_name[64], sd_name[64];
            GetShipFifo(f_name, sizeof(f_name), f, s);
            GetShipSem(s_name, sizeof(s_name), f, s);
            GetShipDoneSem(sd_name, sizeof(sd_name), f, s);
            mkfifo(f_name, 0666);
            sem_open(s_name, O_CREAT, 0666, 0);
            sem_open(sd_name, O_CREAT, 0666, 0);
        }
    }

    j->fd_judge = open(FIFO_JUDGE, O_RDWR);
    j->fd_cc1 = open(FIFO_CC1, O_RDWR);
    j->fd_cc2 = open(FIFO_CC2, O_RDWR);
    j->log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    InitFleet(&j->f1, 1);
    InitFleet(&j->f2, 2);
}

void JudgeRun(Judge *j) {
    dprintf(j->log_fd, "=== BATTLE SIMULATION STARTED ===\n");
    int round_num = 1;
    int winner = 0;

    while (round_num <= j->max_rounds) {
        Message r_start = {.type = MSG_ROUND_START, .round_num = round_num};
        write(j->fd_cc1, &r_start, sizeof(Message));
        write(j->fd_cc2, &r_start, sizeof(Message));
        sem_post(j->sem_r1);
        sem_post(j->sem_r2);

        sem_wait(j->sem_s1);
        sem_wait(j->sem_s2);

        Message slv1, slv2;
        read(j->fd_judge, &slv1, sizeof(Message));
        read(j->fd_judge, &slv2, sizeof(Message));

        if (slv1.fleet == 2) {
            Message tmp = slv1;
            slv1 = slv2;
            slv2 = tmp;
        }

        dprintf(j->log_fd, "[Round %d] Fleet 1 shots: %d, Fleet 2 shots: %d\n",
                round_num, slv1.shot_cnt, slv2.shot_cnt);

        int hits_on_f2[SHIPS_CNT] = {0};
        for (int i = 0; i < slv1.shot_cnt; ++i) {
            int r = slv1.shots[i].r;
            int c = slv1.shots[i].c;
            if (j->f2.board[r][c] == 'S') {
                j->f2.board[r][c] = 'X';
                for (int s = 0; s < SHIPS_CNT; ++s) {
                    for (int d = 0; d < j->f2.ships[s].size; ++d) {
                        if (j->f2.ships[s].pos[d].r == r && j->f2.ships[s].pos[d].c == c) {
                            j->f2.ships[s].decks--;
                            hits_on_f2[s]++;
                            dprintf(j->log_fd, "  Fleet 1 HIT Fleet 2 ship #%d at (%d,%d)\n", s, r, c);
                        }
                    }
                }
            } else if (j->f2.board[r][c] == '.') {
                j->f2.board[r][c] = 'o';
            }
        }

        int hits_on_f1[SHIPS_CNT] = {0};
        for (int i = 0; i < slv2.shot_cnt; ++i) {
            int r = slv2.shots[i].r;
            int c = slv2.shots[i].c;
            if (j->f1.board[r][c] == 'S') {
                j->f1.board[r][c] = 'X';
                for (int s = 0; s < SHIPS_CNT; ++s) {
                    for (int d = 0; d < j->f1.ships[s].size; ++d) {
                        if (j->f1.ships[s].pos[d].r == r && j->f1.ships[s].pos[d].c == c) {
                            j->f1.ships[s].decks--;
                            hits_on_f1[s]++;
                            dprintf(j->log_fd, "  Fleet 2 HIT Fleet 1 ship #%d at (%d,%d)\n", s, r, c);
                        }
                    }
                }
            } else if (j->f1.board[r][c] == '.') {
                j->f1.board[r][c] = 'o';
            }
        }

        DrawWorld(&j->f1, &j->f2, round_num);

        int f1_alive = 0, f2_alive = 0;
        for (int i = 0; i < SHIPS_CNT; ++i) {
            if (j->f1.ships[i].decks > 0) f1_alive += j->f1.ships[i].decks;
            if (j->f2.ships[i].decks > 0) f2_alive += j->f2.ships[i].decks;
        }

        dprintf(1, "Intact decks: \x1b[1;32mFleet 1 = %d\x1b[0m | \x1b[1;31mFleet 2 = %d\x1b[0m\n",
                f1_alive, f2_alive);

        Message rep1 = {.type = MSG_ROUND_RESULT};
        Message rep2 = {.type = MSG_ROUND_RESULT};
        for (int i = 0; i < SHIPS_CNT; ++i) {
            rep1.shots[i].r = j->f1.ships[i].decks;
            rep1.shots[i].c = hits_on_f1[i];
            rep2.shots[i].r = j->f2.ships[i].decks;
            rep2.shots[i].c = hits_on_f2[i];
        }
        write(j->fd_cc1, &rep1, sizeof(Message));
        write(j->fd_cc2, &rep2, sizeof(Message));

        if (f1_alive == 0 || f2_alive == 0) {
            if (f1_alive == 0 && f2_alive == 0) winner = 3;
            else if (f2_alive == 0) winner = 1;
            else winner = 2;
            break;
        }

        usleep(j->delay_ms * 1000);
        round_num++;
    }

    if (winner == 0) {
        int f1_alive = 0, f2_alive = 0;
        for (int i = 0; i < SHIPS_CNT; ++i) {
            f1_alive += j->f1.ships[i].decks;
            f2_alive += j->f2.ships[i].decks;
        }
        if (f1_alive > f2_alive) winner = 1;
        else if (f2_alive > f1_alive) winner = 2;
        else winner = 3;
    }

    dprintf(1, "\n\x1b[1;33m=== GAME OVER ===\x1b[0m\n");
    if (winner == 1) dprintf(1, "\x1b[1;32mWinner: FLEET 1\x1b[0m\n");
    else if (winner == 2) dprintf(1, "\x1b[1;31mWinner: FLEET 2\x1b[0m\n");
    else dprintf(1, "\x1b[1;37mResult: DRAW\x1b[0m\n");

    dprintf(j->log_fd, "GAME OVER. Winner = %d\n", winner);

    Message term = {.type = MSG_GAME_OVER};
    write(j->fd_cc1, &term, sizeof(Message));
    write(j->fd_cc2, &term, sizeof(Message));
    sem_post(j->sem_r1);
    sem_post(j->sem_r2);
}

void JudgeClose(Judge *j) {
    close(j->fd_judge);
    close(j->fd_cc1);
    close(j->fd_cc2);
    close(j->log_fd);
    sem_close(j->sem_r1);
    sem_close(j->sem_r2);
    sem_close(j->sem_s1);
    sem_close(j->sem_s2);
    JudgeCleanupIPC();
}
