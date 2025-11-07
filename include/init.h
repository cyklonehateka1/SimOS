#ifndef INIT_H
#define INIT_H

#include <stdbool.h>
int ipc_server_start(GlobalState *state);
int ipc_server_stop(GlobalState *state, int *fd);
int ipc_accept_connection(int server_fd);
ssize_t ipc_send_full(int fd, const char *buf, size_t len);
char *ipc_recv_line(int fd, int timeout_ms);

int node_sessions_fill_pollfds(struct pollfd *pfds, int max_fds);

#endif