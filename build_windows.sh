#!/bin/bash

# Windows build script with dependency checking

set -e

echo "=== Snake Game - Windows Build Script ==="
echo

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for MinGW compiler
echo -n "Checking for MinGW-w64 compiler... "
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo -e "${RED}NOT FOUND${NC}"
    echo
    echo "MinGW-w64 is required to build for Windows."
    echo "Install it with:"
    echo "  sudo apt-get update"
    echo "  sudo apt-get install mingw-w64"
    echo
    exit 1
fi
echo -e "${GREEN}OK${NC}"

# Check for SDL2 headers
echo -n "Checking for SDL2 headers... "
SDL2_INCLUDE="/usr/x86_64-w64-mingw32/include/SDL2"
if [ ! -d "$SDL2_INCLUDE" ]; then
    echo -e "${RED}NOT FOUND${NC}"
    echo
    echo "SDL2 development libraries for MinGW are required."
    echo "Download from: https://github.com/libsdl-org/SDL/releases"
    echo "Look for: SDL2-devel-*-mingw.tar.gz"
    echo
    echo "Extract and install with:"
    echo "  tar -xzf SDL2-devel-*-mingw.tar.gz"
    echo "  cd SDL2-*/x86_64-w64-mingw32"
    echo "  sudo cp -r include/* /usr/x86_64-w64-mingw32/include/"
    echo "  sudo cp -r lib/* /usr/x86_64-w64-mingw32/lib/"
    echo "  sudo cp -r bin/*.dll /usr/x86_64-w64-mingw32/bin/"
    echo
    exit 1
fi
echo -e "${GREEN}OK${NC}"

# Check for SDL2_ttf headers
echo -n "Checking for SDL2_ttf headers... "
if [ ! -f "/usr/x86_64-w64-mingw32/include/SDL2/SDL_ttf.h" ]; then
    echo -e "${RED}NOT FOUND${NC}"
    echo
    echo "SDL2_ttf development libraries for MinGW are required."
    echo "Download from: https://github.com/libsdl-org/SDL_ttf/releases"
    echo "Look for: SDL2_ttf-devel-*-mingw.tar.gz"
    echo
    echo "Extract and install with:"
    echo "  tar -xzf SDL2_ttf-devel-*-mingw.tar.gz"
    echo "  cd SDL2_ttf-*/x86_64-w64-mingw32"
    echo "  sudo cp -r include/* /usr/x86_64-w64-mingw32/include/"
    echo "  sudo cp -r lib/* /usr/x86_64-w64-mingw32/lib/"
    echo "  sudo cp -r bin/*.dll /usr/x86_64-w64-mingw32/bin/"
    echo
    exit 1
fi
echo -e "${GREEN}OK${NC}"

echo
echo "All dependencies found!"
echo
echo "Building for Windows..."
echo

# Build
make -f makefile.windows

if [ $? -eq 0 ]; then
    echo
    echo -e "${GREEN}=== Build Successful! ===${NC}"
    echo
    echo "Executable created: bin_windows/snake.exe"
    echo
    echo "Next steps:"
    echo "  1. Run ./package_windows.sh to create distribution ZIP"
    echo "  2. The ZIP file will include the .exe and all required DLLs"
    echo
else
    echo
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
