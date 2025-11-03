#pragma once
#include "env.h"
#include "loop.h"

char *get_token(const char *input, const char *delimiter, int index);

Node *load_nodes_from_config(Config *config, int *out_count);
Node *find_node_by_name(Config *config, const char *name);
char *send_command_to_node(const Node *node, const char *command);
void execute_node_command(GlobalState *global_state, const char *input_command);