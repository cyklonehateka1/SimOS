CC = gcc
CFLAGS = -Iinclude -Wall -Wextra
LDFLAGS = -lyaml
SRC = $(wildcard controller/*.c core/*.c core/config/*.c core/ipc/*.c common/utils/*.c)
OUT = simos

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -I/opt/homebrew/opt/libyaml/include
    LDFLAGS += -L/opt/homebrew/opt/libyaml/lib
endif

$(OUT): $(SRC)
	$(CC) $(SRC) $(CFLAGS) $(LDFLAGS) -o $(OUT)

clean:
	rm -f $(OUT)
