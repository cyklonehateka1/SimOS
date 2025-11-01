CC = gcc
CFLAGS = -Iinclude -I/opt/homebrew/opt/libyaml/include -Wall -Wextra
LDFLAGS = -L/opt/homebrew/opt/libyaml/lib -lyaml
SRC = $(wildcard controller/*.c core/*.c core/config/*.c core/ipc/*.c common/utils/*.c)
OUT = simos

$(OUT): $(SRC)
	$(CC) $(SRC) $(CFLAGS) $(LDFLAGS) -o $(OUT)

clean:
	rm -f $(OUT)
