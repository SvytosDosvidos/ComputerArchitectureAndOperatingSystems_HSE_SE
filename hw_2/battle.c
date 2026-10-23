#include "battle.h"
#include "utils.h"

static bool check_bounds(int ind_i, int ind_j, int h, int w) {
    return ind_i >= 0 && ind_i < h && ind_j >= 0 && ind_j < w;
}

static bool can_place(Game *game, int side, int start_i, int start_j, int len, int dir, bool check_near) {
    int h = game->cfg.h;
    int w = game->cfg.w;

    for (int k = 0; k < len; k++) {
        int cur_i = start_i + (dir == 0 ? k : 0);
        int cur_j = start_j + (dir == 1 ? k : 0);

        if (!check_bounds(cur_i, cur_j, h, w)) {
            return false;
        }
        if (game->field[side][cur_i][cur_j] != CELL_EMPTY) {
            return false;
        }
        if (check_near) {
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    int ni = cur_i + di;
                    int nj = cur_j + dj;
                    if (check_bounds(ni, nj, h, w) && game->field[side][ni][nj] != CELL_EMPTY) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static void place_ship(Game *game, int side, int ind, int len, unsigned int *seed) {
    int h = game->cfg.h;
    int w = game->cfg.w;
    int start_i = 0, start_j = 0, dir = 0;
    bool flag_placed = false;

    for (int step = 0; step < 1000 && !flag_placed; step++) {
        int r_i = get_rand(seed, h);
        int r_j = get_rand(seed, w);
        int r_d = get_rand(seed, 2);
        if (can_place(game, side, r_i, r_j, len, r_d, step < 600)) {
            start_i = r_i;
            start_j = r_j;
            dir = r_d;
            flag_placed = true;
        }
    }

    if (!flag_placed) {
        for (int i = 0; i < h && !flag_placed; i++) {
            for (int j = 0; j < w && !flag_placed; j++) {
                if (can_place(game, side, i, j, len, 1, false)) {
                    start_i = i;
                    start_j = j;
                    dir = 1;
                    flag_placed = true;
                }
            }
        }
    }

    Ship *ship = &game->ships[side][ind];
    ship->id = ind + 1;
    ship->side = side;
    ship->start_i = start_i;
    ship->start_j = start_j;
    ship->len = len;
    ship->dir = dir;
    ship->cnt_decks = len;
    ship->flag_active = 0;
    ship->seed = *seed ^ (unsigned int)((side + 1) * 100 + ind);
    ship->game = game;

    for (int k = 0; k < len; k++) {
        ship->deck_hit[k] = 0;
        int cur_i = start_i + (dir == 0 ? k : 0);
        int cur_j = start_j + (dir == 1 ? k : 0);
        game->field[side][cur_i][cur_j] = CELL_SHIP;
        game->map_id[side][cur_i][cur_j] = ind;
        game->map_deck[side][cur_i][cur_j] = k;
    }

    game->cnt_decks[side] += len;
}

void init_game(Game *game) {
    static const int ship_lens[MAX_SHIPS] = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};
    unsigned int seed = game->cfg.seed;

    pthread_mutex_init(&game->mtx_state, NULL);
    pthread_mutex_init(&game->mtx_print, NULL);

    for (int s = 0; s < 2; s++) {
        game->cnt_alive[s] = game->cfg.cnt_ships;
        game->cnt_decks[s] = 0;

        for (int i = 0; i < game->cfg.cnt_ships; i++) {
            int len = ship_lens[i % MAX_SHIPS];
            place_ship(game, s, i, len, &seed);
            sem_init(&game->sem_ship_start[s][i], 0, 0);
        }

        game->centers[s].side = s;
        game->centers[s].cnt_fixed = 0;
        game->centers[s].seed = seed ^ (unsigned int)(s * 555 + 7);
        game->centers[s].game = game;

        sem_init(&game->sem_ship_done[s], 0, 0);
        sem_init(&game->sem_center_start[s], 0, 0);
        sem_init(&game->sem_center_done[s], 0, 0);
    }
}

void destroy_game(Game *game) {
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < game->cfg.cnt_ships; i++) {
            sem_destroy(&game->sem_ship_start[s][i]);
        }
        sem_destroy(&game->sem_ship_done[s]);
        sem_destroy(&game->sem_center_start[s]);
        sem_destroy(&game->sem_center_done[s]);
    }
    pthread_mutex_destroy(&game->mtx_state);
    pthread_mutex_destroy(&game->mtx_print);
}

static void choose_target(Game *game, Ship *ship, int *res_i, int *res_j) {
    int side = ship->side;
    int enemy = 1 - side;
    int h = game->cfg.h;
    int w = game->cfg.w;

    if (game->cfg.mode_strat == 1) {
        int di[4] = {-1, 1, 0, 0};
        int dj[4] = {0, 0, -1, 1};
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (game->field[enemy][i][j] == CELL_HIT) {
                    for (int d = 0; d < 4; d++) {
                        int ni = i + di[d];
                        int nj = j + dj[d];
                        if (check_bounds(ni, nj, h, w) && game->used_shots[side][ni][nj] == 0) {
                            *res_i = ni;
                            *res_j = nj;
                            return;
                        }
                    }
                }
            }
        }
    }

    for (int step = 0; step < 100; step++) {
        int r_i = get_rand(&ship->seed, h);
        int r_j = get_rand(&ship->seed, w);
        if (game->used_shots[side][r_i][r_j] == 0 || game->cfg.flag_repeat) {
            *res_i = r_i;
            *res_j = r_j;
            return;
        }
    }

    *res_i = get_rand(&ship->seed, h);
    *res_j = get_rand(&ship->seed, w);
}

void *run_ship(void *arg) {
    Ship *ship = (Ship *)arg;
    Game *game = ship->game;
    int side = ship->side;
    int ind = ship->id - 1;

    while (true) {
        if (sem_wait(&game->sem_ship_start[side][ind]) != 0) {
            if (flag_stop || game->flag_end) break;
            continue;
        }
        if (game->flag_end) {
            break;
        }
        if (!ship->flag_active) {
            continue;
        }

        int cnt = (game->cfg.mode_shots == 0) ? ship->cnt_decks : 1;

        pthread_mutex_lock(&game->mtx_state);
        for (int k = 0; k < cnt && game->cnt_shots[side] < MAX_SHOTS; k++) {
            int ti = 0, tj = 0;
            choose_target(game, ship, &ti, &tj);

            int pos = game->cnt_shots[side]++;
            game->shots[side][pos].id_ship = ship->id;
            game->shots[side][pos].side = side;
            game->shots[side][pos].ind_i = ti;
            game->shots[side][pos].ind_j = tj;
        }
        pthread_mutex_unlock(&game->mtx_state);

        print_msg(game, "[Корабль #%d, Флот %c] Готов, палуб: %d/%d, выстрелов в залпе: %d\n",
                  ship->id, side == 0 ? 'A' : 'B', ship->cnt_decks, ship->len, cnt);

        sem_post(&game->sem_ship_done[side]);
    }
    return NULL;
}

static void coordinate_shots(Game *game, Center *center) {
    int side = center->side;
    int h = game->cfg.h;
    int w = game->cfg.w;
    int used_now[MAX_SIZE][MAX_SIZE];
    memset(used_now, 0, sizeof(used_now));

    int cnt_changed = 0;
    for (int k = 0; k < game->cnt_shots[side]; k++) {
        int cur_i = game->shots[side][k].ind_i;
        int cur_j = game->shots[side][k].ind_j;

        bool is_dup = (!game->cfg.flag_repeat && used_now[cur_i][cur_j]);
        bool is_old = (game->cfg.mode_center == 0 && game->used_shots[side][cur_i][cur_j]);

        if (is_dup || is_old) {
            bool found = false;
            for (int i = 0; i < h && !found; i++) {
                for (int j = 0; j < w && !found; j++) {
                    if (!used_now[i][j] && !game->used_shots[side][i][j]) {
                        cur_i = i;
                        cur_j = j;
                        game->shots[side][k].ind_i = cur_i;
                        game->shots[side][k].ind_j = cur_j;
                        cnt_changed++;
                        center->cnt_fixed++;
                        found = true;
                    }
                }
            }
        }

        used_now[cur_i][cur_j] = 1;
        game->used_shots[side][cur_i][cur_j] = 1;
    }

    print_msg(game, "[Командный центр %c] Цели согласованы: %d выстр. (заменено повторов: %d)\n",
              side == 0 ? 'A' : 'B', game->cnt_shots[side], cnt_changed);
}

void *run_center(void *arg) {
    Center *center = (Center *)arg;
    Game *game = center->game;
    int side = center->side;

    while (true) {
        if (sem_wait(&game->sem_center_start[side]) != 0) {
            if (flag_stop || game->flag_end) break;
            continue;
        }
        if (game->flag_end) {
            break;
        }

        for (int i = 0; i < game->cnt_active[side]; i++) {
            while (sem_wait(&game->sem_ship_done[side]) != 0) {
                if (flag_stop || game->flag_end) break;
            }
        }
        if (game->flag_end) {
            break;
        }

        pthread_mutex_lock(&game->mtx_state);
        coordinate_shots(game, center);
        pthread_mutex_unlock(&game->mtx_state);

        sem_post(&game->sem_center_done[side]);
    }
    return NULL;
}

static void apply_shot(Game *game, Shot s) {
    int side = s.side;
    int enemy = 1 - side;
    int i = s.ind_i;
    int j = s.ind_j;

    game->total_shots[side]++;
    int cell = game->field[enemy][i][j];

    if (cell == CELL_SHIP) {
        int id_s = game->map_id[enemy][i][j];
        int id_d = game->map_deck[enemy][i][j];
        Ship *target = &game->ships[enemy][id_s];

        if (target->deck_hit[id_d] == 0) {
            target->deck_hit[id_d] = 1;
            target->cnt_decks--;
            game->cnt_decks[enemy]--;
            game->field[enemy][i][j] = CELL_HIT;
            game->total_hits[side]++;

            print_msg(game, "[Попадание] Флот %c (Корабль #%d) -> (%d, %d): ранен Корабль #%d флота %c (палуб: %d/%d)\n",
                      side == 0 ? 'A' : 'B', s.id_ship, i, j,
                      target->id, enemy == 0 ? 'A' : 'B', target->cnt_decks, target->len);

            if (target->cnt_decks == 0) {
                game->cnt_alive[enemy]--;
                print_msg(game, "[Уничтожение] Корабль #%d флота %c УНИЧТОЖЕН!\n",
                          target->id, enemy == 0 ? 'A' : 'B');
            }
        }
    } else if (cell == CELL_EMPTY) {
        game->field[enemy][i][j] = CELL_MISS;
    }
}

static void stop_all_threads(Game *game) {
    game->flag_end = 1;
    for (int s = 0; s < 2; s++) {
        sem_post(&game->sem_center_start[s]);
        for (int i = 0; i < game->cfg.cnt_ships; i++) {
            sem_post(&game->sem_ship_start[s][i]);
            sem_post(&game->sem_ship_done[s]);
        }
    }
}

void *run_judge(void *arg) {
    Game *game = (Game *)arg;

    print_msg(game, "=== СТАРТ МОРСКОГО БОЯ АВТОНОМНЫХ КОРАБЛЕЙ ===\n");
    print_fields(game);

    for (int r = 1; r <= game->cfg.max_rounds; r++) {
        if (flag_stop) {
            print_msg(game, "\n[Сигнал] Бой прерван пользователем!\n");
            break;
        }

        game->cur_round = r;
        for (int s = 0; s < 2; s++) {
            game->cnt_shots[s] = 0;
            game->cnt_active[s] = 0;
            for (int i = 0; i < game->cfg.cnt_ships; i++) {
                if (game->ships[s][i].cnt_decks > 0) {
                    game->ships[s][i].flag_active = 1;
                    game->cnt_active[s]++;
                } else {
                    game->ships[s][i].flag_active = 0;
                }
            }
        }

        print_msg(game, "\n--- РАУНД %d (активных кораблей: A=%d, B=%d) ---\n",
                  r, game->cnt_active[0], game->cnt_active[1]);

        for (int s = 0; s < 2; s++) {
            sem_post(&game->sem_center_start[s]);
            for (int i = 0; i < game->cfg.cnt_ships; i++) {
                if (game->ships[s][i].flag_active) {
                    sem_post(&game->sem_ship_start[s][i]);
                }
            }
        }

        for (int s = 0; s < 2; s++) {
            while (sem_wait(&game->sem_center_done[s]) != 0) {
                if (flag_stop) break;
            }
        }
        if (flag_stop) {
            print_msg(game, "\n[Сигнал] Бой остановлен во время раунда!\n");
            break;
        }

        pthread_mutex_lock(&game->mtx_state);
        for (int s = 0; s < 2; s++) {
            for (int k = 0; k < game->cnt_shots[s]; k++) {
                apply_shot(game, game->shots[s][k]);
            }
        }
        pthread_mutex_unlock(&game->mtx_state);

        print_fields(game);

        if (game->cnt_alive[0] == 0 && game->cnt_alive[1] == 0) {
            print_msg(game, "\nИТОГ: Оба флота уничтожены одновременно! Ничья.\n");
            break;
        }
        if (game->cnt_alive[1] == 0) {
            print_msg(game, "\nИТОГ: Победа флота A! Флот B полностью уничтожен.\n");
            break;
        }
        if (game->cnt_alive[0] == 0) {
            print_msg(game, "\nИТОГ: Победа флота B! Флот A полностью уничтожен.\n");
            break;
        }
        if (r == game->cfg.max_rounds) {
            print_msg(game, "\nИТОГ: Достигнут лимит раундов (%d). Палуб A=%d, B=%d.\n",
                      r, game->cnt_decks[0], game->cnt_decks[1]);
        }

        if (game->cfg.delay_ms > 0) {
            usleep((useconds_t)game->cfg.delay_ms * 1000u);
        }
    }

    stop_all_threads(game);
    return NULL;
}