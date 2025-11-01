#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>
#include <stdbool.h>
#include "../../include/env.h"

Config* config_load(const char *path) {
    FILE *fh = fopen(path, "r");
    if (!fh) {
        fprintf(stderr, "Cannot open config file: %s\n", path);
        return NULL;
    }

    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fh);

    yaml_event_t event;
    Config *cfg = calloc(1, sizeof(Config));
    char key[256];

    while (true) {
        yaml_parser_parse(&parser, &event);
        if (event.type == YAML_SCALAR_EVENT) {
            if (!key[0]) strcpy(key, (char *)event.data.scalar.value);
            else {
                if (strcmp(key, "db_path") == 0)
                    strcpy(cfg->db_path, (char *)event.data.scalar.value);
                else if (strcmp(key, "log_path") == 0)
                    strcpy(cfg->log_path, (char *)event.data.scalar.value);
                else if (strcmp(key, "listen_port") == 0)
                    cfg->listen_port = atoi((char *)event.data.scalar.value);
                key[0] = '\0';
            }
        } else if (event.type == YAML_STREAM_END_EVENT)
            break;
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);
    return cfg;
}