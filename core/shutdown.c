#include "../include/logging.h"
#include "../include/loop.h"
#include "../include/shutdown.h"

void shutdown_gracefully(void) {
    log_info("Shutdown requested");
    request_event_loop_stop();
}
