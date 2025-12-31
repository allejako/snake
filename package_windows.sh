#!/bin/bash

# Script to package Windows build for distribution

set -e

echo "=== Snake Game - Windows Package Builder ==="
echo

# Check if Windows build exists
if [ ! -f "bin_windows/snake.exe" ]; then
    echo "Error: Windows build not found!"
    echo "Please run: make -f makefile.windows"
    exit 1
fi

# Create packaging directory
PACKAGE_DIR="snake_windows_package"
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

echo "Creating package directory..."

# Copy executable
cp bin_windows/snake.exe "$PACKAGE_DIR/"

# Copy assets
if [ -d "assets" ]; then
    cp -r assets "$PACKAGE_DIR/"
    echo "Copied assets/"
fi

# Copy DLLs from MinGW
DLL_SOURCE="/usr/x86_64-w64-mingw32/bin"
if [ -d "$DLL_SOURCE" ]; then
    echo "Copying SDL2 DLLs..."
    cp "$DLL_SOURCE/SDL2.dll" "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: SDL2.dll not found"
    cp "$DLL_SOURCE/SDL2_ttf.dll" "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: SDL2_ttf.dll not found"
    cp "$DLL_SOURCE/libfreetype-6.dll" "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libfreetype-6.dll not found"
    cp "$DLL_SOURCE"/zlib*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: zlib DLL not found"
    cp "$DLL_SOURCE"/libpng*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libpng DLL not found"
    cp "$DLL_SOURCE"/libbz2*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libbz2 DLL not found"
    cp "$DLL_SOURCE"/libharfbuzz*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libharfbuzz DLL not found"
    cp "$DLL_SOURCE"/libglib*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libglib DLL not found"
    cp "$DLL_SOURCE"/libgraphite2*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libgraphite2 DLL not found"
    cp "$DLL_SOURCE"/libintl*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libintl DLL not found"
    cp "$DLL_SOURCE"/libiconv*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libiconv DLL not found"
    cp "$DLL_SOURCE"/libpcre*.dll "$PACKAGE_DIR/" 2>/dev/null || echo "  Warning: libpcre DLL not found"
fi

# Create README for Windows users
cat > "$PACKAGE_DIR/README.txt" << 'EOF'
Snake Game - Windows Edition
============================

## How to Run

Simply double-click snake.exe to start the game!

## Controls

Menu Navigation:
- Arrow keys: Navigate menus
- Enter: Select
- Escape: Go back

Gameplay (Singleplayer):
- Arrow keys or WASD: Move snake
- Escape: Pause game

Gameplay (Local Multiplayer):
- Player 1: Arrow keys
- Player 2: WASD
- Escape: Pause game

## Features

- Singleplayer mode with combo system and speed effects
- Local multiplayer (up to 4 players on same keyboard)
- Configurable keybindings
- Sound effects and music
- High score tracking
- Customizable settings

## Notes

- Online multiplayer is not available in the Windows version
- All game settings are saved in settings.dat
- High scores are stored in scoreboard.dat

## Troubleshooting

If the game doesn't start:
1. Make sure all DLL files are in the same folder as snake.exe
2. Install Visual C++ Redistributable if prompted
3. Check that your graphics drivers are up to date

For issues or feedback, visit: [Your GitHub/project URL]

Enjoy the game!
EOF

echo "Created README.txt"

# Create ZIP file
ZIP_NAME="snake_windows.zip"
rm -f "$ZIP_NAME"

echo "Creating ZIP archive..."
(cd "$PACKAGE_DIR" && zip -r ../"$ZIP_NAME" .)

echo
echo "=== Package Complete! ==="
echo "Created: $ZIP_NAME"
echo "Size: $(du -h "$ZIP_NAME" | cut -f1)"
echo
echo "Package contents:"
unzip -l "$ZIP_NAME" | tail -n +4 | head -n -2
echo
echo "Ready for distribution!"
