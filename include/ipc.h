// include/ipc.h
#ifndef IPC_H
#define IPC_H

#include <stdlib.h>
#include <sys/types.h>

#include "env.h"

// Start the IPC server (controller side). Returns listening fd or -1.
int ipc_server_start(GlobalState *state);

// Stop the IPC server (close listen fd). Return 0 on success.
int ipc_server_stop(void);

// Accept a new connection on the listening fd. Returns new client fd or -1.
int ipc_accept_connection(int listen_fd);

// Send a full message (ensures full send); appends newline if missing.
// Returns bytes sent or -1 on error.
ssize_t ipc_send_full(int fd, const char *buf, size_t len);

// Receive a single newline terminated line. Returns heap-allocated string (caller must free).
// On EOF or error returns NULL.
char *ipc_recv_line(int fd, int timeout_ms);

#endif // IPC_H
