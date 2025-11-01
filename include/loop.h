#ifndef LOOP_H
#define LOOP_H

#include "env.h"  // for Config

typedef struct {
    Config *config;
} GlobalState;

// Initializes and returns a GlobalState instance
GlobalState init_global_state(void);

// Runs the main event loop
void run_event_loop(GlobalState *state);

#endif // LOOP_H