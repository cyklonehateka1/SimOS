#pragma once
#include "env.h"
#include "loop.h"
#include <time.h>

char *get_token(const char *input, const char *delimiter, int index);

Node *load_nodes_from_config(Config *config, int *out_count);
Node *find_node_by_name(Config *config, const char *name);
char *send_command_to_node(const Node *node, const char *command);
void execute_node_command(GlobalState *global_state, const char *input_command);

#define MAX_SESSIONS 128

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
ssize_t node_session_send(NodeSession *s, const char *msg);
int node_sessions_fill_pollfds(struct pollfd *pfds, int max_fds);