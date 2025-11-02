#ifndef ENV_H
#define ENV_H

#define DEFAULT_CONFIG_PATH "config.yaml"

typedef struct {
    char *name;
    char *address;
    char *os;
} Node;

typedef struct {
    char db_path[256];
    char log_path[256];
    int listen_port;
    Node *nodes;
} Config;

Config* config_load(const char *path);

#endif