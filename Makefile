CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -O2 $(shell pkg-config --cflags sdl3)
LDFLAGS ?= $(shell pkg-config --libs sdl3)

BIN = civlin
SRC = src/main.c

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
