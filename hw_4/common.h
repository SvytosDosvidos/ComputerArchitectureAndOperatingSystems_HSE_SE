#ifndef NAVAL_COMMON_H
#define NAVAL_COMMON_H

#include <stddef.h>

#define MAX_SIDE 16
#define MAX_SHIPS 8
#define MAX_LINE 2048

int parse_int(const char *text, int min, int max, int *value);
int connect_to(const char *host, const char *port);
int listen_on(const char *host, const char *port);
int set_nonblock(int fd);
int send_line(int fd, const char *line);
unsigned next_random(unsigned *state);

#endif
