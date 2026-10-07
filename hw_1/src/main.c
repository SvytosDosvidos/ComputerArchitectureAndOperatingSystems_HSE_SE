#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "../include/common.h"
#include "../include/board.h"
#include "../include/ship.h"
#include "../include/fleet.h"
#include "../include/judge.h"
#include "../include/logger.h"
#include "../include/config.h"

static volatile sig_atomic_t g_interrupted = 0;

static void handle_signal(int sig) {
    (void)sig;
    g_interrupted = 1;
}

static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static bool place_fleet_ships(Fleet *fleet, const SimulationConfig *cfg, int id_base, TargetStrategy strat) {
    int ship_idx = 0;

    for (int len = 4; len >= 1; --len) {
        int count = cfg->ship_counts[len];
        for (int c = 0; c < count; ++c) {
            bool placed = false;
            int max_attempts = 2000;
            int attempts = 0;

            while (!placed && attempts++ < max_attempts) {
                int x = rand() % cfg->board_width;
                int y = rand() % cfg->board_height;
                Orientation orient = (rand() % 2 == 0) ? ORIENT_HORIZONTAL : ORIENT_VERTICAL;

                if (board_can_place_ship(&fleet->own_board, x, y, len, orient, false)) {
                    int s_id = id_base + ship_idx + 1;
                    board_place_ship(&fleet->own_board, s_id, x, y, len, orient);
                    ship_init(&fleet->ships[ship_idx], s_id, fleet->id, x, y, len, orient, strat);
                    fleet->ship_count++;
                    ship_idx++;
                    placed = true;
                }
            }

            if (!placed) {
                dprintf(STDERR_FILENO, "[ОШИБКА] Не удалось разместить корабль длины %d для флота %s (тесное поле)\n",
                        len, fleet->name);
                return false;
            }
        }
    }
    return true;
}

int main(int argc, char **argv) {
    setup_signals();

    SimulationConfig cfg;
    config_set_defaults(&cfg);

    if (config_parse_args(&cfg, argc, argv) != 0) {
        return EXIT_FAILURE;
    }

    srand(cfg.random_seed);

    logger_init(cfg.log_filename, cfg.use_colors);

    dprintf(STDOUT_FILENO, "================================================================\n");
    dprintf(STDOUT_FILENO, "   ИДЗ №1: МОРСКОЙ БОЙ АВТОНОМНЫХ КОРАБЛЕЙ (ВАРИАНТ 22)\n");
    dprintf(STDOUT_FILENO, "   НИУ ВШЭ, ФКН, ПИ. Курс «Архитектура компьютера и ОС»\n");
    dprintf(STDOUT_FILENO, "================================================================\n\n");

    config_print_summary(&cfg);

    Fleet fleetA, fleetB;
    fleet_init(&fleetA, FLEET_A, "Флот Альфа (Северный)", cfg.board_width, cfg.board_height,
               cfg.coord_mode_A, cfg.shot_rule, cfg.allow_repeat_targets);
    fleet_init(&fleetB, FLEET_B, "Флот Браво (Южный)", cfg.board_width, cfg.board_height,
               cfg.coord_mode_B, cfg.shot_rule, cfg.allow_repeat_targets);

    if (!place_fleet_ships(&fleetA, &cfg, 100, cfg.strategy_A)) {
        dprintf(STDERR_FILENO, "[ФАТАЛЬНО] Ошибка расстановки флота Альфа\n");
        logger_close();
        return EXIT_FAILURE;
    }

    if (!place_fleet_ships(&fleetB, &cfg, 200, cfg.strategy_B)) {
        dprintf(STDERR_FILENO, "[ФАТАЛЬНО] Ошибка расстановки флота Браво\n");
        logger_close();
        return EXIT_FAILURE;
    }

    Judge judge;
    judge_init(&judge, cfg.max_rounds);

    log_event("СУДЬЯ", "Флоты успешно развернуты. Инварианты проверены. Бой начинается!");
    logger_render_battlefields(&fleetA, &fleetB, cfg.reveal_enemy_ships);

    while (!judge.battle_over && !g_interrupted) {
        fleet_prepare_round_salvo(&fleetA);
        fleet_prepare_round_salvo(&fleetB);

        if (!judge_verify_round_invariants(&judge, &fleetA, &fleetB)) {
            log_event("СУДЬЯ", "Нарушение инвариантов боя! Остановка симуляции.");
            break;
        }

        judge_process_simultaneous_salvos(&judge, &fleetA, &fleetB, logger_get_file_descriptor());

        logger_render_battlefields(&fleetA, &fleetB, cfg.reveal_enemy_ships);

        if (judge_check_battle_completion(&judge, &fleetA, &fleetB)) {
            break;
        }

        if (cfg.step_delay_ms > 0) {
            usleep(cfg.step_delay_ms * 1000);
        }
    }

    if (g_interrupted) {
        log_event("ПРЕРЫВАНИЕ", "Получен сигнал SIGINT/SIGTERM (Ctrl+C). Корректное завершение...");
        judge.stats.termination_reason = "Симуляция прервана пользователем по сигналу (SIGINT)";
    }

    logger_render_final_report(&judge, &fleetA, &fleetB);

    dprintf(STDOUT_FILENO, "\n[ИНФОРМАЦИЯ] Журнал работы успешно записан в файл: %s\n", cfg.log_filename);
    logger_close();

    return EXIT_SUCCESS;
}
