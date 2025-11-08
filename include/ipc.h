
#ifndef IPC_H
#define IPC_H

#include <stdlib.h>
#include <sys/types.h>

#include "env.h"

int ipc_server_start(GlobalState *state);

int ipc_server_stop(void);

int ipc_accept_connection(int listen_fd);

ssize_t ipc_send_full(int fd, const char *buf, size_t len);

char *ipc_recv_line(int fd, int timeout_ms);

#endif 
