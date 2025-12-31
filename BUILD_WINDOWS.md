# Building for Windows

This guide explains how to create a Windows build of the Snake game from WSL2/Linux.

## Prerequisites

### 1. Install MinGW-w64 Cross-Compiler

```bash
sudo apt-get update
sudo apt-get install mingw-w64
```

### 2. Install Windows SDL2 Development Libraries

Download the MinGW development libraries:
- SDL2: https://github.com/libsdl-org/SDL/releases (SDL2-devel-*-mingw.tar.gz)
- SDL2_ttf: https://github.com/libsdl-org/SDL_ttf/releases (SDL2_ttf-devel-*-mingw.tar.gz)

Extract them and install to `/usr/x86_64-w64-mingw32/`:

```bash
# Extract SDL2
tar -xzf SDL2-devel-*-mingw.tar.gz
cd SDL2-*/x86_64-w64-mingw32
sudo cp -r include/* /usr/x86_64-w64-mingw32/include/
sudo cp -r lib/* /usr/x86_64-w64-mingw32/lib/
sudo cp -r bin/*.dll /usr/x86_64-w64-mingw32/bin/

# Extract SDL2_ttf
tar -xzf SDL2_ttf-devel-*-mingw.tar.gz
cd SDL2_ttf-*/x86_64-w64-mingw32
sudo cp -r include/* /usr/x86_64-w64-mingw32/include/
sudo cp -r lib/* /usr/x86_64-w64-mingw32/lib/
sudo cp -r bin/*.dll /usr/x86_64-w64-mingw32/bin/
```

## Building

```bash
make -f makefile.windows
```

This will create:
- `bin_windows/snake.exe` - The game executable
- All object files in `build_windows/`

## Packaging for Distribution

Run the packaging script:

```bash
./package_windows.sh
```

This creates `snake_windows.zip` containing:
- snake.exe
- All required SDL2 DLLs
- assets/ folder with fonts and sounds
- README_WINDOWS.txt

## Notes

- Online multiplayer is disabled in Windows builds (mpapi is Linux-only)
- The game will show "Multiplayer (Coming Soon)" instead of online multiplayer options
- Singleplayer, local multiplayer, and all other features work normally
