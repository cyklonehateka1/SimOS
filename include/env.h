#ifndef ENV_H
#define ENV_H

#define DEFAULT_CONFIG_PATH "config.yaml"

typedef struct {
    char db_path[256];
    char log_path[256];
    int listen_port;
} Config;

Config* config_load(const char *path);

#endif