#include <stdlib.h>
#include "../include/logging.h"
#include "../include/shutdown.h"

void shutdown_gracefully(void) {
    log_info("Shutting down SimOS gracefully...");

    // TODO: Add cleanup logic here when GlobalState has dynamically allocated data

    log_info("Shutdown complete. Goodbye!");
    exit(0);
}
