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
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fprintf(stderr, "Failed to initialize YAML parser\n");
        fclose(fh);
        return NULL;
    }
    yaml_parser_set_input_file(&parser, fh);

    Config *cfg = calloc(1, sizeof(Config));
    char key[256] = {0};
    int node_index = -1;
    bool in_nodes_seq = false;

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "YAML parsing error\n");
            yaml_parser_delete(&parser);
            fclose(fh);
            free(cfg);
            return NULL;
        }

        switch (event.type) {
            case YAML_SCALAR_EVENT:
                if (!key[0]) {
                    // First scalar = key
                    strcpy(key, (char *)event.data.scalar.value);
                } else {
                    // Second scalar = value
                    if (!in_nodes_seq) {
                        if (strcmp(key, "db_path") == 0)
                            strcpy(cfg->db_path, (char *)event.data.scalar.value);
                        else if (strcmp(key, "log_path") == 0)
                            strcpy(cfg->log_path, (char *)event.data.scalar.value);
                        else if (strcmp(key, "listen_port") == 0)
                            cfg->listen_port = atoi((char *)event.data.scalar.value);
                    } else {
                        // Inside a node object
                        if (strcmp(key, "name") == 0)
                            strcpy(cfg->nodes[node_index].name, (char *)event.data.scalar.value);
                        else if (strcmp(key, "address") == 0)
                            strcpy(cfg->nodes[node_index].address, (char *)event.data.scalar.value);
                        else if (strcmp(key, "os") == 0)
                            strcpy(cfg->nodes[node_index].os, (char *)event.data.scalar.value);
                    }
                    key[0] = '\0';
                }
                break;

            case YAML_SEQUENCE_START_EVENT:
                if (strcmp(key, "nodes") == 0) {
                    in_nodes_seq = true;
                    node_index = -1;
                    key[0] = '\0';
                }
                break;

            case YAML_MAPPING_START_EVENT:
                if (in_nodes_seq)
                    node_index++;
                break;

            case YAML_SEQUENCE_END_EVENT:
                in_nodes_seq = false;
                break;

            case YAML_STREAM_END_EVENT:
                yaml_event_delete(&event);
                goto done;

            default:
                break;
        }

        yaml_event_delete(&event);
    }

done:
    yaml_parser_delete(&parser);
    fclose(fh);
    return cfg;
}
