#define _POSIX_C_SOURCE 200809L
#include "common.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s HOST PORT\n", argv[0]); return 2;
    }
    signal(SIGPIPE, SIG_IGN);
    int fd = connect_to(argv[1], argv[2]);
    if (fd < 0) { perror("connect"); return 1; }
    if (send_line(fd, "HELLO OBSERVER\n")) { perror("hello"); close(fd); return 1; }
    char buf[4096];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { perror("recv"); close(fd); return 1; }
        if (!n) break;
        size_t written = 0;
        while (written < (size_t)n) {
            ssize_t out = write(STDOUT_FILENO, buf + written, (size_t)n - written);
            if (out < 0 && errno == EINTR) continue;
            if (out <= 0) { close(fd); return 1; }
            written += (size_t)out;
        }
    }
    close(fd); return 0;
}
