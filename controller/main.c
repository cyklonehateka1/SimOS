#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/cli.h"
#include "../include/controller.h"
#include "../include/env.h"
#include "../include/logging.h"
#include "../include/loop.h"
#include "../include/init.h"

GlobalState global_state = {0};

int main(int argc, char **argv) {
    printf("Starting SimOS Controller.. \n");

    CliArgs args = parse_cli_args(argc, argv);

    global_state = init_global_state();

    const char *config_path = args.config ? args.config : DEFAULT_CONFIG_PATH;
    Config *config = config_load(config_path);
    if (!config) {
        fprintf(stderr, "Failed to load configuration from %s\n", config_path);
        exit(EXIT_FAILURE);
    }

    global_state.config = config;

    if (!log_init(config->log_path)) {
        fprintf(stderr, "Failed to initialize logger\n");
        exit(EXIT_FAILURE);
    }
    log_info("Logger initialized");

    if (!init_ipc_channels()) {
        log_error("IPC initialization failed");
        exit(EXIT_FAILURE);
    }

    log_info("Initialization complete. Entering main event loop...");
    run_event_loop(&global_state);

    log_info("Shutting down SimOS...");
    log_close();

    return EXIT_SUCCESS;
}
