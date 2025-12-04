<div align="center">

```
   _____       __                     
  / ___/__  __/ /_  ____ __________  
  \__ \/ / / / __ \/ __ `/ ___/ __ \ 
 ___/ / /_/ / /_/ / /_/ / /  / /_/ / 
/____/\__,_/_.___/\__,_/_/   \____/  
```

# Saburo - Advanced CS2 External Tool

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/taisrisk/Tainan-Wing-CS2-kernel-Cheat-Source/releases)
[![License](https://img.shields.io/badge/license-Educational-orange.svg)](#)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](#)
[![CS2](https://img.shields.io/badge/game-Counter--Strike%202-yellow.svg)](#)

**A sophisticated kernel-level external tool for Counter-Strike 2 featuring automatic updates, intelligent offset management, and advanced driver handling.**

[Features](#-features) • [Installation](#-installation) • [Usage](#-usage) • [Configuration](#%EF%B8%8F-configuration) • [FAQ](#-faq)

</div>

---

## ✨ Features

### 🎯 Combat Features
- **Aimbot** - Intelligent aim assistance with FOV-based targeting and smooth aiming
  - Right-click activation
  - Automatic target locking with persistent tracking
  - Dynamic head offset adjustment for crouched players
  - Team check support
  
- **Triggerbot** - Automated firing when crosshair is on target
  - Smart enemy detection
  - Configurable team check
  - Optimized performance

- **BunnyHop** - Automatic jumping for enhanced movement
  - Perfect timing system
  - Space key detection
  - Configurable delay

### 👁️ Visual Features
- **ESP Overlay** - Real-time player information display
  - **Snaplines** - Lines connecting to player positions
  - **Distance Display** - Shows distance to each player
  - **Head Angle Lines** - Visual aim guide showing player view direction
  - **Wall Check** - Distinguish between visible and occluded players
  - **Chams** - Player highlighting through walls
  
### 🔧 System Features
- **Auto-Update System** - Automatic version management
  - Checks for updates on startup
  - Downloads and installs updates automatically
  - Version verification and validation

- **Auto-Offset Updater** - Always stay up-to-date with CS2 patches
  - Fetches latest offsets from GitHub (a2x/cs2-dumper)
  - Local caching system for offline use
  - Automatic parsing and integration

- **Intelligent Driver Management**
  - Automatic driver loading via KDU
  - Driver patching for enhanced stealth
  - Connection retry mechanism
  - Clean driver detection and validation

- **Process Monitor** - Real-time CS2 process tracking
  - Detects game closure
  - Offers restart or exit options
  - Automatic re-attachment support

- **Settings Persistence** - Your preferences, remembered
  - Auto-saves all feature states
  - Loads saved settings on startup
  - Per-session configuration

### 🎮 User Experience
- **Smart Lobby Detection** - Waits for you to join a match
  - Validates game state before activation
  - ESP preview in lobby
  - Automatic in-game detection

- **Re-Attach System** - Seamless match transitions
  - Detects round/map changes
  - Automatic player re-initialization
  - Persistent settings across sessions

- **Real-Time Console Display** - Clean, informative interface
  - Color-coded status indicators
  - FPS and player count display
  - Timestamped logging system

---

## 📋 Requirements

- **OS**: Windows 10/11 (64-bit)
- **Game**: Counter-Strike 2 (latest version)
- **Privileges**: Administrator rights required
- **Security**: Windows Defender Real-Time Protection must be disabled
- **.NET**: No additional runtime required (native C++20)

---

## 🚀 Installation

### Step 1: Download
Download the latest release from the [Releases](https://github.com/taisrisk/Tainan-Wing-CS2-kernel-Cheat-Source/releases) page.

### Step 2: Extract
Extract all files to a dedicated folder (e.g., `C:\Saburo\`)

### Step 3: Disable Windows Defender
⚠️ **Important**: Real-Time Protection interferes with kernel driver loading.

1. Open Windows Security
2. Go to **Virus & threat protection**
3. Click **Manage settings**
4. Turn OFF **Real-time protection**

### Step 4: Run as Administrator
Right-click `Saburo.exe` → **Run as administrator**

### Step 5: Follow On-Screen Instructions
The tool will automatically:
- Check for updates
- Download required drivers
- Patch driver signatures
- Load the kernel driver
- Wait for CS2 to launch

---

## 🎮 Usage

### Controls

| Key | Function |
|-----|----------|
| `0` | Toggle Debug Console |
| `1` | Toggle Aimbot |
| `2` | Toggle Triggerbot |
| `3` | Toggle BunnyHop |
| `4` | Toggle Head Angle Lines |
| `5` | Toggle Snaplines |
| `6` | Toggle Distance ESP |
| `7` | Toggle Snapline Wall-Check |
| `8` | Toggle Team Check |
| `9` | Toggle Chams |

### Aimbot Activation
- Hold **RIGHT MOUSE BUTTON** while aimbot is enabled

### Basic Workflow
1. **Launch Saburo** (as administrator)
2. **Start Counter-Strike 2**
3. **Join a match** (Casual, Competitive, Deathmatch, etc.)
4. **Configure features** using number keys
5. Settings are automatically saved!

---

## ⚙️ Configuration

### Settings File
Settings are stored in `saburo_settings.cfg` and persist across sessions.

### Default Settings
```
Triggerbot: ON
Aimbot: OFF
BunnyHop: OFF
Team Check: ON
All ESP Features: OFF
```

### Logs
All activities are logged to `Logs.txt` for troubleshooting.

---

## 🔄 How It Works

### Auto-Update System
1. Contacts version server on startup
2. Compares local version with remote version
3. Downloads driver package if update needed
4. Verifies installation integrity
5. Updates local version file

### Auto-Offset System
1. Attempts to load cached offsets from local files
2. If cache invalid or missing, downloads from [a2x/cs2-dumper](https://github.com/a2x/cs2-dumper)
3. Parses JSON offset data
4. Caches offsets locally for faster startup
5. Automatically updates when CS2 patches

### Driver Patching
1. Downloads vulnerable signed driver
2. Patches device names and symbolic links
3. Creates backup of original driver
4. Loads patched driver via KDU
5. Establishes kernel-level memory access

### Process Monitor
- Checks every 1 second if CS2 is still running
- Detects game closure immediately
- Prompts for exit or restart
- Maintains settings persistence

---

## 🛠️ Troubleshooting

### "Driver not detected"
- Ensure Windows Defender is **fully disabled**
- Run as **Administrator**
- Check if KDU files are present

### "Failed to download offsets"
- Check your internet connection
- Ensure GitHub is accessible
- Offsets cache may be used as fallback

### "CS2 not detected"
- Launch CS2 after Saburo is running
- Wait for CS2 to fully load
- Check Task Manager for `cs2.exe` process

### "Aimbot not working"
- Hold **RIGHT MOUSE BUTTON**
- Ensure aimbot is enabled (key `1`)
- Check if team check is configured correctly (key `8`)

### ESP Not Displaying
- Ensure you've joined an active match
- Check if overlay is initialized (console logs)
- Verify CS2 window is focused

---

## ❓ FAQ

**Q: Is this detected by VAC?**  
A: This tool operates at kernel-level externally. Use at your own risk. The developers are not responsible for any consequences.

**Q: Do I need to download offsets manually?**  
A: No! The tool automatically downloads and updates offsets from GitHub.

**Q: Can I use this on FACEIT/ESEA?**  
A: **Not recommended.** Third-party anti-cheats have more aggressive detection.

**Q: Does this work after CS2 updates?**  
A: Yes! The auto-offset system fetches new offsets automatically after game patches.

**Q: Why does Windows Defender flag this?**  
A: Kernel drivers and memory manipulation tools are flagged by antivirus software due to their low-level nature.

**Q: Can I run this alongside other programs?**  
A: Yes, but avoid running multiple kernel drivers simultaneously.

**Q: Will my settings be saved?**  
A: Yes! All feature states are automatically saved to `saburo_settings.cfg` when toggled.

---

## 📝 Credits

- **Developer**: zrorisc
- **Offsets**: [a2x/cs2-dumper](https://github.com/a2x/cs2-dumper)
- **Additional Offsets**: [sezzyaep/CS2-OFFSETS](https://github.com/sezzyaep/CS2-OFFSETS)
- **Driver Loading**: KDU (Kernel Driver Utility)

---

## ⚖️ Disclaimer

This project is for **educational purposes only**. The developers do not condone cheating in online games and are not responsible for any bans, suspensions, or other consequences resulting from the use of this software.

**Use at your own risk.**

---

## 📄 License

This project is released for educational purposes. See the repository for more information.

---

<div align="center">

**Made with ❤️ by zrorisc**

⭐ **Star this repo if you find it useful!**

</div>
