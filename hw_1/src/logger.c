#define _POSIX_C_SOURCE 200809L
#include "../include/logger.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

static int g_log_fd = -1;
static bool g_use_colors = true;

#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

void logger_init(const char *log_filepath, bool use_colors) {
    g_use_colors = use_colors;

    if (log_filepath && strlen(log_filepath) > 0) {
        g_log_fd = open(log_filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (g_log_fd < 0) {
            dprintf(STDERR_FILENO, "[ПРЕДУПРЕЖДЕНИЕ] Не удалось открыть лог-файл '%s' для записи\n", log_filepath);
        } else {
            dprintf(g_log_fd, "=== ЖУРНАЛ ИМИТАЦИОННОЙ МОДЕЛИ «МОРСКОЙ БОЙ АВТОНОМНЫХ КОРАБЛЕЙ» ===\n");
            dprintf(g_log_fd, "=== НИУ ВШЭ, Факультет компьютерных наук, АКОС, Вариант 22 ===\n\n");
        }
    }
}

void logger_close(void) {
    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "\n=== ЗАВЕРШЕНИЕ ЖУРНАЛИРОВАНИЯ ===\n");
        close(g_log_fd);
        g_log_fd = -1;
    }
}

int logger_get_file_descriptor(void) {
    return g_log_fd;
}

void log_msg(const char *fmt, ...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    vdprintf(STDOUT_FILENO, fmt, args1);
    va_end(args1);

    if (g_log_fd >= 0) {
        vdprintf(g_log_fd, fmt, args2);
    }
    va_end(args2);
}

void log_event(const char *category, const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (g_use_colors) {
        dprintf(STDOUT_FILENO, "%s[%s]%s %s\n", COLOR_CYAN, category, COLOR_RESET, buffer);
    } else {
        dprintf(STDOUT_FILENO, "[%s] %s\n", category, buffer);
    }

    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "[%s] %s\n", category, buffer);
    }
}

void log_event_ship_readiness(const Fleet *f) {
    if (!f) return;
    dprintf(STDOUT_FILENO, "\n%s--- Готовность флота %s ---%s\n",
            g_use_colors ? COLOR_BOLD : "", f->name, g_use_colors ? COLOR_RESET : "");

    for (int i = 0; i < f->ship_count; ++i) {
        const AutonomousShip *s = &f->ships[i];
        if (s->is_alive) {
            dprintf(STDOUT_FILENO, "  * Корабль #%d: [%d/%d палуб] готов к залпу на (%d,%d), стратегия: %s\n",
                    s->id, s->decks_alive, s->decks_total, s->x, s->y,
                    s->strategy == STRATEGY_HUNT_TARGET ? "Охота (Hunt)" :
                    (s->strategy == STRATEGY_CHECKERBOARD ? "Шахматная (Checker)" : "Случайная (Random)"));
        } else {
            dprintf(STDOUT_FILENO, "  * Корабль #%d: [УНИЧТОЖЕН]\n", s->id);
        }
    }

    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "--- Готовность флота %s ---\n", f->name);
        for (int i = 0; i < f->ship_count; ++i) {
            const AutonomousShip *s = &f->ships[i];
            if (s->is_alive) {
                dprintf(g_log_fd, "  * Корабль #%d: [%d/%d палуб] готов к бою на (%d,%d)\n",
                        s->id, s->decks_alive, s->decks_total, s->x, s->y);
            }
        }
    }
}

void log_event_formed_targets(const Fleet *f) {
    if (!f) return;
    dprintf(STDOUT_FILENO, "  Сформированные цели флота %s (%d выстрелов):\n", f->name, f->round_salvo_count);
    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "  Сформированные цели флота %s (%d выстрелов):\n", f->name, f->round_salvo_count);
    }

    for (int i = 0; i < f->round_salvo_count; ++i) {
        dprintf(STDOUT_FILENO, "    -> Корабль #%d целится в (%d, %d)\n",
                f->round_salvo[i].shooter_ship_id,
                f->round_salvo[i].target.x,
                f->round_salvo[i].target.y);
        if (g_log_fd >= 0) {
            dprintf(g_log_fd, "    -> Корабль #%d целится в (%d, %d)\n",
                    f->round_salvo[i].shooter_ship_id,
                    f->round_salvo[i].target.x,
                    f->round_salvo[i].target.y);
        }
    }
}

void log_event_round_start(int round_num) {
    const char *border = "================================================================";
    if (g_use_colors) {
        dprintf(STDOUT_FILENO, "\n%s%s%s\n", COLOR_YELLOW, border, COLOR_RESET);
        dprintf(STDOUT_FILENO, "%s>>> НАЧАЛО РАУНДА %d <<<%s\n", COLOR_BOLD, round_num, COLOR_RESET);
        dprintf(STDOUT_FILENO, "%s%s%s\n", COLOR_YELLOW, border, COLOR_RESET);
    } else {
        dprintf(STDOUT_FILENO, "\n%s\n>>> НАЧАЛО РАУНДА %d <<<\n%s\n", border, round_num, border);
    }

    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "\n%s\n>>> НАЧАЛО РАУНДА %d <<<\n%s\n", border, round_num, border);
    }
}

void log_event_shot_application(const char *attacker_name, int ship_id, Point target, ShotOutcome outcome, int victim_ship_id) {
    const char *res_str = "ПРОМАХ";
    const char *color = COLOR_BLUE;

    if (outcome == SHOT_HIT) {
        res_str = "ПОПАДАНИЕ (РАНЕН)";
        color = COLOR_YELLOW;
    } else if (outcome == SHOT_SUNK) {
        res_str = "ПОПАДАНИЕ (УНИЧТОЖЕН)";
        color = COLOR_RED;
    } else if (outcome == SHOT_REPEAT) {
        res_str = "ПОВТОРНЫЙ ВЫСТРЕЛ";
        color = COLOR_RESET;
    }

    if (g_use_colors) {
        dprintf(STDOUT_FILENO, "  [%sЗалп%s] %s (Корабль #%d) -> (%d, %d): %s%s%s",
                COLOR_CYAN, COLOR_RESET, attacker_name, ship_id, target.x, target.y,
                color, res_str, COLOR_RESET);
        if (victim_ship_id >= 0) {
            dprintf(STDOUT_FILENO, " (цель: корабль #%d)", victim_ship_id);
        }
        dprintf(STDOUT_FILENO, "\n");
    } else {
        dprintf(STDOUT_FILENO, "  [Залп] %s (Корабль #%d) -> (%d, %d): %s",
                attacker_name, ship_id, target.x, target.y, res_str);
        if (victim_ship_id >= 0) {
            dprintf(STDOUT_FILENO, " (цель: корабль #%d)", victim_ship_id);
        }
        dprintf(STDOUT_FILENO, "\n");
    }

    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "  [Залп] %s (Корабль #%d) -> (%d, %d): %s",
                attacker_name, ship_id, target.x, target.y, res_str);
        if (victim_ship_id >= 0) {
            dprintf(g_log_fd, " (цель: корабль #%d)", victim_ship_id);
        }
        dprintf(g_log_fd, "\n");
    }
}

void log_event_combat_capacity_change(const AutonomousShip *ship) {
    if (!ship) return;
    if (g_use_colors) {
        dprintf(STDOUT_FILENO, "    %s[Боеспособность]%s Корабль #%d: осталось %d/%d палуб (огневая мощь: %d залпов)\n",
                COLOR_YELLOW, COLOR_RESET, ship->id, ship->decks_alive, ship->decks_total, ship->decks_alive);
    } else {
        dprintf(STDOUT_FILENO, "    [Боеспособность] Корабль #%d: осталось %d/%d палуб\n",
                ship->id, ship->decks_alive, ship->decks_total);
    }

    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "    [Боеспособность] Корабль #%d: осталось %d/%d палуб\n",
                ship->id, ship->decks_alive, ship->decks_total);
    }
}

void log_event_ship_destroyed(const AutonomousShip *ship) {
    if (!ship) return;
    if (g_use_colors) {
        dprintf(STDOUT_FILENO, "    %s[КАТАСТРОФА]%s Корабль #%d ПОЛНОСТЬЮ УНИЧТОЖЕН И ИДЁТ НА ДНО!\n",
                COLOR_RED, COLOR_RESET, ship->id);
    } else {
        dprintf(STDOUT_FILENO, "    [КАТАСТРОФА] Корабль #%d ПОЛНОСТЬЮ УНИЧТОЖЕН И ИДЁТ НА ДНО!\n",
                ship->id);
    }

    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "    [КАТАСТРОФА] Корабль #%d ПОЛНОСТЬЮ УНИЧТОЖЕН!\n", ship->id);
    }
}

void log_event_fleets_summary(const Fleet *fA, const Fleet *fB) {
    if (!fA || !fB) return;
    const char *sub_border = "----------------------------------------------------------------";
    dprintf(STDOUT_FILENO, "\n%s\n", sub_border);
    dprintf(STDOUT_FILENO, "%sСводное состояние флотов:%s\n", g_use_colors ? COLOR_BOLD : "", g_use_colors ? COLOR_RESET : "");
    dprintf(STDOUT_FILENO, "  * %s: Живых кораблей: %d/%d, Уцелевших палуб: %d\n",
            fA->name, fleet_alive_ship_count(fA), fA->ship_count, fleet_total_alive_decks(fA));
    dprintf(STDOUT_FILENO, "  * %s: Живых кораблей: %d/%d, Уцелевших палуб: %d\n",
            fB->name, fleet_alive_ship_count(fB), fB->ship_count, fleet_total_alive_decks(fB));
    dprintf(STDOUT_FILENO, "%s\n", sub_border);

    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "\n%s\nСводное состояние флотов:\n", sub_border);
        dprintf(g_log_fd, "  * %s: Живых кораблей: %d/%d, Уцелевших палуб: %d\n",
                fA->name, fleet_alive_ship_count(fA), fA->ship_count, fleet_total_alive_decks(fA));
        dprintf(g_log_fd, "  * %s: Живых кораблей: %d/%d, Уцелевших палуб: %d\n%s\n",
                fB->name, fleet_alive_ship_count(fB), fB->ship_count, fleet_total_alive_decks(fB), sub_border);
    }
}

static char render_cell_char(CellState state, bool reveal) {
    switch (state) {
        case CELL_SHIP: return reveal ? '#' : '~';
        case CELL_HIT:  return 'X';
        case CELL_MISS: return 'o';
        default:        return '~';
    }
}

void logger_render_battlefields(const Fleet *fA, const Fleet *fB, bool reveal_ships) {
    if (!fA || !fB) return;
    int w = fA->own_board.width;
    int h = fA->own_board.height;

    dprintf(STDOUT_FILENO, "\n     %-*s           %-*s\n", w * 2, fA->name, w * 2, fB->name);

    dprintf(STDOUT_FILENO, "    ");
    for (int x = 0; x < w; ++x) dprintf(STDOUT_FILENO, " %d", x % 10);
    dprintf(STDOUT_FILENO, "        ");
    for (int x = 0; x < w; ++x) dprintf(STDOUT_FILENO, " %d", x % 10);
    dprintf(STDOUT_FILENO, "\n");

    for (int y = 0; y < h; ++y) {
        dprintf(STDOUT_FILENO, "%2d |", y);
        for (int x = 0; x < w; ++x) {
            char c = render_cell_char(fA->own_board.grid[y][x], reveal_ships);
            if (g_use_colors) {
                if (c == 'X') dprintf(STDOUT_FILENO, " %s%c%s", COLOR_RED, c, COLOR_RESET);
                else if (c == 'o') dprintf(STDOUT_FILENO, " %s%c%s", COLOR_BLUE, c, COLOR_RESET);
                else if (c == '#') dprintf(STDOUT_FILENO, " %s%c%s", COLOR_GREEN, c, COLOR_RESET);
                else dprintf(STDOUT_FILENO, " %c", c);
            } else {
                dprintf(STDOUT_FILENO, " %c", c);
            }
        }
        dprintf(STDOUT_FILENO, " |    %2d |", y);

        for (int x = 0; x < w; ++x) {
            char c = render_cell_char(fB->own_board.grid[y][x], reveal_ships);
            if (g_use_colors) {
                if (c == 'X') dprintf(STDOUT_FILENO, " %s%c%s", COLOR_RED, c, COLOR_RESET);
                else if (c == 'o') dprintf(STDOUT_FILENO, " %s%c%s", COLOR_BLUE, c, COLOR_RESET);
                else if (c == '#') dprintf(STDOUT_FILENO, " %s%c%s", COLOR_GREEN, c, COLOR_RESET);
                else dprintf(STDOUT_FILENO, " %c", c);
            } else {
                dprintf(STDOUT_FILENO, " %c", c);
            }
        }
        dprintf(STDOUT_FILENO, " |\n");
    }
    dprintf(STDOUT_FILENO, "\n");
}

void logger_render_final_report(const Judge *judge, const Fleet *fA, const Fleet *fB) {
    if (!judge || !fA || !fB) return;
    const char *sep = "================================================================";

    dprintf(STDOUT_FILENO, "\n%s%s%s\n", g_use_colors ? COLOR_BOLD : "", sep, g_use_colors ? COLOR_RESET : "");
    dprintf(STDOUT_FILENO, "%sИТОГОВЫЙ ОТЧЕТ БОЯ СУДЬИ:%s\n", g_use_colors ? COLOR_GREEN : "", g_use_colors ? COLOR_RESET : "");
    dprintf(STDOUT_FILENO, "  * Результат: %s\n", judge->stats.termination_reason);
    dprintf(STDOUT_FILENO, "  * Количество раундов: %d\n", judge->current_round);
    dprintf(STDOUT_FILENO, "  * Выстрелов флота A: %d (попаданий: %d, потоплено: %d)\n",
            judge->stats.total_shots_fired[0], judge->stats.total_hits[0], judge->stats.total_ships_sunk[0]);
    dprintf(STDOUT_FILENO, "  * Выстрелов флота B: %d (попаданий: %d, потоплено: %d)\n",
            judge->stats.total_shots_fired[1], judge->stats.total_hits[1], judge->stats.total_ships_sunk[1]);
    dprintf(STDOUT_FILENO, "%s\n", sep);

    if (g_log_fd >= 0) {
        dprintf(g_log_fd, "\n%s\nИТОГОВЫЙ ОТЧЕТ БОЯ СУДЬИ:\n", sep);
        dprintf(g_log_fd, "  * Результат: %s\n", judge->stats.termination_reason);
        dprintf(g_log_fd, "  * Количество раундов: %d\n", judge->current_round);
        dprintf(g_log_fd, "  * Выстрелов флота A: %d (попаданий: %d, потоплено: %d)\n",
                judge->stats.total_shots_fired[0], judge->stats.total_hits[0], judge->stats.total_ships_sunk[0]);
        dprintf(g_log_fd, "  * Выстрелов флота B: %d (попаданий: %d, потоплено: %d)\n",
                judge->stats.total_shots_fired[1], judge->stats.total_hits[1], judge->stats.total_ships_sunk[1]);
        dprintf(g_log_fd, "%s\n", sep);
    }
}
