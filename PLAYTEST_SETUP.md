# Snake Game - Playtest Setup Instructions

Hey! Thanks for helping playtest the multiplayer snake game. Here's how to get it running on your Windows machine.

## Step 1: Install WSL2 (One-time setup, ~5 minutes)

1. **Open PowerShell as Administrator**
   - Press `Windows + X`
   - Select "Windows PowerShell (Admin)" or "Terminal (Admin)"

2. **Run this command:**
   ```powershell
   wsl --install
   ```

3. **Restart your computer** when prompted

4. **After restart**, Ubuntu will open automatically
   - Create a username (can be anything, like your name)
   - Create a password (you'll need this later)

## Step 2: Get the Game Files

1. **Extract the `snake` folder** I sent you to somewhere easy to find, like:
   - `C:\Users\YourName\snake`

2. **Open Ubuntu** (search for "Ubuntu" in the Start menu)

3. **Navigate to the game folder:**
   ```bash
   cd /mnt/c/Users/YourName/snake
   ```
   *(Replace "YourName" with your actual Windows username)*

## Step 3: Install Game Dependencies (One-time setup)

Run the setup script:
```bash
./setup.sh
```

Enter your Ubuntu password when asked. This will install the required libraries.

## Step 4: Build and Play!

**Build the game:**
```bash
make
```

**Run the game (without audio - recommended for WSL2):**
```bash
./bin/snake_sdl.exe --no-audio
```

**OR use the shortcut:**
```bash
make run ARGS="--no-audio"
```

## Playing Multiplayer

When you're testing multiplayer, you'll need:
- The **multiplayer server** running somewhere (ask me for the IP/port)
- Both of us to be able to connect to it

### Controls
- **Arrow keys** - Move snake
- **ESC** - Pause / Back to menu
- Check the Options menu in-game for key bindings

## Troubleshooting

**"Command not found" errors:**
- Make sure you ran `./setup.sh` first

**Display issues:**
- Make sure you have an X server running (WSL2 usually handles this automatically in Windows 11)
- Or try running with `--no-audio` flag

**Can't find the game folder:**
- Windows drives are mounted at `/mnt/` in WSL2
- `C:\` becomes `/mnt/c/`
- `D:\` becomes `/mnt/d/`

## Need Help?

Contact me if you run into any issues!
