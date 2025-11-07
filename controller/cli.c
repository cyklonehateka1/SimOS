#include <string.h>
#include <stdlib.h>
#include "../include/cli.h"
#include "../include/logging.h"
#include "../include/loop.h"
#include "../include/node_manager.h"
#include "../include/shutdown.h"
#include "../include/utils/util_fns.h"

extern GlobalState global_state;

CliArgs parse_cli_args(int argc, char **argv) {
    CliArgs args = {0};
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            args.config = argv[++i];
    }
    return args;
}

void parse_cli_command(const char *input_line) {
    if (input_line == NULL || strlen(input_line) == 0) {
        log_info("Empty command");
        return;
    }

    while (*input_line == ' ') input_line++;

    if (strncmp(input_line, "nodes", 5) == 0 || strcmp(input_line, "nodes list") == 10) {
       NodeSession sessions[MAX_SESSIONS];

       for (int i = 0; i < MAX_SESSIONS; i++){
            log_info(sessions->meta.name);
       }
    }else if (strncmp(input_line, "ping", 4) == 0) {
        NodeSession sessions[MAX_SESSIONS];

        char *cmd_copy = strdup(input_line);
        char *token = strtok(cmd_copy, " ");
        char *nodename = strtok(NULL, " ");

        if (nodename == NULL) {
            log_error("Usage: ping <nodename>");
            exit(-1);
        }
        NodeSession *session = node_session_find_by_name(nodename);

        if (session == NULL) {
            log_error("Node not found: %s", nodename);
        } else {
            const char *msg = "{\"type\":\"ping\"}\n";
            node_session_send(session, msg);
            log_info("Ping sent to %s", nodename);
        }
    }else if (strncmp(input_line, "node", 4) == 0) {
        execute_node_command(&global_state, input_line);
    }
    else if(strncmp(input_line, "exec", 4) == 0){
        char *cmd_copy = strdup(input_line);
        char *token = strtok(cmd_copy, " ");
        char *nodename = strtok(NULL, " ");

         const char *first_space = strchr(input_line, ' ');
        if (!first_space) {
            log_error("No command supplied after node name");
            free(nodename);
            return;
        }

        const char *actual_cmd = first_space + 1;

        NodeSession session = node_session_find_by_name(nodename);

        if (session == NULL){
            log_error("Node session not found");
        }
    }
    else if (strcmp(input_line, "exit") == 0 || strcmp(input_line, "quit")) {
        shutdown_gracefully();
    }
    else {
        log_info("Unknown command");
    }
}