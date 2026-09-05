#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/cli.h"
#include "../include/env.h"
#include "../include/logging.h"
#include "../include/loop.h"
#include "../include/db.h"
#include <signal.h>

GlobalState global_state = {0};

static void on_signal(int signal_number) { (void)signal_number; request_event_loop_stop(); }

int main(int argc, char **argv) {
    printf("\n  ____  _          ___  ____\n"
           " / ___|(_)_ __ ___/ _ \\/ ___|\n"
           " \\___ \\| | '_ ` _ \\ | |\\___ \\\n"
           "  ___) | | | | | | |_| |___) |\n"
           " |____/|_|_| |_| |_|\\___/|____/\n"
           " Distributed miniature operating system\n\n");

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

    if (!db_init(config->db_path)) log_error("Event journal unavailable at %s", config->db_path);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    log_info("Initialization complete. Entering main event loop...");
    printf("Kernel online on TCP port %d. Type 'help' for commands.\n", config->listen_port);
    fflush(stdout);
    run_event_loop(&global_state);

    log_info("Shutting down SimOS...");
    db_close();
    config_free(config);
    log_close();

    return EXIT_SUCCESS;
}
