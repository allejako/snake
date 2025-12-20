#!/bin/bash
# Snake Game - WSL2 Setup Script
# This script installs all dependencies needed to run the game

echo "========================================="
echo "  Snake Game - Dependency Installer"
echo "========================================="
echo ""
echo "This will install the required libraries to run the game."
echo "You may need to enter your password for sudo access."
echo ""

# Update package list
echo "Updating package list..."
sudo apt-get update

# Install dependencies
echo ""
echo "Installing game dependencies..."
sudo apt-get install -y libsdl2-dev libsdl2-ttf-dev libjansson-dev make gcc

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================="
    echo "  Installation Complete!"
    echo "========================================="
    echo ""
    echo "To play the game:"
    echo "  make run"
    echo ""
    echo "Or build and run separately:"
    echo "  make          # Build the game"
    echo "  make run      # Run the game"
    echo ""
    echo "For WSL2, we recommend running without audio:"
    echo "  ./bin/snake_sdl.exe --no-audio"
    echo ""
else
    echo ""
    echo "========================================="
    echo "  Installation Failed"
    echo "========================================="
    echo "Please check the error messages above."
    exit 1
fi
