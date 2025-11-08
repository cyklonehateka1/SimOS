#include <stdlib.h>
#include "../include/logging.h"
#include "../include/env.h"
#include "../include/shutdown.h"

void shutdown_gracefully(void) {
    log_info("Shutting down SimOS gracefully...");

    log_info("Shutdown complete. Goodbye!");
    exit(0);
}
