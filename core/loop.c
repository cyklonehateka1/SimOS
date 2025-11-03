#include <stdio.h>
#include <unistd.h>
#include "../include/loop.h"
#include "../include/logging.h"

GlobalState init_global_state(void) {
    GlobalState state;
    state.config = NULL;
    return state;
}

void run_event_loop(GlobalState *state) {
    (void)state;
    int tick = 0;
    while (tick < 5) {
        log_info("[Tick %d] SimOS controller active...", tick);
        sleep(1);
        tick++;
    }
}
