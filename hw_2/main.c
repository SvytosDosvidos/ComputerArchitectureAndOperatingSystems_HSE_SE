#include "battle.h"
#include "utils.h"

volatile sig_atomic_t flag_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    flag_stop = 1;
}

int main(int argc, char **argv) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    Game *game = calloc(1, sizeof(Game));
    char log_path[128] = "battle.log";

    init_config(&game->cfg);
    parse_args(argc, argv, &game->cfg, log_path);

    game->fd_log = open(log_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    init_game(game);

    for (int s = 0; s < 2; s++) {
        pthread_create(&game->centers[s].thread, NULL, run_center, &game->centers[s]);
        for (int i = 0; i < game->cfg.cnt_ships; i++) {
            pthread_create(&game->ships[s][i].thread, NULL, run_ship, &game->ships[s][i]);
        }
    }
    pthread_create(&game->judge_thread, NULL, run_judge, game);

    pthread_join(game->judge_thread, NULL);
    for (int s = 0; s < 2; s++) {
        pthread_join(game->centers[s].thread, NULL);
        for (int i = 0; i < game->cfg.cnt_ships; i++) {
            pthread_join(game->ships[s][i].thread, NULL);
        }
    }

    print_stats(game);

    if (game->fd_log >= 0) {
        close(game->fd_log);
    }
    destroy_game(game);
    free(game);
    return 0;
}