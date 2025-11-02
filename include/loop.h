#ifndef LOOP_H
#define LOOP_H

#include "env.h"  

typedef struct {
    Config *config;
} GlobalState;

GlobalState init_global_state(void);

void run_event_loop(GlobalState *state);

#endif 