#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/cli.h"
#include "../include/controller.h"
#include "../include/env.h"
#include "../include/logging.h"
#include "../include/loop.h"
#include "../include/init.h"

int main(int argc, char **argv) {
    printf("Starting SimOS Controller.. \n");

    // Step 1: Parse CLI arguments
    CliArgs args = parse_cli_args(argc, argv);

    // Step 2: Initialize global state
    GlobalState global_state = init_global_state();

    // Step 3: Load configuration
    const char *config_path = args.config ? args.config : DEFAULT_CONFIG_PATH;
    Config *config = config_load(config_path);
    if (!config) {
        fprintf(stderr, "Failed to load configuration from %s\n", config_path);
        exit(EXIT_FAILURE);
    }

    global_state.config = config;

    // Step 4: Initialize logger
    if (!log_init(config->log_path)) {
        fprintf(stderr, "Failed to initialize logger\n");
        exit(EXIT_FAILURE);
    }
    log_info("Logger initialized");

    // Step 5: Initialize subsystems
    if (!init_ipc_channels()) {
        log_error("IPC initialization failed");
        exit(EXIT_FAILURE);
    }

    // Step 6: Start main loop
    log_info("Initialization complete. Entering main event loop...");
    run_event_loop(&global_state);

    // Step 7: Graceful shutdown
    log_info("Shutting down SimOS...");
    log_close();

    return EXIT_SUCCESS;
}