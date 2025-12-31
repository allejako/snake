@echo off
REM Windows packaging script - creates distributable ZIP

echo === Snake Game - Windows Packager ===
echo.

REM Check if build exists
if not exist "bin_windows\snake.exe" (
    echo ERROR: snake.exe not found!
    echo Please run: mingw32-make -f Makefile.win
    pause
    exit /b 1
)

REM Create package directory
set PACKAGE_DIR=snake_windows
if exist "%PACKAGE_DIR%" rmdir /S /Q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"

echo Copying files...

REM Copy executable
copy bin_windows\snake.exe "%PACKAGE_DIR%\" > nul

REM Copy assets
if exist "assets" (
    xcopy /E /I assets "%PACKAGE_DIR%\assets" > nul
    echo   - assets/
)

REM Copy DLLs from MinGW bin (adjust path if needed)
set MINGW_BIN=C:\mingw64\bin
if exist "%MINGW_BIN%" (
    echo Copying DLLs...
    copy "%MINGW_BIN%\SDL2.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\SDL2_ttf.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libfreetype-6.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\zlib1.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libpng*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libbz2*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libharfbuzz*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libglib*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libgraphite2*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libintl*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libiconv*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libpcre*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libstdc++*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libgcc*.dll" "%PACKAGE_DIR%\" 2>nul
    copy "%MINGW_BIN%\libwinpthread*.dll" "%PACKAGE_DIR%\" 2>nul
)

REM Create README
echo Creating README.txt...
(
echo Snake Game - Windows Edition
echo ============================
echo.
echo HOW TO RUN
echo ----------
echo Simply double-click snake.exe to start the game!
echo.
echo CONTROLS
echo --------
echo Menu Navigation:
echo   Arrow keys: Navigate menus
echo   Enter: Select
echo   Escape: Go back
echo.
echo Singleplayer:
echo   Arrow keys or WASD: Move snake
echo   Escape: Pause game
echo.
echo FEATURES
echo --------
echo - Singleplayer with combo system and visual effects
echo - Local multiplayer ^(same keyboard^)
echo - Customizable keybindings
echo - Sound effects and music
echo - High score tracking
echo.
echo NOTES
echo -----
echo - Online multiplayer is not available in Windows version
echo - Settings saved in settings.dat
echo - High scores saved in scoreboard.dat
echo.
echo Enjoy!
) > "%PACKAGE_DIR%\README.txt"

REM Create ZIP using PowerShell
echo Creating ZIP file...
powershell -Command "Compress-Archive -Path '%PACKAGE_DIR%\*' -DestinationPath 'snake_windows.zip' -Force"

if exist "snake_windows.zip" (
    echo.
    echo === Package Complete! ===
    echo.
    echo Created: snake_windows.zip
    for %%A in (snake_windows.zip) do echo Size: %%~zA bytes
    echo.
    echo Contents:
    dir /B "%PACKAGE_DIR%"
    echo.
    echo Ready for distribution!
    echo.
) else (
    echo ERROR: Failed to create ZIP file
    pause
    exit /b 1
)

pause
