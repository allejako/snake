# Windows Release Quick Start

This guide will help you create a distributable Windows build from WSL2/Linux.

## Quick Build (3 commands)

If you have the prerequisites installed:

```bash
./build_windows.sh        # Build the Windows executable
./package_windows.sh      # Create distribution ZIP
```

The result will be `snake_windows.zip` ready for distribution!

## First Time Setup

### 1. Install MinGW-w64 (Cross-Compiler)

```bash
sudo apt-get update
sudo apt-get install mingw-w64
```

### 2. Download SDL2 Libraries for Windows

You need two packages:

**SDL2:**
- Go to: https://github.com/libsdl-org/SDL/releases/latest
- Download: `SDL2-devel-*-mingw.tar.gz`

**SDL2_ttf:**
- Go to: https://github.com/libsdl-org/SDL_ttf/releases/latest
- Download: `SDL2_ttf-devel-*-mingw.tar.gz`

### 3. Install SDL2 Libraries

```bash
# SDL2
tar -xzf SDL2-devel-*-mingw.tar.gz
cd SDL2-*/x86_64-w64-mingw32
sudo cp -r include/* /usr/x86_64-w64-mingw32/include/
sudo cp -r lib/* /usr/x86_64-w64-mingw32/lib/
sudo cp -r bin/*.dll /usr/x86_64-w64-mingw32/bin/
cd ../..

# SDL2_ttf
tar -xzf SDL2_ttf-devel-*-mingw.tar.gz
cd SDL2_ttf-*/x86_64-w64-mingw32
sudo cp -r include/* /usr/x86_64-w64-mingw32/include/
sudo cp -r lib/* /usr/x86_64-w64-mingw32/lib/
sudo cp -r bin/*.dll /usr/x86_64-w64-mingw32/bin/
cd ../..
```

### 4. Build and Package

```bash
./build_windows.sh        # Checks dependencies and builds
./package_windows.sh      # Creates distribution ZIP
```

## What Gets Included

The `snake_windows.zip` file will contain:

- `snake.exe` - The game executable
- `SDL2.dll` - SDL2 library
- `SDL2_ttf.dll` - SDL2_ttf library
- Various support DLLs (freetype, zlib, etc.)
- `assets/` folder - Fonts and audio files
- `README.txt` - Instructions for Windows users

## Windows Build Differences

The Windows build has these differences from the Linux version:

- **No Online Multiplayer**: The mpapi library is Linux-only, so online multiplayer is completely disabled
- **Singleplayer**: Works exactly the same
- **Local Multiplayer**: Works exactly the same
- **Settings & Scoreboard**: Work exactly the same

## Testing the Build

You can test the Windows build in WSL if you have an X server running:

```bash
cd snake_windows_package
wine snake.exe  # If wine is installed
```

Or copy the entire `snake_windows_package` folder to a Windows machine to test.

## Distribution

The `snake_windows.zip` file is ready to distribute. Users just:
1. Extract the ZIP
2. Double-click `snake.exe`
3. Play!

No installation required, all DLLs are included.

## Troubleshooting

**Build fails with "command not found":**
- Make sure mingw-w64 is installed: `which x86_64-w64-mingw32-gcc`

**Build fails with "SDL2.h: No such file":**
- SDL2 headers not installed correctly
- Check `/usr/x86_64-w64-mingw32/include/SDL2/` exists

**Package script can't find DLLs:**
- SDL2 DLLs not in `/usr/x86_64-w64-mingw32/bin/`
- Re-install SDL2 binaries per step 3 above

**Game crashes on Windows:**
- Missing DLL - check all DLLs are in same folder as snake.exe
- User may need Visual C++ Redistributable

## File Structure After Packaging

```
snake_windows_package/
├── snake.exe           # Main executable
├── SDL2.dll           # SDL2 library
├── SDL2_ttf.dll       # Font rendering
├── *.dll              # Various dependencies
├── README.txt         # User instructions
└── assets/
    ├── fonts/         # Game fonts
    └── sounds/        # Audio files
```
