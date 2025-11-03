#ifndef ENV_H
#define ENV_H

#define DEFAULT_CONFIG_PATH "config.yaml"
#define MAX_NODES 10
#define DEFAULT_NODE_PORT 8080

typedef struct {
    char name[256];
    char address[256];
    char os[64];
} Node;

typedef struct {
    char db_path[256];
    char log_path[256];
    int listen_port;
    Node *nodes;
    int node_count;
} Config;

Config* config_load(const char *path);
void config_free(Config *cfg);

#endif