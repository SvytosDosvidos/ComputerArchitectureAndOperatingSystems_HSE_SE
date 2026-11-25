#define _POSIX_C_SOURCE 200809L
#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int parse_int(const char *text, int min, int max, int *value) {
    char *end;
    long result;
    errno = 0;
    result = strtol(text, &end, 10);
    if (errno || end == text || *end || result < min || result > max) return -1;
    *value = (int)result;
    return 0;
}

static int socket_for(const char *host, const char *port, int passive) {
    struct addrinfo hints = {0}, *addresses, *item;
    int fd = -1, yes = 1;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = passive ? AI_PASSIVE : 0;
    if (getaddrinfo(host, port, &hints, &addresses)) return -1;
    for (item = addresses; item; item = item->ai_next) {
        fd = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (fd < 0) continue;
        if (passive) {
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
            if (!bind(fd, item->ai_addr, item->ai_addrlen) && !listen(fd, 32)) break;
        } else if (!connect(fd, item->ai_addr, item->ai_addrlen)) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    return fd;
}

int connect_to(const char *host, const char *port) { return socket_for(host, port, 0); }
int listen_on(const char *host, const char *port) { return socket_for(host, port, 1); }

int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    return flags < 0 ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Used for the short, bounded protocol messages sent to ship clients. */
int send_line(int fd, const char *line) {
    size_t left = strlen(line);
    while (left) {
        ssize_t n = send(fd, line, left, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        line += n;
        left -= (size_t)n;
    }
    return 0;
}

unsigned next_random(unsigned *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}
