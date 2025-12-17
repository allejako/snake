CC := gcc

CFLAGS := -Wall -Wextra -std=c11 -O2 -g -Iinclude
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
TTF_CFLAGS := $(shell pkg-config --cflags SDL2_ttf)
TTF_LIBS   := $(shell pkg-config --libs SDL2_ttf)
# SDL_mixer removed - using simple_audio instead for better WSL2 compatibility

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
INCLUDE_DIR := include

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

BIN := $(BIN_DIR)/snake_sdl.exe

EXTRA_LIBS := -ljansson

.PHONY: all clean run

all: $(BIN)

# ---- Linking ----
$(BIN): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(SDL_LIBS) $(TTF_LIBS) $(EXTRA_LIBS)

# ---- Compilation: src/foo.c -> build/foo.o ----
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(TTF_CFLAGS) -c $< -o $@

# ---- Create directories if they don't exist ----
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
