#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include "../include/loop.h"
#include "../include/logging.h"
#include "../include/ipc.h"
#include "../include/node_manager.h"
#include "../include/cli.h"
#include "../include/env.h"

GlobalState init_global_state(void) {
    GlobalState state;
    state.config = NULL;
    return state;
}

static char *read_stdin_line(void) {
    size_t cap = 512;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    while (1) {
        ssize_t r = read(STDIN_FILENO, buf + len, 1);
        if (r <= 0) {
            if (len == 0) { free(buf); return NULL; }
            break;
        }
        if (buf[len] == '\n') {
            buf[len] = '\0';
            return buf;
        }
        len++;
        if (len + 1 >= cap) {
            cap *= 2;
            char *n = realloc(buf, cap);
            if (!n) { free(buf); return NULL; }
            buf = n;
        }
    }
    buf[len] = '\0';
    return buf;
}

void handle_node_message(int fd, const char *msg, GlobalState *state);

void run_event_loop(GlobalState *state) {
    if (!state || !state->config) {
        log_error("run_event_loop: invalid global state");
        return;
    }

    node_sessions_init();

    int server_fd = ipc_server_start(state);
    if (server_fd < 0) {
        log_error("Failed to start IPC server");
        return;
    }

    const int MAX_PFDS = 2 + MAX_SESSIONS + 5;
    struct pollfd *pfds = calloc(MAX_PFDS, sizeof(struct pollfd));
    if (!pfds) {
        log_error("malloc pfds failed");
        ipc_server_stop();
        return;
    }

    while (1) {
        int nfds = 0;
        pfds[nfds].fd = STDIN_FILENO;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        nfds++;

        pfds[nfds].fd = server_fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        nfds++;

        int filled = node_sessions_fill_pollfds(&pfds[nfds], MAX_PFDS - nfds);
        nfds += filled;

        int timeout_ms = 1000;
        int rc = poll(pfds, nfds, timeout_ms);
        if (rc < 0) {
            if (errno == EINTR) continue;
            log_error("poll() error: %s", strerror(errno));
            break;
        }

        int idx = 0;
        if (pfds[idx].revents & POLLIN) {
            char *line = read_stdin_line();
            if (line) {
                parse_cli_command(line);
                free(line);
            } else {
                log_info("stdin closed, shutting down");
                break;
            }
        }
        idx++;

        if (pfds[idx].revents & POLLIN) {
            int cfd = ipc_accept_connection(server_fd);
            if (cfd >= 0) {
                char *hello = ipc_recv_line(cfd, 2000);
                if (!hello) {
                    close(cfd);
                    log_error("No hello from new connection, closed");
                } else {
                    Node meta = {0};
                    if (parse_hello_message(hello, &meta) == 0) {
                        node_session_add(&meta, cfd);
                    } else {
                        log_error("Invalid hello message: %s", hello);
                        close(cfd);
                    }
                    free(hello);
                }
            }
        }
        idx++;

        for (int p = idx; p < nfds; ++p) {
            if (pfds[p].revents & (POLLIN | POLLPRI)) {
                int fd = pfds[p].fd;
                char *msg = ipc_recv_line(fd, 0);
                if (!msg) {
                    node_session_remove_by_fd(fd);
                } else {
                    handle_node_message(fd, msg, state);
                    free(msg);
                }
            } else if (pfds[p].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                node_session_remove_by_fd(pfds[p].fd);
            }
        }

    }

    free(pfds);
    node_sessions_cleanup();
    ipc_server_stop();
}