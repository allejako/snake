# SnakeGPT

A classic Snake game implemented in C with SDL2, featuring customizable keybindings and local multiplayer support.

## Project Structure

```
SnakeGPT/
├── src/              # Source files (.c)
│   ├── main.c
│   ├── game.c
│   ├── snake.c
│   ├── board.c
│   ├── ui_sdl.c
│   ├── text_sdl.c
│   ├── keybindings.c
│   ├── scoreboard.c
│   └── input_buffer.c
├── include/          # Header files (.h)
│   ├── game.h
│   ├── snake.h
│   ├── board.h
│   ├── ui_sdl.h
│   ├── text_sdl.h
│   ├── keybindings.h
│   ├── scoreboard.h
│   ├── input_buffer.h
│   └── common.h
├── assets/           # Game assets
│   ├── fonts/
│   └── PTF-NORDIC-Rnd.ttf
├── data/             # Runtime data files
│   ├── scoreboard.csv
│   └── keybindings.ini
├── build/            # Compiled object files (generated)
├── bin/              # Compiled binary (generated)
│   └── snake_sdl
├── makefile
├── .gitignore
└── README.md
```

## Building

### Prerequisites
- GCC compiler
- SDL2 development libraries
- SDL2_ttf development libraries

### Compile
```bash
make
```

### Clean build artifacts
```bash
make clean
```

### Run the game
```bash
make run
# or directly:
./bin/snake_sdl
```

## Features

- **Singleplayer Mode**: Classic snake gameplay with scoring
- **Customizable Keybindings**: Remap controls for up to 4 players
  - Default Player 1: WASD + E
  - Default Player 2: Arrow keys + Right Shift
  - Default Player 3: IJKL + U
  - Default Player 4: TFGH + R
- **Persistent Scoreboard**: High scores saved to CSV
- **Input Buffering**: Queue up to 2 direction changes between game ticks
- **Pause Menu**: Pause during gameplay with ESC

## Controls

### Menu Navigation
- **UP/DOWN**: Navigate menu (uses Player 1's keybindings)
- **ENTER**: Select menu item
- **ESC**: Back/Quit

### Gameplay (Default Player 1)
- **W**: Move up
- **S**: Move down
- **A**: Move left
- **D**: Move right
- **ESC**: Pause game

## Configuration

### Keybindings
Edit keybindings through the in-game Options menu or manually edit `data/keybindings.ini`

### Scoreboard
High scores are stored in `data/scoreboard.csv`

## Code Overview

### Core Modules
- **game**: Game state management, collision detection, scoring
- **snake**: Snake movement, growth, and collision logic
- **board**: Game board and food placement
- **ui_sdl**: SDL rendering and UI system
- **keybindings**: Configurable key mapping with auto-swap conflict resolution
- **scoreboard**: High score persistence
- **input_buffer**: Input queue to prevent missed keypresses

### Architecture Highlights
- State machine pattern for app flow (menu, gameplay, options, etc.)
- Separated state handlers for maintainability
- AppContext struct bundles all state for clean function signatures
- INI-style configuration file format

## License

This project is open source and available for educational purposes.
