#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/cli.h"
#include "../include/logging.h"
#include "../include/node_manager.h"
#include "../include/shutdown.h"
#include "../include/db.h"
#include "../include/json.h"

CliArgs parse_cli_args(int argc, char **argv) {
    CliArgs args = (CliArgs){0};
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            args.config = argv[++i];
        }
    }
    return args;
}

static void print_help(void) {
    puts("\nSimOS kernel commands:\n"
         "  help                         Show this command guide\n"
         "  nodes                        List connected child nodes\n"
         "  status [node|all]            Inspect node CPU/memory/uptime\n"
         "  ping <node>                  Check whether a node responds\n"
         "  exec <node> <command>        Run a process on one node\n"
         "  broadcast <command>          Run a process on every node\n"
         "  history                      Show the persistent event journal\n"
         "  clear                        Clear the terminal\n"
         "  shutdown | exit | quit       Stop the kernel cleanly\n");
}

static int send_exec(NodeSession *session, const char *cmd_text) {
    static unsigned int counter;
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    char id[64]; snprintf(id, sizeof(id), "%lld_%u",
        (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000, ++counter);
    char *escaped = json_escape(cmd_text); if (!escaped) return -1;
    size_t capacity = strlen(escaped) + 160;
    char *payload = malloc(capacity); if (!payload) { free(escaped); return -1; }
    snprintf(payload, capacity, "{\"type\":\"exec\",\"id\":\"%s\",\"cmd\":\"%s\"}", id, escaped);
    int result = node_session_send(session, payload) < 0 ? -1 : 0;
    if (result == 0) {
        session->last_seen = time(NULL); db_store_event(session->meta.name, "exec", cmd_text);
        printf("Scheduled process %s on %s\n", id, session->meta.name); fflush(stdout);
    }
    free(payload); free(escaped); return result;
}

void parse_cli_command(const char *input_line, void *opaque_state) {
    (void)opaque_state;
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

    if (strcmp(verb, "help") == 0) {
        print_help();
    } else if (strcmp(verb, "nodes") == 0) {
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
    } else if (strcmp(verb, "status") == 0) {
        char *name = strtok_r(NULL, " ", &saveptr);
        if (!name || strcmp(name, "all") == 0) {
            NodeSession snapshot[MAX_SESSIONS]; int count = node_sessions_copy(snapshot, MAX_SESSIONS);
            if (!count) puts("No child nodes are connected.");
            for (int i = 0; i < count; i++) node_session_send(node_session_find_by_name(snapshot[i].meta.name), "{\"type\":\"status\"}");
        } else {
            NodeSession *session = node_session_find_by_name(name);
            if (!session) log_error("Node not found: %s", name);
            else node_session_send(session, "{\"type\":\"status\"}");
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

        if (send_exec(session, cmd_text) < 0) {
            log_error("Failed to send exec to %s", node_name);
        }
    } else if (strcmp(verb, "broadcast") == 0) {
        char *cmd = saveptr; while (cmd && isspace((unsigned char)*cmd)) cmd++;
        if (!cmd || !*cmd) log_error("Usage: broadcast <command>");
        else {
            NodeSession snapshot[MAX_SESSIONS]; int count = node_sessions_copy(snapshot, MAX_SESSIONS);
            for (int i = 0; i < count; i++) send_exec(node_session_find_by_name(snapshot[i].meta.name), cmd);
            if (!count) puts("No child nodes are connected.");
        }
    } else if (strcmp(verb, "history") == 0) {
        db_list_events();
    } else if (strcmp(verb, "clear") == 0) {
        fputs("\033[2J\033[H", stdout); fflush(stdout);
    } else if (strcmp(verb, "exit") == 0 || strcmp(verb, "quit") == 0 || strcmp(verb, "shutdown") == 0) {
        log_info("Exit command received");
        free(line);
        shutdown_gracefully();
        return;
    } else {
        log_error("Unknown command: %s", verb);
    }

    free(line);
}
