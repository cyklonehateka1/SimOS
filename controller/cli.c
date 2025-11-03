#include <string.h>
#include <stdlib.h>
#include "../include/cli.h"
#include "../include/logging.h"
#include "../include/loop.h"
#include "../include/node_manager.h"
#include "../include/shutdown.h"

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

    if (strncmp(input_line, "node", 4) == 0) {
    execute_node_command(&global_state, input_line);
    }
    else if (strcmp(input_line, "exit") == 0) {
        shutdown_gracefully();
    }
    else {
        log_info("Unknown command");
    }
}