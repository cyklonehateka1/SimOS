#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/cli.h"
#include "../include/logging.h"
#include "../include/node_manager.h"
#include "../include/shutdown.h"

static char *escape_json_string(const char *input) {
    if (!input) return strdup("");
    size_t len = strlen(input);
    size_t cap = len * 2 + 1;
    char *out = malloc(cap);
    if (!out) return strdup("");

    size_t o = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = input[i];
        if (c == '"' || c == '\\') {
            if (o + 2 >= cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) break;
                out = tmp;
            }
            out[o++] = '\\';
            out[o++] = c;
        } else if (c == '\n') {
            if (o + 2 >= cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) break;
                out = tmp;
            }
            out[o++] = '\\';
            out[o++] = 'n';
        } else if (c == '\r') {
            if (o + 2 >= cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) break;
                out = tmp;
            }
            out[o++] = '\\';
            out[o++] = 'r';
        } else if (c == '\t') {
            if (o + 2 >= cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) break;
                out = tmp;
            }
            out[o++] = '\\';
            out[o++] = 't';
        } else {
            if (o + 1 >= cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) break;
                out = tmp;
            }
            out[o++] = c;
        }
    }
    out[o] = '\0';
    return out;
}

CliArgs parse_cli_args(int argc, char **argv) {
    CliArgs args = (CliArgs){0};
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            args.config = argv[++i];
        }
    }
    return args;
}

void parse_cli_command(const char *input_line) {
    if (!input_line) {
        log_info("Empty command");
        return;
    }

    char *line = strdup(input_line);
    if (!line) return;

    char *cursor = line;
    while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    if (*cursor == '\0') {
        log_info("Empty command");
        free(line);
        return;
    }

    char *saveptr = NULL;
    char *verb = strtok_r(cursor, " ", &saveptr);
    if (!verb) {
        log_info("Empty command");
        free(line);
        return;
    }

    if (strcmp(verb, "nodes") == 0) {
        NodeSession snapshot[MAX_SESSIONS];
        int count = node_sessions_copy(snapshot, MAX_SESSIONS);

        printf("Connected nodes (%d):\n", count);
        if (count == 0) {
            printf("  <none>\n");
        } else {
            for (int i = 0; i < count; ++i) {
                printf("  - %s (fd=%d, os=%s)\n",
                       snapshot[i].meta.name[0] ? snapshot[i].meta.name : "<unnamed>",
                       snapshot[i].fd,
                       snapshot[i].meta.os[0] ? snapshot[i].meta.os : "unknown");
            }
        }
        fflush(stdout);
    } else if (strcmp(verb, "ping") == 0) {
        char *node_name = strtok_r(NULL, " ", &saveptr);
        if (!node_name) {
            log_error("Usage: ping <node-name>");
            free(line);
            return;
        }

        NodeSession *session = node_session_find_by_name(node_name);
        if (!session) {
            log_error("Node not found: %s", node_name);
            free(line);
            return;
        }

        const char *ping_msg = "{\"type\":\"ping\"}";
        if (node_session_send(session, ping_msg) < 0) {
            log_error("Failed to send ping to %s", node_name);
        } else {
            session->last_seen = time(NULL);
            log_info("Ping sent to %s", node_name);
        }
    } else if (strcmp(verb, "exec") == 0) {
        char *node_name = strtok_r(NULL, " ", &saveptr);
        if (!node_name) {
            log_error("Usage: exec <node-name> <command>");
            free(line);
            return;
        }

        char *cmd_text = saveptr;
        while (cmd_text && isspace((unsigned char)*cmd_text)) cmd_text++;
        if (!cmd_text || *cmd_text == '\0') {
            log_error("No command provided for exec");
            free(line);
            return;
        }

        NodeSession *session = node_session_find_by_name(node_name);
        if (!session) {
            log_error("Node not found: %s", node_name);
            free(line);
            return;
        }

        static unsigned int counter = 0;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        long long timestamp_ms = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
        counter++;

        char id[64];
        snprintf(id, sizeof(id), "%lld_%u", timestamp_ms, counter);

        char *escaped_cmd = escape_json_string(cmd_text);
        if (!escaped_cmd) {
            log_error("Failed to allocate command buffer");
            free(line);
            return;
        }

        char payload[1024];
        int written = snprintf(payload, sizeof(payload),
                               "{\"type\":\"exec\",\"id\":\"%s\",\"cmd\":\"%s\"}",
                               id, escaped_cmd);
        free(escaped_cmd);

        if (written < 0 || written >= (int)sizeof(payload)) {
            log_error("Command payload too large to send");
            free(line);
            return;
        }

        if (node_session_send(session, payload) < 0) {
            log_error("Failed to send exec to %s", node_name);
        } else {
            session->last_seen = time(NULL);
            log_info("Sent command id=%s to %s", id, node_name);
        }
    } else if (strcmp(verb, "exit") == 0 || strcmp(verb, "quit") == 0) {
        log_info("Exit command received");
        free(line);
        shutdown_gracefully();
        return;
    } else {
        log_error("Unknown command: %s", verb);
    }

    free(line);
}
