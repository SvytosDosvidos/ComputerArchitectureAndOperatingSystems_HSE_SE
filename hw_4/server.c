#define _POSIX_C_SOURCE 200809L
#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 128
#define OUT_SIZE 32768
#define MAX_SHOTS (MAX_SHIPS * MAX_SIDE)

typedef struct { int x, y, ship, hit; } Shot;
typedef struct { int x, y, horizontal, length, decks; } Ship;
typedef struct {
    int fd, role, fleet, ship;
    char in[MAX_LINE]; size_t in_len;
    char out[OUT_SIZE]; size_t out_len;
} Client;
typedef struct {
    int size, count, lengths[MAX_SHIPS], rounds, delay, repeat, round, running, finished;
    int shots_one, placement_rows;
    unsigned seed;
    Ship ships[2][MAX_SHIPS];
    int board[2][MAX_SIDE][MAX_SIDE]; /* 0: sea; 1..count: intact deck; negative: destroyed deck */
    unsigned char fired[2][MAX_SIDE][MAX_SIDE];
    Shot shots[2][MAX_SHOTS]; int shot_count[2], received[2][MAX_SHIPS];
    Client clients[MAX_CLIENTS]; int listener, log_fd;
} Game;

static volatile sig_atomic_t stop_requested;
static void stop_handler(int sig) { (void)sig; stop_requested = 1; }

static void emit(Game *g, const char *fmt, ...) {
    char line[MAX_LINE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof line - 2, fmt, args);
    va_end(args);
    size_t len = strlen(line);
    line[len++] = '\n'; line[len] = 0;
    fputs(line, stdout); fflush(stdout);
    if (g->log_fd >= 0) {
        ssize_t ignored = write(g->log_fd, line, len);
        (void)ignored;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        Client *c = &g->clients[i];
        if (c->fd < 0 || c->role != 2) continue;
        if (c->out_len + len > OUT_SIZE) { close(c->fd); c->fd = -1; continue; }
        memcpy(c->out + c->out_len, line, len);
        c->out_len += len;
    }
}

static void queue(Client *c, const char *fmt, ...) {
    char line[MAX_LINE]; va_list args;
    va_start(args, fmt); vsnprintf(line, sizeof line, fmt, args); va_end(args);
    size_t len = strlen(line);
    if (c->out_len + len <= OUT_SIZE) {
        memcpy(c->out + c->out_len, line, len); c->out_len += len;
    }
}

static int ship_client(Game *g, int fleet, int id) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (g->clients[i].fd >= 0 && g->clients[i].role == 1 &&
            g->clients[i].fleet == fleet && g->clients[i].ship == id) return i;
    return -1;
}

static int all_connected(Game *g) {
    for (int f = 0; f < 2; f++) for (int s = 0; s < g->count; s++)
        if (ship_client(g, f, s) < 0) return 0;
    return 1;
}

static void snapshot(Game *g, Client *c) {
    queue(c, "SNAPSHOT round=%d state=%s size=%d\n", g->round,
          g->finished ? "finished" : g->running ? "running" : "waiting", g->size);
    for (int f = 0; f < 2; f++) {
        for (int s = 0; s < g->count; s++) {
            Ship *sh = &g->ships[f][s];
            queue(c, "SHIP fleet=%d id=%d x=%d y=%d length=%d decks=%d\n",
                  f + 1, s + 1, sh->x, sh->y, sh->length, sh->decks);
        }
        for (int y = 0; y < g->size; y++) {
            char row[MAX_SIDE + 1];
            for (int x = 0; x < g->size; x++) {
                int v = g->board[f][y][x];
                row[x] = v < 0 ? 'X' : v > 0 ? 'S' : g->fired[1-f][y][x] ? 'o' : '.';
            }
            row[g->size] = 0;
            queue(c, "GRID fleet=%d y=%d %s\n", f + 1, y, row);
        }
    }
    queue(c, "ENDSNAPSHOT\n");
}

static void place_fleet(Game *g, int f) {
    for (int s = 0; s < g->count; s++) {
        int len = g->lengths[s], placed = 0;
        if (g->placement_rows) {
            if (g->count > g->size) { fprintf(stderr, "too many rows for fleet\n"); exit(2); }
            Ship *sh = &g->ships[f][s];
            *sh = (Ship){(g->size - len) / 2, s, 1, len, len};
            for (int k = 0; k < len; k++) g->board[f][s][sh->x + k] = s + 1;
            continue;
        }
        for (int attempt = 0; attempt < 10000 && !placed; attempt++) {
            int horizontal = (int)(next_random(&g->seed) % 2);
            int x = (int)(next_random(&g->seed) % (unsigned)(g->size - (horizontal ? len : 1) + 1));
            int y = (int)(next_random(&g->seed) % (unsigned)(g->size - (horizontal ? 1 : len) + 1));
            placed = 1;
            for (int k = 0; k < len; k++)
                if (g->board[f][y + (horizontal ? 0 : k)][x + (horizontal ? k : 0)]) placed = 0;
            if (!placed) continue;
            Ship *sh = &g->ships[f][s];
            *sh = (Ship){x, y, horizontal, len, len};
            for (int k = 0; k < len; k++)
                g->board[f][y + (horizontal ? 0 : k)][x + (horizontal ? k : 0)] = s + 1;
        }
        if (!placed) { fprintf(stderr, "fleet does not fit on board\n"); exit(2); }
    }
}

static void start_round(Game *g) {
    g->running = 1; g->round++;
    memset(g->received, 0, sizeof g->received);
    memset(g->shot_count, 0, sizeof g->shot_count);
    emit(g, "ROUND %d BEGIN", g->round);
    for (int f = 0; f < 2; f++) for (int s = 0; s < g->count; s++) {
        Ship *sh = &g->ships[f][s];
        if (!sh->decks) { g->received[f][s] = 1; continue; }
        int ci = ship_client(g, f, s);
        int shots = g->shots_one ? 1 : sh->decks;
        queue(&g->clients[ci], "TURN %d %d\n", g->round, shots);
        emit(g, "READY fleet=%d ship=%d decks=%d shots=%d", f + 1, s + 1, sh->decks, shots);
    }
}

static int all_received(Game *g) {
    for (int f = 0; f < 2; f++) for (int s = 0; s < g->count; s++)
        if (!g->received[f][s]) return 0;
    return 1;
}

/* Command center: resolve same-side targets before either fleet takes damage. */
static void coordinate(Game *g, int f) {
    unsigned char used[MAX_SIDE][MAX_SIDE] = {{0}};
    for (int i = 0; i < g->shot_count[f]; i++) {
        Shot *shot = &g->shots[f][i];
        if (!g->repeat && (used[shot->y][shot->x] || g->fired[f][shot->y][shot->x])) {
            int found = 0;
            for (int y = 0; y < g->size && !found; y++) for (int x = 0; x < g->size; x++)
                if (!used[y][x] && !g->fired[f][y][x]) {
                    emit(g, "RETARGET fleet=%d ship=%d from=%d,%d to=%d,%d",
                         f + 1, shot->ship + 1, shot->x, shot->y, x, y);
                    shot->x = x; shot->y = y; found = 1; break;
                }
        }
        used[shot->y][shot->x] = 1;
    }
}

static void finish(Game *g, const char *reason) {
    int health[2] = {0, 0};
    for (int f = 0; f < 2; f++) for (int s = 0; s < g->count; s++)
        health[f] += g->ships[f][s].decks;
    int winner = health[0] == health[1] ? 0 : health[0] > health[1] ? 1 : 2;
    g->finished = 1; g->running = 0;
    emit(g, "GAME_OVER reason=%s winner=%d health=%d,%d", reason, winner, health[0], health[1]);
    for (int f = 0; f < 2; f++) for (int s = 0; s < g->count; s++) {
        int ci = ship_client(g, f, s);
        if (ci >= 0) queue(&g->clients[ci], "GAME_OVER %d\n", winner);
    }
}

static void resolve_round(Game *g) {
    coordinate(g, 0); coordinate(g, 1);
    emit(g, "ROUND %d APPLY shots=%d,%d", g->round, g->shot_count[0], g->shot_count[1]);
    /* All FIRE messages have arrived. Their shots remain valid even if a ship sinks here. */
    for (int f = 0; f < 2; f++) for (int i = 0; i < g->shot_count[f]; i++) {
        Shot *shot = &g->shots[f][i]; int enemy = 1 - f;
        int *cell = &g->board[enemy][shot->y][shot->x];
        g->fired[f][shot->y][shot->x] = 1;
        if (*cell > 0) {
            int target = *cell - 1;
            *cell = -*cell;
            g->ships[enemy][target].decks--;
            shot->hit = 1;
            emit(g, "HIT fleet=%d ship=%d target_fleet=%d target_ship=%d x=%d y=%d remaining=%d",
                 f + 1, shot->ship + 1, enemy + 1, target + 1, shot->x, shot->y,
                 g->ships[enemy][target].decks);
            if (!g->ships[enemy][target].decks)
                emit(g, "SUNK fleet=%d ship=%d", enemy + 1, target + 1);
        } else emit(g, "MISS fleet=%d ship=%d x=%d y=%d repeat=%d",
                    f + 1, shot->ship + 1, shot->x, shot->y, *cell < 0);
    }
    int health[2] = {0,0};
    for (int f = 0; f < 2; f++) for (int s = 0; s < g->count; s++)
        health[f] += g->ships[f][s].decks;
    emit(g, "SUMMARY round=%d health=%d,%d", g->round, health[0], health[1]);
    if (!health[0] || !health[1]) finish(g, "fleet_destroyed");
    else if (g->round >= g->rounds) finish(g, "round_limit");
}

static void disconnect_client(Game *g, int i) {
    Client *c = &g->clients[i];
    int role = c->role, f = c->fleet, s = c->ship;
    close(c->fd); c->fd = -1;
    if (role == 2) emit(g, "OBSERVER_LEFT");
    if (role == 1 && !g->finished) {
        emit(g, "SHIP_LEFT fleet=%d ship=%d", f + 1, s + 1);
        if (g->running) finish(g, "ship_disconnected");
    }
}

static int handle_fire(Game *g, Client *c, char *line) {
    int round, count, x, y;
    char *save, *token = strtok_r(line, " ", &save);
    token = strtok_r(NULL, " ", &save);
    if (!token || parse_int(token, 1, g->rounds, &round) || round != g->round) return -1;
    token = strtok_r(NULL, " ", &save);
    if (!token || parse_int(token, 1, MAX_SIDE, &count) ||
        count != (g->shots_one ? 1 : g->ships[c->fleet][c->ship].decks) ||
        g->received[c->fleet][c->ship]) return -1;
    Shot tmp[MAX_SIDE];
    for (int i = 0; i < count; i++) {
        token = strtok_r(NULL, " ", &save);
        if (!token || parse_int(token, 0, g->size - 1, &x)) return -1;
        token = strtok_r(NULL, " ", &save);
        if (!token || parse_int(token, 0, g->size - 1, &y)) return -1;
        tmp[i] = (Shot){x,y,c->ship,0};
    }
    if (strtok_r(NULL, " ", &save)) return -1;
    for (int i = 0; i < count; i++) {
        g->shots[c->fleet][g->shot_count[c->fleet]++] = tmp[i];
        emit(g, "TARGET round=%d fleet=%d ship=%d x=%d y=%d",
             g->round, c->fleet + 1, c->ship + 1, tmp[i].x, tmp[i].y);
    }
    g->received[c->fleet][c->ship] = 1;
    if (all_received(g)) resolve_round(g);
    return 0;
}

static void handle_line(Game *g, int i, char *line) {
    Client *c = &g->clients[i];
    if (!c->role) {
        int f, s; char extra;
        if (!strcmp(line, "HELLO OBSERVER")) {
            c->role = 2;
            snapshot(g, c);
            emit(g, "OBSERVER_JOINED");
        } else if (sscanf(line, "HELLO SHIP %d %d %c", &f, &s, &extra) == 2 &&
                   f >= 1 && f <= 2 && s >= 1 && s <= g->count && !g->running &&
                   ship_client(g, f - 1, s - 1) < 0) {
            c->role = 1; c->fleet = f - 1; c->ship = s - 1;
            Ship *sh = &g->ships[f-1][s-1];
            queue(c, "WELCOME %d %d %d %d %d\n", g->size, sh->length, sh->x, sh->y,
                  g->seed + (unsigned)(f * 101 + s));
            emit(g, "SHIP_JOINED fleet=%d ship=%d length=%d x=%d y=%d", f, s,
                 sh->length, sh->x, sh->y);
            if (all_connected(g)) start_round(g);
        } else disconnect_client(g, i);
    } else if (c->role == 1 && g->running && !g->finished && !strncmp(line, "FIRE ", 5)) {
        if (handle_fire(g, c, line)) disconnect_client(g, i);
    } else disconnect_client(g, i);
}

static void read_client(Game *g, int i) {
    Client *c = &g->clients[i]; char buf[512];
    for (;;) {
        ssize_t n = recv(c->fd, buf, sizeof buf, 0);
        if (!n) { disconnect_client(g, i); return; }
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                disconnect_client(g, i);
            return;
        }
        for (ssize_t k = 0; k < n; k++) {
            if (buf[k] == '\n') {
                c->in[c->in_len] = 0;
                if (c->in_len && c->in[c->in_len-1] == '\r') c->in[--c->in_len] = 0;
                handle_line(g, i, c->in);
                c->in_len = 0;
                if (c->fd < 0) return;
            } else if (c->in_len < sizeof c->in - 1) c->in[c->in_len++] = buf[k];
            else { disconnect_client(g, i); return; }
        }
    }
}

static void flush_client(Game *g, int i) {
    Client *c = &g->clients[i];
    if (!c->out_len) return;
    ssize_t n = send(c->fd, c->out, c->out_len, MSG_NOSIGNAL);
    if (n > 0) { memmove(c->out, c->out + n, c->out_len - (size_t)n); c->out_len -= (size_t)n; }
    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        disconnect_client(g, i);
}

static int parse_fleet(Game *g, const char *text) {
    char copy[128]; if (strlen(text) >= sizeof copy) return -1;
    strcpy(copy, text); char *save, *part = strtok_r(copy, ",", &save); int total = 0;
    while (part && g->count < MAX_SHIPS) {
        int len; if (parse_int(part, 1, g->size, &len)) return -1;
        g->lengths[g->count++] = len; total += len;
        part = strtok_r(NULL, ",", &save);
    }
    return (!part && g->count && total <= g->size * g->size) ? 0 : -1;
}

static void usage(const char *name) {
    fprintf(stderr, "Usage: %s HOST PORT [--size 8] [--fleet 3,2,1] [--rounds 30] [--delay 100] [--seed 42] [--shots decks|one] [--placement random|rows] [--repeat] [--log FILE]\n", name);
}

int main(int argc, char **argv) {
    Game *g = calloc(1, sizeof *g);
    if (!g) return 1;
    g->size = 8; g->rounds = 30; g->delay = 100; g->seed = 42; g->log_fd = -1;
    const char *fleet = "3,2,1";
    const char *log_path = NULL;
    if (argc < 3) { usage(argv[0]); return 2; }
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--repeat")) { g->repeat = 1; continue; }
        if (i + 1 == argc) { usage(argv[0]); return 2; }
        if (!strcmp(argv[i], "--size")) { if (parse_int(argv[++i], 4, MAX_SIDE, &g->size)) return 2; }
        else if (!strcmp(argv[i], "--fleet")) fleet = argv[++i];
        else if (!strcmp(argv[i], "--rounds")) { if (parse_int(argv[++i], 1, 10000, &g->rounds)) return 2; }
        else if (!strcmp(argv[i], "--delay")) { if (parse_int(argv[++i], 0, 10000, &g->delay)) return 2; }
        else if (!strcmp(argv[i], "--seed")) { int seed; if (parse_int(argv[++i], 1, 2147483647, &seed)) return 2; g->seed = (unsigned)seed; }
        else if (!strcmp(argv[i], "--shots")) {
            const char *value = argv[++i];
            if (strcmp(value, "decks") && strcmp(value, "one")) return 2;
            g->shots_one = !strcmp(value, "one");
        }
        else if (!strcmp(argv[i], "--placement")) {
            const char *value = argv[++i];
            if (strcmp(value, "random") && strcmp(value, "rows")) return 2;
            g->placement_rows = !strcmp(value, "rows");
        }
        else if (!strcmp(argv[i], "--log")) log_path = argv[++i];
        else { usage(argv[0]); return 2; }
    }
    if (parse_fleet(g, fleet)) { fprintf(stderr, "invalid fleet\n"); return 2; }
    if (log_path) {
        g->log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (g->log_fd < 0) { perror("log"); return 1; }
    }
    unsigned initial_seed = g->seed;
    for (int i = 0; i < MAX_CLIENTS; i++) g->clients[i].fd = -1;
    place_fleet(g, 0); place_fleet(g, 1);
    g->listener = listen_on(argv[1], argv[2]);
    if (g->listener < 0 || set_nonblock(g->listener)) { perror("listen"); return 1; }
    signal(SIGINT, stop_handler); signal(SIGTERM, stop_handler); signal(SIGPIPE, SIG_IGN);
    emit(g, "SERVER host=%s port=%s size=%d ships=%d rounds=%d seed=%u", argv[1], argv[2], g->size, g->count, g->rounds, initial_seed);
    while (!stop_requested) {
        struct pollfd pfds[MAX_CLIENTS + 1]; int map[MAX_CLIENTS + 1], n = 1;
        pfds[0] = (struct pollfd){g->listener, POLLIN, 0}; map[0] = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) if (g->clients[i].fd >= 0) {
            pfds[n] = (struct pollfd){g->clients[i].fd,
                (short)(POLLIN | (g->clients[i].out_len ? POLLOUT : 0)), 0}; map[n++] = i;
        }
        int result = poll(pfds, (nfds_t)n, 100);
        if (result < 0 && errno != EINTR) { perror("poll"); break; }
        if (pfds[0].revents & POLLIN) for (;;) {
            int fd = accept(g->listener, NULL, NULL);
            if (fd < 0) break;
            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) if (g->clients[i].fd < 0) { slot = i; break; }
            if (slot < 0 || set_nonblock(fd)) close(fd);
            else { g->clients[slot] = (Client){0}; g->clients[slot].fd = fd; }
        }
        for (int j = 1; j < n; j++) {
            int i = map[j]; if (g->clients[i].fd < 0) continue;
            if (pfds[j].revents & (POLLERR | POLLHUP | POLLNVAL)) { disconnect_client(g, i); continue; }
            if (pfds[j].revents & POLLIN) read_client(g, i);
            if (g->clients[i].fd >= 0 && (pfds[j].revents & POLLOUT)) flush_client(g, i);
        }
        if (g->finished) {
            int pending = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) if (g->clients[i].fd >= 0 && g->clients[i].out_len) pending = 1;
            if (!pending) break;
        }
        if (g->running && all_received(g) && !g->finished) {
            struct timespec delay = {g->delay / 1000, (g->delay % 1000) * 1000000L};
            nanosleep(&delay, NULL); start_round(g);
        }
    }
    if (!g->finished) finish(g, "interrupted");
    for (int i = 0; i < MAX_CLIENTS; i++) if (g->clients[i].fd >= 0) {
        flush_client(g, i); close(g->clients[i].fd);
    }
    close(g->listener);
    if (g->log_fd >= 0) close(g->log_fd);
    free(g); return 0;
}
