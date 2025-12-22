# Snake Game

A modern implementation of the classic Snake game in C with SDL2, featuring **online multiplayer**, customizable keybindings, lives system, and combo scoring.

## Features

### Core Gameplay
- **Singleplayer Mode**: Classic snake gameplay with scoring and death animations
- **Online Multiplayer**: Play with up to 4 players online with zero input lag
- **Lives System**: 3 lives per player with respawn mechanics
- **Combo Scoring**: Chain food pickups for score multipliers (2x, 3x, 4x)
- **Death Animations**: Snakes explode into food when dying
- **Customizable Keybindings**: Remap controls to your preference
- **Persistent Scoreboard**: High scores saved to CSV
- **Audio System**: Background music and sound effects with volume controls

### Multiplayer Features
- **Session-Based Rooms**: Create or join games with 6-character session IDs
- **Public/Private Games**: Choose visibility in server browser
- **Ready System**: Players ready up before game starts
- **Synchronized Countdown**: 3-2-1 countdown ensures simultaneous start
- **Lives-Based Gameplay**: Last player standing wins
- **Win Tracking**: Persistent win counts across multiple rounds
- **Smooth Remote Rendering**: 4x position updates per tick for fluid movement
- **Zero Input Lag**: Pure client-authoritative architecture

## Building

### Prerequisites
- GCC compiler
- SDL2 development libraries (`libsdl2-dev`)
- SDL2_ttf development libraries (`libsdl2-ttf-dev`)
- jansson library for JSON (`libjansson-dev`)
- Make

### Ubuntu/Debian Installation
```bash
sudo apt-get install build-essential libsdl2-dev libsdl2-ttf-dev libjansson-dev
```

### Compile
```bash
make              # Build the game
make run          # Build and run
make clean        # Remove build artifacts
```

### Run
```bash
./bin/snake_sdl.exe                # Normal mode
./bin/snake_sdl.exe --no-audio     # Disable audio (useful for WSL2)
./bin/snake_sdl.exe --help         # Show command-line options
```

## Controls

### Menu Navigation
- **Arrow Keys**: Navigate menu items
- **ENTER**: Select menu item
- **ESC**: Back/Quit

### Gameplay
- **Arrow Keys**: Move snake (UP/DOWN/LEFT/RIGHT)
- **ESC**: Pause game

### Multiplayer Lobby
- **Arrow Keys**: Navigate
- **SPACE**: Toggle ready status
- **ENTER**: Start game (host only, all players must be ready)
- **ESC**: Leave lobby

## How to Play

### Singleplayer
1. Select "Singleplayer" from main menu
2. Use arrow keys to control your snake
3. Eat food to grow and increase score
4. Avoid walls and your own tail
5. Game ends when you die - enter your name for the scoreboard

### Online Multiplayer

**Hosting a Game:**
1. Select "Online Multiplayer" from main menu
2. Choose "Host Game"
3. Select "Public" (visible in browser) or "Private" (requires session ID)
4. Share the session ID with other players
5. Press SPACE to ready up
6. Press ENTER when all players are ready to start

**Joining a Game:**
1. Select "Online Multiplayer" from main menu
2. Choose "Join Game"
3. Enter the 6-character session ID
4. Press SPACE to ready up
5. Wait for host to start the game

**During Multiplayer:**
- Each player starts with 3 lives
- When you die, your snake explodes into food
- You respawn after the death animation if you have lives remaining
- Last player with lives wins the round
- Win counts are tracked across multiple rounds

## Project Structure

```
snake/
├── src/                    # Source files
│   ├── main.c             # Main game loop and state machine
│   ├── game.c             # Singleplayer game logic
│   ├── multiplayer_game.c # Multiplayer game logic
│   ├── online_multiplayer.c # Network synchronization
│   ├── snake.c            # Snake movement and collision
│   ├── board.c            # Game board and food placement
│   ├── ui_sdl.c           # SDL rendering and UI
│   ├── audio_sdl.c        # Audio system
│   ├── keybindings.c      # Configurable controls
│   ├── scoreboard.c       # High score persistence
│   ├── input_buffer.c     # Input queueing
│   └── mpapi/             # Multiplayer API library
├── include/               # Header files
├── assets/                # Game assets
│   ├── fonts/            # Font files
│   ├── music/            # Background music
│   └── audio/            # Sound effects
├── data/                  # Runtime data files (auto-created)
│   ├── scoreboard.csv    # High scores
│   ├── keybindings.ini   # Key mappings
│   └── audio.ini         # Volume settings
├── build/                 # Compiled object files
├── bin/                   # Compiled binary
├── Makefile
├── CLAUDE.md             # Developer documentation
└── README.md             # This file
```

## Configuration

### Keybindings
- Edit through the in-game Options menu
- Or manually edit `data/keybindings.ini`
- Fully customizable controls

### Audio
- Adjust volumes through the Sound Settings menu
- Or manually edit `data/audio.ini`
- Separate controls for music and sound effects
- Use `--no-audio` flag to disable audio system

### Multiplayer Server
- Default server: `snake.bobbyfromboston.info:6969`
- Edit `include/config.h` to change server settings
- Requires recompilation after changes

## Technical Details

### Architecture
- **State Machine**: Clean separation of game states (menu, gameplay, lobby, etc.)
- **Pure Client Authoritative**: Zero input lag multiplayer architecture
- **Event-Driven Networking**: WebSocket-based with callback system
- **JSON Serialization**: Game state transmitted as compact JSON
- **Tick-Based Simulation**: 80ms tick rate for consistent gameplay

### Multiplayer Design
- Each client simulates only their own snake
- Host generates food and detects inter-player collisions
- 4x position updates per tick (every 20ms) for smooth rendering
- Clients send one-time notifications for events (death, respawn, food eaten)
- Synchronized countdown ensures all clients start simultaneously

### Performance
- Static memory allocation for game state
- Efficient JSON serialization with flat arrays
- Minimal network bandwidth (~200 bytes/tick for 4 players)
- Thread-based event listener for non-blocking network I/O

## Troubleshooting

### "Failed to connect to server"
- Check your internet connection
- Verify server is running and accessible
- Check firewall settings
- Confirm server address in `include/config.h`

### "Session not found"
- Session ID must be exactly 6 characters
- Session may have expired or been closed
- Ask host for current session ID

### Audio Issues (WSL2)
- Use `--no-audio` flag to disable audio
- Or ensure PulseAudio is configured in WSL2

### Build Errors
- Ensure all dependencies are installed
- Check SDL2 and jansson development packages
- Run `make clean` before rebuilding

## Credits

- **Font**: BBH Bogle (Regular)
- **Networking**: Custom MPAPI WebSocket library
- **Audio**: Custom SDL2-based audio system

## License

This project is open source and available for educational purposes.

## Contributing

Feel free to fork, modify, and submit pull requests. Areas for improvement:
- Additional game modes
- Power-ups and obstacles
- More visual effects
- AI opponents
- Tournament system
- Spectator mode
