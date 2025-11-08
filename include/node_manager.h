#pragma once
#include <poll.h>
#include <sys/types.h>
#include <time.h>

#include "env.h"

#define MAX_SESSIONS 128
#define MAX_MSG_LEN (256 * 1024)

typedef struct {
    Node meta;
    int fd;
    time_t last_seen;
    int connected;
} NodeSession;

void node_sessions_init(void);
NodeSession *node_session_add(const Node *node_meta, int fd);
void node_session_remove_by_name(const char *name);
void node_session_remove_by_fd(int fd);
NodeSession *node_session_find_by_name(const char *name);
NodeSession *node_session_find_by_fd(int fd);
int node_sessions_copy(NodeSession *out_array, int max_entries);
ssize_t node_session_send(NodeSession *s, const char *msg);
int node_sessions_fill_pollfds(struct pollfd *pfds, int max_fds);
void node_sessions_cleanup(void);
void handle_node_message(int fd, const char *msg, GlobalState *state);

int parse_hello_message(const char *msg, Node *out_node);