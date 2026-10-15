CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -O2 $(shell pkg-config --cflags sdl3)
LDFLAGS ?= $(shell pkg-config --libs sdl3)

BIN = civlin
SRC = src/main.c src/civ_common.c src/save.c src/game.c src/map.c src/dialog.c src/video.c src/audio.c src/rsc_loader.c src/gfx_indexed.c src/boot.c src/title.c src/mainmenu.c src/newgame.c src/halloffame.c src/worldgen.c

DUMP_BIN = rsc_dump
DUMP_SRC = src/rsc_dump.c src/rsc_loader.c src/gfx_indexed.c

all: $(BIN) $(DUMP_BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(DUMP_BIN): $(DUMP_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(BIN)
	./$(BIN)

run-dump: $(DUMP_BIN)
	./$(DUMP_BIN)

clean:
	rm -f $(BIN) $(DUMP_BIN)
