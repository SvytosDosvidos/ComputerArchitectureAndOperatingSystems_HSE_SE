#include "protocol.h"
#include "judge.h"
#include "command_center.h"
#include "ship.h"

static void SignalHandler(int sig) {
    (void)sig;
    dprintf(1, "\n[Interrupt] Cleaning IPC and shutting down...\n");
    JudgeCleanupIPC();
    exit(0);
}

int main(int argc, char **argv) {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    int rounds = 30;
    int delay = 200;
    unsigned int seed = (unsigned int)time(NULL);

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-r") && i + 1 < argc) rounds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) delay = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) seed = (unsigned int)atoi(argv[++i]);
    }

    if (argc > 1 && !strcmp(argv[1], "judge")) {
        Judge judge;
        JudgeInit(&judge, rounds, delay, seed);
        JudgeRun(&judge);
        JudgeClose(&judge);
        return 0;
    }

    if (argc > 2 && !strcmp(argv[1], "cc")) {
        CommandCenter cc;
        CCInit(&cc, atoi(argv[2]), seed);
        CCRun(&cc);
        CCClose(&cc);
        return 0;
    }

    if (argc > 4 && !strcmp(argv[1], "ship")) {
        Ship ship;
        ShipInit(&ship, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), seed);
        ShipRun(&ship);
        ShipClose(&ship);
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--all")) {
        pid_t pids[9];
        int count = 0;

        pid_t pid_j = fork();
        if (pid_j == 0) {
            Judge judge;
            JudgeInit(&judge, rounds, delay, seed);
            JudgeRun(&judge);
            JudgeClose(&judge);
            exit(0);
        }
        pids[count++] = pid_j;
        usleep(50000);

        for (int f = 1; f <= 2; ++f) {
            pid_t pid_cc = fork();
            if (pid_cc == 0) {
                CommandCenter cc;
                CCInit(&cc, f, seed);
                CCRun(&cc);
                CCClose(&cc);
                exit(0);
            }
            pids[count++] = pid_cc;
        }
        usleep(50000);

        int ship_sizes[SHIPS_CNT] = {3, 2, 1};
        for (int f = 1; f <= 2; ++f) {
            for (int s = 0; s < SHIPS_CNT; ++s) {
                pid_t pid_s = fork();
                if (pid_s == 0) {
                    Ship ship;
                    ShipInit(&ship, f, s, ship_sizes[s], seed);
                    ShipRun(&ship);
                    ShipClose(&ship);
                    exit(0);
                }
                pids[count++] = pid_s;
            }
        }

        for (int i = 0; i < count; ++i) {
            waitpid(pids[i], NULL, 0);
        }
        return 0;
    }

    dprintf(2, "Usage:\n");
    dprintf(2, "  %s --all [-r rounds] [-d delay_ms] [-s seed]\n", argv[0]);
    dprintf(2, "  %s judge [-r rounds] [-d delay_ms] [-s seed]\n", argv[0]);
    dprintf(2, "  %s cc <fleet_id> [-s seed]\n", argv[0]);
    dprintf(2, "  %s ship <fleet_id> <ship_id> <size> [-s seed]\n", argv[0]);
    return 1;
}
