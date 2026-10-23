#define _POSIX_C_SOURCE 200809L
#include "../include/config.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void config_set_defaults(SimulationConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(SimulationConfig));

    cfg->board_width = DEFAULT_BOARD_WIDTH;
    cfg->board_height = DEFAULT_BOARD_HEIGHT;

    cfg->ship_counts[4] = 1;
    cfg->ship_counts[3] = 2;
    cfg->ship_counts[2] = 3;
    cfg->ship_counts[1] = 4;
    cfg->total_ships = 10;

    cfg->shot_rule = SHOT_RULE_DECKS_ALIVE;
    cfg->coord_mode_A = COORD_DECONFLICT;
    cfg->coord_mode_B = COORD_NONE;
    cfg->strategy_A = STRATEGY_HUNT_TARGET;
    cfg->strategy_B = STRATEGY_RANDOM;
    cfg->allow_repeat_targets = false;
    cfg->max_rounds = DEFAULT_MAX_ROUNDS;
    cfg->step_delay_ms = DEFAULT_STEP_DELAY_MS;
    cfg->random_seed = (unsigned int)time(NULL);
    cfg->seed_provided = false;
    cfg->use_colors = true;
    cfg->reveal_enemy_ships = true;
    strncpy(cfg->log_filename, "battle_report.log", sizeof(cfg->log_filename) - 1);
}

static void print_help(const char *prog_name) {
    dprintf(STDOUT_FILENO,
        "Использование: %s [опции]\n\n"
        "Опции:\n"
        "  -c, --config <file>        Загрузить конфигурацию из файла (POSIX open/read)\n"
        "  -w, --width <int>          Ширина морских полей (от 5 до 30, по умолчанию: 10)\n"
        "  -h, --height <int>         Высота морских полей (от 5 до 30, по умолчанию: 10)\n"
        "  -r, --rounds <int>         Предельное число раундов (по умолчанию: 100)\n"
        "  -d, --delay <ms>           Задержка между раундами в миллисекундах (по умолчанию: 250)\n"
        "  -s, --seed <uint>          Начальное значение ГСЧ для воспроизводимости\n"
        "  -l, --log <file>           Путь к лог-файлу (по умолчанию: battle_report.log)\n"
        "      --rule <alive|fixed1|max2> Правило числа выстрелов в залпе\n"
        "      --coord-a <none|deconflict> Режим координации командного центра флота A\n"
        "      --coord-b <none|deconflict> Режим координации командного центра флота B\n"
        "      --strat-a <random|hunt|checker> Стратегия кораблей флота A\n"
        "      --strat-b <random|hunt|checker> Стратегия кораблей флота B\n"
        "      --allow-repeat         Разрешить повторные выстрелы по уже пораженным целям\n"
        "      --hide-ships           Скрыть расположение непораженных палуб в консоли\n"
        "      --no-color             Отключить ANSI цветовое оформление\n"
        "      --help                 Показать эту справку\n\n",
        prog_name);
}

int config_load_file(SimulationConfig *cfg, const char *filepath) {
    if (!cfg || !filepath) return -1;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        dprintf(STDERR_FILENO, "[ОШИБКА] Не удалось открыть конфигурационный файл '%s'\n", filepath);
        return -1;
    }

    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0) {
        dprintf(STDERR_FILENO, "[ОШИБКА] Конфигурационный файл пуст или ошибка чтения\n");
        return -1;
    }
    buffer[bytes_read] = '\0';

    char *line = strtok(buffer, "\r\n");
    while (line) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line != '#' && *line != '\0') {
            char key[64] = {0};
            char val[128] = {0};
            char *eq = strchr(line, '=');
            if (eq) {
                size_t klen = eq - line;
                if (klen < sizeof(key)) {
                    strncpy(key, line, klen);
                    key[klen] = '\0';
                    strncpy(val, eq + 1, sizeof(val) - 1);

                    if (strcmp(key, "width") == 0) cfg->board_width = atoi(val);
                    else if (strcmp(key, "height") == 0) cfg->board_height = atoi(val);
                    else if (strcmp(key, "rounds") == 0) cfg->max_rounds = atoi(val);
                    else if (strcmp(key, "delay_ms") == 0) cfg->step_delay_ms = atoi(val);
                    else if (strcmp(key, "seed") == 0) {
                        cfg->random_seed = (unsigned int)strtoul(val, NULL, 10);
                        cfg->seed_provided = true;
                    }
                    else if (strcmp(key, "log_file") == 0) strncpy(cfg->log_filename, val, sizeof(cfg->log_filename) - 1);
                    else if (strcmp(key, "coord_a") == 0) cfg->coord_mode_A = (strcmp(val, "deconflict") == 0) ? COORD_DECONFLICT : COORD_NONE;
                    else if (strcmp(key, "coord_b") == 0) cfg->coord_mode_B = (strcmp(val, "deconflict") == 0) ? COORD_DECONFLICT : COORD_NONE;
                    else if (strcmp(key, "strat_a") == 0) {
                        if (strcmp(val, "hunt") == 0) cfg->strategy_A = STRATEGY_HUNT_TARGET;
                        else if (strcmp(val, "checker") == 0) cfg->strategy_A = STRATEGY_CHECKERBOARD;
                        else cfg->strategy_A = STRATEGY_RANDOM;
                    }
                    else if (strcmp(key, "strat_b") == 0) {
                        if (strcmp(val, "hunt") == 0) cfg->strategy_B = STRATEGY_HUNT_TARGET;
                        else if (strcmp(val, "checker") == 0) cfg->strategy_B = STRATEGY_CHECKERBOARD;
                        else cfg->strategy_B = STRATEGY_RANDOM;
                    }
                    else if (strcmp(key, "allow_repeat") == 0) cfg->allow_repeat_targets = (atoi(val) != 0);
                    else if (strcmp(key, "ship_4") == 0) cfg->ship_counts[4] = atoi(val);
                    else if (strcmp(key, "ship_3") == 0) cfg->ship_counts[3] = atoi(val);
                    else if (strcmp(key, "ship_2") == 0) cfg->ship_counts[2] = atoi(val);
                    else if (strcmp(key, "ship_1") == 0) cfg->ship_counts[1] = atoi(val);
                }
            }
        }
        line = strtok(NULL, "\r\n");
    }

    cfg->total_ships = cfg->ship_counts[1] + cfg->ship_counts[2] + cfg->ship_counts[3] + cfg->ship_counts[4];
    return 0;
}

int config_parse_args(SimulationConfig *cfg, int argc, char **argv) {
    if (!cfg || argc <= 0) return 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            exit(0);
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) && i + 1 < argc) {
            strncpy(cfg->config_filename, argv[++i], sizeof(cfg->config_filename) - 1);
            config_load_file(cfg, cfg->config_filename);
        } else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) && i + 1 < argc) {
            cfg->board_width = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--height") == 0) && i + 1 < argc) {
            cfg->board_height = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rounds") == 0) && i + 1 < argc) {
            cfg->max_rounds = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--delay") == 0) && i + 1 < argc) {
            cfg->step_delay_ms = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seed") == 0) && i + 1 < argc) {
            cfg->random_seed = (unsigned int)strtoul(argv[++i], NULL, 10);
            cfg->seed_provided = true;
        } else if ((strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log") == 0) && i + 1 < argc) {
            strncpy(cfg->log_filename, argv[++i], sizeof(cfg->log_filename) - 1);
        } else if (strcmp(argv[i], "--allow-repeat") == 0) {
            cfg->allow_repeat_targets = true;
        } else if (strcmp(argv[i], "--hide-ships") == 0) {
            cfg->reveal_enemy_ships = false;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            cfg->use_colors = false;
        } else if (strcmp(argv[i], "--rule") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "fixed1") == 0) cfg->shot_rule = SHOT_RULE_FIXED_ONE;
            else if (strcmp(argv[i], "max2") == 0) cfg->shot_rule = SHOT_RULE_MAX_TWO;
            else cfg->shot_rule = SHOT_RULE_DECKS_ALIVE;
        } else if (strcmp(argv[i], "--coord-a") == 0 && i + 1 < argc) {
            i++;
            cfg->coord_mode_A = (strcmp(argv[i], "deconflict") == 0) ? COORD_DECONFLICT : COORD_NONE;
        } else if (strcmp(argv[i], "--coord-b") == 0 && i + 1 < argc) {
            i++;
            cfg->coord_mode_B = (strcmp(argv[i], "deconflict") == 0) ? COORD_DECONFLICT : COORD_NONE;
        } else if (strcmp(argv[i], "--strat-a") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "hunt") == 0) cfg->strategy_A = STRATEGY_HUNT_TARGET;
            else if (strcmp(argv[i], "checker") == 0) cfg->strategy_A = STRATEGY_CHECKERBOARD;
            else cfg->strategy_A = STRATEGY_RANDOM;
        } else if (strcmp(argv[i], "--strat-b") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "hunt") == 0) cfg->strategy_B = STRATEGY_HUNT_TARGET;
            else if (strcmp(argv[i], "checker") == 0) cfg->strategy_B = STRATEGY_CHECKERBOARD;
            else cfg->strategy_B = STRATEGY_RANDOM;
        }
    }

    if (cfg->board_width < MIN_DIM) cfg->board_width = MIN_DIM;
    if (cfg->board_width > MAX_DIM) cfg->board_width = MAX_DIM;
    if (cfg->board_height < MIN_DIM) cfg->board_height = MIN_DIM;
    if (cfg->board_height > MAX_DIM) cfg->board_height = MAX_DIM;
    if (cfg->max_rounds <= 0) cfg->max_rounds = DEFAULT_MAX_ROUNDS;

    return 0;
}

void config_print_summary(const SimulationConfig *cfg) {
    if (!cfg) return;
    dprintf(STDOUT_FILENO, "Параметры симуляции:\n");
    dprintf(STDOUT_FILENO, "  * Размер поля: %dx%d\n", cfg->board_width, cfg->board_height);
    dprintf(STDOUT_FILENO, "  * Состав флота: [4-палубных: %d, 3-палубных: %d, 2-палубных: %d, 1-палубных: %d] (всего: %d кораблей)\n",
            cfg->ship_counts[4], cfg->ship_counts[3], cfg->ship_counts[2], cfg->ship_counts[1], cfg->total_ships);
    dprintf(STDOUT_FILENO, "  * Правило выстрелов: %s\n",
            cfg->shot_rule == SHOT_RULE_DECKS_ALIVE ? "По числу живых палуб" :
            (cfg->shot_rule == SHOT_RULE_FIXED_ONE ? "Ровно 1 выстрел" : "Не более 2"));
    dprintf(STDOUT_FILENO, "  * Командный центр A: %s, Стратегия A: %s\n",
            cfg->coord_mode_A == COORD_DECONFLICT ? "Согласование целей" : "Автономный",
            cfg->strategy_A == STRATEGY_HUNT_TARGET ? "Охота" : (cfg->strategy_A == STRATEGY_CHECKERBOARD ? "Шахматная" : "Случайная"));
    dprintf(STDOUT_FILENO, "  * Командный центр B: %s, Стратегия B: %s\n",
            cfg->coord_mode_B == COORD_DECONFLICT ? "Согласование целей" : "Автономный",
            cfg->strategy_B == STRATEGY_HUNT_TARGET ? "Охота" : (cfg->strategy_B == STRATEGY_CHECKERBOARD ? "Шахматная" : "Случайная"));
    dprintf(STDOUT_FILENO, "  * Повторные цели: %s, Предел раундов: %d, Задержка: %d мс\n",
            cfg->allow_repeat_targets ? "Разрешены" : "Запрещены", cfg->max_rounds, cfg->step_delay_ms);
    dprintf(STDOUT_FILENO, "  * Seed ГСЧ: %u, Лог-файл: %s\n\n", cfg->random_seed, cfg->log_filename);
}
