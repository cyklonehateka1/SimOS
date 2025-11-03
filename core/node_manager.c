#include "../include/env.h"
#include "../include/loop.h"
#include "../include/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


char *get_token(const char *input, const char *delimiter, int index) {
    if (!input || !delimiter) return NULL;
    char *copy = strdup(input);
    if (!copy) return NULL;

    char *saveptr = NULL;
    char *tok = strtok_r(copy, delimiter, &saveptr);
    int i = 0;
    while (tok) {
        if (i == index) {
            char *result = strdup(tok);
            free(copy);
            return result;
        }
        tok = strtok_r(NULL, delimiter, &saveptr);
        i++;
    }

    free(copy);
    return NULL;
}

Node *load_nodes_from_config(Config *config, int *out_count) {
    if (!config || !out_count) return NULL;

    int count = 0;
    while (count < MAX_NODES && config->nodes[count].name[0] != '\0') {
        count++;
    }

    *out_count = count;
    if (count == 0) return NULL;

    Node *nodes = malloc(sizeof(Node) * count);
    if (!nodes) return NULL;

    for (int i = 0; i < count; i++) {
        // Defensive copy: ensure strings are null-terminated and sized properly
        strncpy(nodes[i].name, config->nodes[i].name, sizeof(nodes[i].name) - 1);
        nodes[i].name[sizeof(nodes[i].name) - 1] = '\0';

        strncpy(nodes[i].address, config->nodes[i].address, sizeof(nodes[i].address) - 1);
        nodes[i].address[sizeof(nodes[i].address) - 1] = '\0';

        strncpy(nodes[i].os, config->nodes[i].os, sizeof(nodes[i].os) - 1);
        nodes[i].os[sizeof(nodes[i].os) - 1] = '\0';
    }

    return nodes;
}

Node *find_node_by_name(Config *config, const char *name) {
    if (!config || !name) return NULL;

    for (int i = 0; i < MAX_NODES && config->nodes[i].name[0] != '\0'; i++) {
        if (strcmp(config->nodes[i].name, name) == 0) {
            Node *result = malloc(sizeof(Node));
            if (!result) return NULL;
            strncpy(result->name, config->nodes[i].name, sizeof(result->name) - 1);
            result->name[sizeof(result->name) - 1] = '\0';
            strncpy(result->address, config->nodes[i].address, sizeof(result->address) - 1);
            result->address[sizeof(result->address) - 1] = '\0';
            strncpy(result->os, config->nodes[i].os, sizeof(result->os) - 1);
            result->os[sizeof(result->os) - 1] = '\0';
            return result;
        }
    }

    return NULL;
}

char *send_command_to_node(const Node *node, const char *command) {
    if (!node || !command) return NULL;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_error("socket() failed: %s", strerror(errno));
        return NULL;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(DEFAULT_NODE_PORT); 
    if (inet_pton(AF_INET, node->address, &srv.sin_addr) != 1) {
        log_error("inet_pton failed for address %s", node->address);
        close(sock);
        return NULL;
    }

    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        log_error("connect to %s failed: %s", node->address, strerror(errno));
        close(sock);
        return NULL;
    }

    size_t to_send = strlen(command);
    ssize_t sent_total = 0;
    while ((size_t)sent_total < to_send) {
        ssize_t s = send(sock, command + sent_total, to_send - sent_total, 0);
        if (s < 0) {
            log_error("send() failed: %s", strerror(errno));
            close(sock);
            return NULL;
        }
        sent_total += s;
    }

    size_t buf_cap = 1024;
    size_t buf_len = 0;
    char *buf = malloc(buf_cap);
    if (!buf) {
        log_error("malloc failed");
        close(sock);
        return NULL;
    }

    while (1) {
        ssize_t n = recv(sock, buf + buf_len, buf_cap - buf_len - 1, 0);
        if (n < 0) {
            log_error("recv() failed: %s", strerror(errno));
            free(buf);
            close(sock);
            return NULL;
        } else if (n == 0) {
            break;
        } else {
            buf_len += (size_t)n;
            if (buf_cap - buf_len - 1 < 256) {
                buf_cap *= 2;
                char *new_buf = realloc(buf, buf_cap);
                if (!new_buf) {
                    log_error("realloc failed");
                    free(buf);
                    close(sock);
                    return NULL;
                }
                buf = new_buf;
            }
        }
    }

    buf[buf_len] = '\0';
    close(sock);
    return buf;
}

void execute_node_command(GlobalState *global_state, const char *input_command) {
    if (!global_state || !input_command) return;

    char *node_name = get_token(input_command, " ", 0);
    if (!node_name) {
        log_error("No node name found in command");
        return;
    }

    const char *first_space = strchr(input_command, ' ');
    if (!first_space) {
        log_error("No command supplied after node name");
        free(node_name);
        return;
    }

    const char *actual_cmd = first_space + 1;
    if (*actual_cmd == '\0') {
        log_error("Empty command to execute");
        free(node_name);
        return;
    }

    Node *node = find_node_by_name(global_state->config, node_name);
    if (!node) {
        log_error("Node not found: %s", node_name);
        free(node_name);
        return;
    }

    char *response = send_command_to_node(node, actual_cmd);
    if (response) {
        log_info("Response from %s: %s", node->name, response);
        // TODO: store to DB: db_store_command(node->name, actual_cmd, response);
        free(response);
    } else {
        log_error("Failed to get response from node %s", node->name);
    }

    free(node);
    free(node_name);
}

bool starts_with(const char *str, const char *prefix) {
    size_t len_prefix = strlen(prefix);
    size_t len_str = strlen(str);
    return len_str < len_prefix ? false : strncmp(str, prefix, len_prefix) == 0;
}

