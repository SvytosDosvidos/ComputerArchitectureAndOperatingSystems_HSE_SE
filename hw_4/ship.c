#define _POSIX_C_SOURCE 200809L
#include "common.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int read_line(int fd, char *line, size_t capacity) {
    size_t n = 0; char ch;
    while (n + 1 < capacity) {
        ssize_t got = recv(fd, &ch, 1, 0);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return -1;
        if (ch == '\n') { line[n] = 0; return 0; }
        line[n++] = ch;
    }
    return -1;
}

int main(int argc, char **argv) {
    int fleet, id, sequential = 0;
    if ((argc != 5 && argc != 6) || parse_int(argv[3], 1, 2, &fleet) ||
        parse_int(argv[4], 1, MAX_SHIPS, &id)) {
        fprintf(stderr, "Usage: %s HOST PORT FLEET SHIP_ID [random|sequential]\n", argv[0]); return 2;
    }
    if (argc == 6) {
        if (!strcmp(argv[5], "sequential")) sequential = 1;
        else if (strcmp(argv[5], "random")) return 2;
    }
    signal(SIGPIPE, SIG_IGN);
    int fd = connect_to(argv[1], argv[2]);
    if (fd < 0) { perror("connect"); return 1; }
    char line[MAX_LINE];
    snprintf(line, sizeof line, "HELLO SHIP %d %d\n", fleet, id);
    if (send_line(fd, line) || read_line(fd, line, sizeof line)) { perror("handshake"); return 1; }
    int size, length, x, y; unsigned seed;
    if (sscanf(line, "WELCOME %d %d %d %d %u", &size, &length, &x, &y, &seed) != 5 ||
        size < 4 || size > MAX_SIDE) { fprintf(stderr, "bad welcome\n"); return 1; }
    unsigned char tried[MAX_SIDE * MAX_SIDE] = {0};
    int cursor = 0;
    printf("Ship fleet=%d id=%d length=%d at=(%d,%d) strategy=%s\n",
           fleet, id, length, x, y, sequential ? "sequential" : "random"); fflush(stdout);
    while (!read_line(fd, line, sizeof line)) {
        int round, shots, winner;
        if (sscanf(line, "GAME_OVER %d", &winner) == 1) {
            printf("Ship fleet=%d id=%d: game over, winner=%d\n", fleet, id, winner);
            close(fd); return 0;
        }
        if (sscanf(line, "TURN %d %d", &round, &shots) != 2 || shots < 1 || shots > MAX_SIDE) {
            fprintf(stderr, "unexpected message: %s\n", line); break;
        }
        char reply[MAX_LINE]; int written = snprintf(reply, sizeof reply, "FIRE %d %d", round, shots);
        for (int k = 0; k < shots; k++) {
            int cell, attempts = size * size;
            do {
                cell = sequential ? cursor++ % (size * size) :
                    (int)(next_random(&seed) % (unsigned)(size * size));
            } while (tried[cell] && --attempts > 0);
            tried[cell] = 1;
            written += snprintf(reply + written, sizeof reply - (size_t)written,
                                " %d %d", cell % size, cell / size);
        }
        snprintf(reply + written, sizeof reply - (size_t)written, "\n");
        if (send_line(fd, reply)) { perror("send"); break; }
        printf("Ship fleet=%d id=%d round=%d shots=%d targets:%s",
               fleet, id, round, shots, reply + (int)strcspn(reply, " ") + 1);
        fflush(stdout);
    }
    fprintf(stderr, "Ship fleet=%d id=%d: connection closed\n", fleet, id);
    close(fd); return 1;
}
