CC ?= cc
CPPFLAGS = -Iinclude -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS = -lyaml
COMMON_SRC = core/ipc/init.c common/utils/logging.c common/utils/json.c
CONTROLLER_SRC = controller/main.c controller/cli.c core/loop.c core/node_manager.c core/shutdown.c core/config/env.c core/db/engine.c $(COMMON_SRC)
AGENT_SRC = node/main.c core/node_agent.c $(COMMON_SRC)

ifeq ($(shell uname -s),Darwin)
CPPFLAGS += -D_DARWIN_C_SOURCE -I/opt/homebrew/opt/libyaml/include
LDFLAGS += -L/opt/homebrew/opt/libyaml/lib
endif

.PHONY: all clean test
all: simos simos-agent

simos: $(CONTROLLER_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

simos-agent: $(AGENT_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: all
	./tests/integration.sh

clean:
	rm -f simos simos-agent
