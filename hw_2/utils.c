#include "utils.h"

int get_rand(unsigned int *seed, int mod) {
    if (mod <= 1) {
        return 0;
    }
    *seed = (*seed * 1103515245u + 12345u) & 0x7fffffffu;
    return (int)(*seed % (unsigned int)mod);
}

void init_config(Config *cfg) {
    cfg->h = 10;
    cfg->w = 10;
    cfg->cnt_ships = 6;
    cfg->max_rounds = 30;
    cfg->mode_shots = 0;
    cfg->mode_strat = 1;
    cfg->mode_center = 0;
    cfg->flag_repeat = 0;
    cfg->delay_ms = 200;
    cfg->seed = (unsigned int)time(NULL);
}

static void check_config(Config *cfg) {
    if (cfg->h < 5) cfg->h = 5;
    if (cfg->h > MAX_SIZE) cfg->h = MAX_SIZE;
    if (cfg->w < 5) cfg->w = 5;
    if (cfg->w > MAX_SIZE) cfg->w = MAX_SIZE;
    if (cfg->cnt_ships < 1) cfg->cnt_ships = 1;
    if (cfg->cnt_ships > MAX_SHIPS) cfg->cnt_ships = MAX_SHIPS;
    if (cfg->max_rounds < 1) cfg->max_rounds = 1;
    if (cfg->delay_ms < 0) cfg->delay_ms = 0;
}

bool read_config(Config *cfg, const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    char buf[2048];
    ssize_t sz = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (sz <= 0) {
        return false;
    }
    buf[sz] = '\0';

    char *line = strtok(buf, "\r\n");
    while (line != NULL) {
        char key[64];
        int val = 0;
        if (line[0] != '#' && sscanf(line, "%63s = %d", key, &val) == 2) {
            if (strcmp(key, "h") == 0) cfg->h = val;
            else if (strcmp(key, "w") == 0) cfg->w = val;
            else if (strcmp(key, "cnt_ships") == 0) cfg->cnt_ships = val;
            else if (strcmp(key, "max_rounds") == 0) cfg->max_rounds = val;
            else if (strcmp(key, "mode_shots") == 0) cfg->mode_shots = val;
            else if (strcmp(key, "mode_strat") == 0) cfg->mode_strat = val;
            else if (strcmp(key, "mode_center") == 0) cfg->mode_center = val;
            else if (strcmp(key, "flag_repeat") == 0) cfg->flag_repeat = val;
            else if (strcmp(key, "delay_ms") == 0) cfg->delay_ms = val;
            else if (strcmp(key, "seed") == 0) cfg->seed = (unsigned int)val;
        }
        line = strtok(NULL, "\r\n");
    }

    check_config(cfg);
    return true;
}

void parse_args(int argc, char **argv, Config *cfg, char *log_path) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            if (!read_config(cfg, argv[++i])) {
                dprintf(2, "Error reading config file\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            cfg->h = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            cfg->w = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            cfg->cnt_ships = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            cfg->max_rounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            cfg->seed = (unsigned int)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            cfg->delay_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            strcpy(log_path, argv[++i]);
        }
    }
    check_config(cfg);
}

void print_msg(Game *game, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    pthread_mutex_lock(&game->mtx_print);
    dprintf(1, "%s", buf);
    if (game->fd_log >= 0) {
        dprintf(game->fd_log, "%s", buf);
    }
    pthread_mutex_unlock(&game->mtx_print);
}

static char get_cell_char(int cell) {
    if (cell == CELL_SHIP) return 'O';
    if (cell == CELL_HIT) return 'X';
    if (cell == CELL_MISS) return '*';
    return '.';
}

void print_fields(Game *game) {
    char buf[4096];
    int pos = 0;
    int h = game->cfg.h;
    int w = game->cfg.w;

    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "\nПоле A (кораблей: %d, палуб: %d)       Поле B (кораблей: %d, палуб: %d)\n   ",
                    game->cnt_alive[0], game->cnt_decks[0],
                    game->cnt_alive[1], game->cnt_decks[1]);

    for (int j = 0; j < w; j++) pos += snprintf(buf + pos, sizeof(buf) - pos, "%2d", j);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "      ");
    for (int j = 0; j < w; j++) pos += snprintf(buf + pos, sizeof(buf) - pos, "%2d", j);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "\n");

    for (int i = 0; i < h; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%2d ", i);
        for (int j = 0; j < w; j++) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, " %c", get_cell_char(game->field[0][i][j]));
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "   %2d ", i);
        for (int j = 0; j < w; j++) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, " %c", get_cell_char(game->field[1][i][j]));
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "\n");
    }

    print_msg(game, "%s", buf);
}

void print_stats(Game *game) {
    print_msg(game, "\n================ ИТОГОВАЯ СТАТИСТИКА ================\n");
    print_msg(game, "Раундов пройдено: %d\n", game->cur_round);
    for (int s = 0; s < 2; s++) {
        char side_ch = (s == 0) ? 'A' : 'B';
        print_msg(game,
                  "Флот %c: живых кораблей = %d/%d, целых палуб = %d, выстрелов = %d, попаданий = %d, скоординировано КЦ = %d\n",
                  side_ch, game->cnt_alive[s], game->cfg.cnt_ships,
                  game->cnt_decks[s], game->total_shots[s],
                  game->total_hits[s], game->centers[s].cnt_fixed);
    }
    print_msg(game, "=====================================================\n");
}