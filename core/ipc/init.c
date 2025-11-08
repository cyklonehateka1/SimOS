// core/ipc/init.c
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include "../../include/ipc.h"
#include "../../include/logging.h"
#include "../../include/env.h"
#include "../../include/node_manager.h"

static int listen_fd = -1;

int ipc_server_start(GlobalState *state) {
    if (!state || !state->config) {
        log_error("ipc_server_start: invalid state or config");
        return -1;
    }

    int port = state->config->listen_port;
    if (port <= 0) port = 5050; // fallback

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        log_error("socket() failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_error("setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
        // not fatal - continue
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("bind() failed: %s", strerror(errno));
        close(sfd);
        return -1;
    }

    if (listen(sfd, SOMAXCONN) < 0) {
        log_error("listen() failed: %s", strerror(errno));
        close(sfd);
        return -1;
    }

    // Set non-blocking to use poll safely
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
    }

    listen_fd = sfd;
    log_info("IPC server listening on port %d (fd=%d)", port, listen_fd);
    return listen_fd;
}

int ipc_server_stop(void) {
    if (listen_fd >= 0) {
        close(listen_fd);
        listen_fd = -1;
        log_info("IPC server stopped");
    }
    return 0;
}

int ipc_accept_connection(int listen_fd_local) {
    if (listen_fd_local < 0) return -1;
    struct sockaddr_in claddr;
    socklen_t cllen = sizeof(claddr);
    int cfd = accept(listen_fd_local, (struct sockaddr *)&claddr, &cllen);
    if (cfd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return -1;
        }
        log_error("accept() failed: %s", strerror(errno));
        return -1;
    }
    // set non-blocking
    int flags = fcntl(cfd, F_GETFL, 0);
    if (flags >= 0) fcntl(cfd, F_SETFL, flags | O_NONBLOCK);

    char addrbuf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &claddr.sin_addr, addrbuf, sizeof(addrbuf));
    log_info("Accepted connection from %s fd=%d", addrbuf, cfd);
    return cfd;
}

// send all bytes, append newline if missing
ssize_t ipc_send_full(int fd, const char *buf, size_t len) {
    if (fd < 0 || !buf) return -1;
    size_t total = 0;
    // ensure newline at end for our framing
    int has_nl = (len > 0 && buf[len-1] == '\n');
    size_t send_len = len + (has_nl ? 0 : 1);
    char *tmp = NULL;
    if (!has_nl) {
        tmp = malloc(send_len + 1);
        if (!tmp) return -1;
        memcpy(tmp, buf, len);
        tmp[len] = '\n';
        tmp[len+1] = '\0';
    }

    const char *p = has_nl ? buf : tmp;

    while (total < send_len) {
        ssize_t s = send(fd, p + total, send_len - total, 0);
        if (s < 0) {
            if (errno == EINTR) continue;
            if (tmp) free(tmp);
            log_error("send() failed fd=%d: %s", fd, strerror(errno));
            return -1;
        }
        total += (size_t)s;
    }

    if (tmp) free(tmp);
    return (ssize_t)total;
}

// receive until newline; returns heap-allocated string (without trailing newline)
char *ipc_recv_line(int fd, int timeout_ms) {
    if (fd < 0) return NULL;

    // Optionally, wait with poll for readability
    if (timeout_ms >= 0) {
        struct pollfd p;
        p.fd = fd; p.events = POLLIN;
        int rc = poll(&p, 1, timeout_ms);
        if (rc == 0) return NULL; // timeout
        if (rc < 0 && errno != EINTR) return NULL;
    }

    size_t cap = 1024;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    while (1) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) { // peer closed
            free(buf);
            return NULL;
        } else if (r < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // no data now - return what we have if any
                if (len == 0) {
                    free(buf);
                    return NULL;
                } else {
                    break;
                }
            }
            free(buf);
            return NULL;
        } else {
            if (len + 1 >= cap) {
                cap *= 2;
                if (cap > MAX_MSG_LEN) { // protect
                    free(buf);
                    return NULL;
                }
                char *n = realloc(buf, cap);
                if (!n) { free(buf); return NULL; }
                buf = n;
            }
            if (c == '\n') break;
            buf[len++] = c;
        }
    }

    buf[len] = '\0';
    return buf;
}
