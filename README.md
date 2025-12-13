# Apex Legends ESP & TriggerBot

An advanced external overlay application for Apex Legends featuring ESP (Extra Sensory Perception) visualization and intelligent triggerbot functionality with kernel-level driver integration.

## 🚀 Quick Start

```powershell
# Step 1: Load the driver (run as Administrator, requires restart after each reboot)
& "c:\Users\haeri\Downloads\Compressed\skibidirizz\kdmapper\x64\Release\kdmapper_Release.exe" "C:\Users\haeri\Downloads\Compressed\skibidirizz\ext\uictl\kernel mode\build\driver\driver.sys"

# Step 2: Run the ESP overlay
& "c:\Users\haeri\Downloads\Compressed\skibidirizz\ext\bin\Release\ext.exe"
```

```
# Build ext
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "c:\Users\haeri\Downloads\Compressed\skibidirizz\ext\apex_esp.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0.26100.0 /t:Rebuild /m
```

## 📊 Current Project State

| Component | Status | Notes |
|-----------|--------|-------|
| **ext.exe** | ✅ Working | Console mode with debug output |
| **driver.sys** | ✅ Working | kdmapper compatible, shared memory via MmMapViewInSystemSpace |
| **kdmapper** | ✅ Working | Loads unsigned driver without disabling Secure Boot |
| **Driver Communication** | ✅ Working | IOCTL + shared memory section |
| **Memory Reading** | ✅ Working | Physical memory translation via CR3 |
| **ESP Rendering** | ⚠️ Needs Offsets | Requires updated game offsets |
| **TriggerBot** | ⚠️ Needs Offsets | Requires updated game offsets |

### 🔧 What You Need To Do

**The only thing missing is updated game offsets!** Edit `src/memory/offsets.h` with current Season 24 offsets:

```cpp
#define OFF_LOCAL_PLAYER 0x???????    // Find current offset
#define OFF_ENTITY_LIST 0x???????     // Find current offset
// etc...
```

After updating offsets, rebuild ext.exe:
```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Users\haeri\Downloads\Compressed\skibidirizz\ext\apex_esp.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Build
```

## ⚠️ Disclaimer

**EDUCATIONAL PURPOSES ONLY**

This project is intended for educational purposes and security research only. Using this software in online games violates the game's Terms of Service and End User License Agreement (EULA). The authors do not condone cheating in any form and are not responsible for any consequences resulting from the use of this software.

**Use at your own risk. Account bans are likely if used in live gameplay.**

## 🎯 Features

### Visual ESP
- **Player Box ESP**: 2D/3D bounding boxes around players
- **Health & Shield Bars**: Real-time health/shield visualization with armor type color coding
- **Distance Indicators**: Shows distance to each player
- **Team-based coloring**: Distinguishes teammates from enemies
- **Skeleton ESP**: Bone structure visualization
- **Snaplines**: Lines from screen center to players
- **Customizable colors and styles**

### TriggerBot
- **Advanced bone detection system** with multiple targeting modes:
  - Body only
  - Head only
  - Head + Body (priority head)
  - Full body (all bones)
- **5-level delay system** for human-like behavior (10ms - 300ms)
- **Velocity prediction** for moving targets
- **Hitbox visualization** for debugging
- **Kernel-level mouse injection** for undetectable input
- **Configurable hotkeys**

### Security Features
- **Kernel-mode driver interface** for memory reading
- **DMA-style memory access** (no traditional ReadProcessMemory calls)
- **Process name randomization** (mimics common Windows processes)
- **DirectX 11 overlay** with transparency
- **Anti-detection measures**

## 🏗️ Architecture

### Project Structure

```
ext/
├── src/
│   ├── main.cpp                    # Main entry point and core loops
│   ├── driver/
│   │   ├── driver_interface.cpp    # Kernel driver communication
│   │   ├── driver_interface.h
│   │   └── common.h                # Shared memory structures
│   ├── game/
│   │   ├── entity.h                # Player entity representation
│   │   ├── esp_feature.h           # ESP rendering logic
│   │   ├── triggerbot.h/cpp        # Triggerbot implementation
│   │   ├── feature_manager.h       # Feature management system
│   │   ├── distance_manager.h      # Distance calculations
│   │   ├── math.h                  # Vector/matrix math utilities
│   │   ├── esp_config.h            # ESP configuration
│   │   └── name_toggle.h           # Name visibility toggle
│   ├── memory/
│   │   ├── memory_reader.h         # Memory reading utilities
│   │   └── offsets.h               # Game memory offsets
│   ├── overlay/
│   │   ├── overlay.cpp/h           # DirectX overlay window
│   │   └── esp_renderer.h          # ESP drawing functions
│   ├── security/
│   │   └── process_protection.h    # Anti-debugging features
│   └── OS-ImGui/                   # ImGui integration for overlay
├── bin/Release/                    # Compiled output
└── obj/Release/                    # Build artifacts
```

### Key Components

#### 1. Driver Interface (`driver/driver_interface.h`)
Communicates with a kernel-mode driver via shared memory to read game memory without traditional Windows API calls.

**Key functions:**
- `GetProcessId()` - Find target process
- `GetModuleBase()` - Get base address of game modules
- `ReadMemory()` - Read memory through kernel driver
- `InjectMouseClick()` - Kernel-level mouse input injection

#### 2. ESP Feature System (`game/esp_feature.h`)
Modular feature system implementing the `IFeature` interface:
- Health/shield bars with armor type colors
- Player information rendering
- Box ESP with customizable styles
- Distance calculations

#### 3. TriggerBot (`game/triggerbot.h`)
Intelligent triggerbot with:
- **Delay Levels**:
  - Level 1: 200-300ms (Very Safe)
  - Level 2: 100-200ms (Safe)
  - Level 3: 50-100ms (Balanced)
  - Level 4: 25-50ms (Aggressive)
  - Level 5: 10-25ms (Very Aggressive)
- **Bone Targeting Modes**:
  - Body only
  - Head only
  - Head + Body
  - Full body
- Velocity prediction for moving targets
- Hitbox debugging visualization

#### 4. Memory Reading (`memory/memory_reader.h`)
Template-based memory reading utilities:
```cpp
template<typename T>
T Read(uintptr_t address);

template<typename T>
void ReadArray(uintptr_t address, T* buffer, size_t count);
```

#### 5. Overlay System (`overlay/overlay.h`)
DirectX 11-based transparent overlay:
- Borderless window positioned over game
- ImGui integration for menu
- Real-time ESP rendering
- 30 FPS default (configurable)

## 🛠️ Requirements

### Development
- **Visual Studio 2025 Preview** (v18) with C++ desktop development workload
- **Windows SDK 10.0.26100.0**
- **Windows Driver Kit (WDK) 10.0.26100.0** (for driver compilation only)
- **Platform Toolset v143** (MSVC 14.44)

### Runtime
- **Windows 10/11** (x64 only)
- **kdmapper** (included - loads driver without disabling Secure Boot)
- **Apex Legends** (DirectX 12 version: `r5apex_dx12.exe`)
- **Administrator privileges** (for driver communication)

## 📦 Compilation

### Build ext.exe (ESP Overlay)

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Users\haeri\Downloads\Compressed\skibidirizz\ext\apex_esp.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Build
```

Output: `bin\Release\ext.exe`

### Build driver.sys (Kernel Driver)

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Users\haeri\Downloads\Compressed\skibidirizz\ext\uictl\kernel mode\driver.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:WDKBuildFolder=10.0.26100.0 /p:VisualStudioVersion=17.0 /p:ApiValidator_Enable=false /t:Build
```

Output: `uictl\kernel mode\build\driver\driver.sys`

### Build kdmapper (Driver Loader)

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Users\haeri\Downloads\Compressed\skibidirizz\kdmapper\kdmapper.sln" /p:Configuration=Release /p:Platform=x64 /t:Build
```

Output: `kdmapper\x64\Release\kdmapper_Release.exe`

## 🚀 Usage

### Prerequisites
1. **Kernel driver must be loaded** before running the application
2. Run as **Administrator**
3. Ensure Apex Legends is installed (DirectX 12 version)

### Basic Usage
1. Run `ext.exe` as Administrator
2. The application will wait for Apex Legends to start (`r5apex_dx12.exe`)
3. Once detected, the overlay window will appear
4. Press **L** to toggle the menu

### Menu Navigation
- **L**: Toggle menu visibility
- **Mouse**: Navigate menu options
- Use checkboxes and sliders to configure features

### ⚡ Recompilation After Changes
**Yes, you need to recompile ext.exe every time you change the code (including offsets.h).**

Quick rebuild command:
```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Users\haeri\Downloads\Compressed\skibidirizz\ext\apex_esp.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Build
```

**Note:** You do NOT need to reload the driver after updating ext.exe - just restart ext.exe.

### ESP Configuration
- Enable/disable ESP
- Customize box style (2D/3D)
- Toggle health/shield bars
- Adjust colors for different teams
- Enable/disable distance indicators
- Configure snaplines and skeleton

### TriggerBot Configuration
- **Enable/Disable**: Toggle triggerbot on/off
- **Delay Level**: Choose reaction time (1-5)
  - Level 1: Safest, most human-like
  - Level 5: Fastest, more detectable
- **Bone Mode**: Select targeting preference
  - Body only: Chest shots
  - Head only: Headshots
  - Head + Body: Priority head
  - Full body: All hitboxes
- **Hotkey**: Set activation key (default: customizable)
- **Prediction**: Enable velocity-based prediction
- **Show Hitboxes**: Debug visualization

### Performance Settings
- **Target FPS**: Default 30 FPS (reduces CPU usage)
- Adjust in menu for higher refresh rate (30-144 FPS)

## 🔧 Configuration

### Game Offsets (`memory/offsets.h`)
Game offsets may need updating after Apex Legends patches:

```cpp
#define OFF_LOCAL_PLAYER 0x2a7be28    // Local player pointer
#define OFF_ENTITY_LIST 0x65fe6d8     // Entity list base
#define OFF_HEALTH 0x0324             // Current health
#define OFF_SHIELD 0x01a0             // Current shield
// ... more offsets
```

### Finding Updated Offsets
1. Use pattern scanning tools (e.g., ReClass.NET, Cheat Engine)
2. Compare with community offset updates
3. Test offsets by checking if ESP displays correctly

### Customization
Edit `main.cpp` to customize:
- Target FPS (`g_TargetFPS`)
- Entity scan count (default: 64 players)
- Default colors and styles
- Hotkey assignments

## 🧪 Technical Details

### Memory Reading Architecture
- **Kernel driver communication** via shared memory section
- **DMA-style access** bypasses user-mode hooks
- **CR3 register** used for process context switching

### ESP Rendering Pipeline
1. **Memory Loop Thread** (reads game memory every frame)
   - Scans entity list
   - Reads player positions, health, shields
   - Updates view matrix for world-to-screen calculations
2. **Overlay Thread** (renders ESP)
   - Transforms 3D coordinates to 2D screen space
   - Draws boxes, health bars, text
   - Updates at target FPS

### TriggerBot Logic
1. Check if hotkey is pressed
2. Get local player view angles
3. For each enemy entity:
   - Get bone positions
   - Apply velocity prediction if enabled
   - Check if bone is on crosshair
4. If target detected, inject mouse click via kernel driver
5. Apply randomized human-like delay

### World-to-Screen Conversion
```cpp
Vector2 worldToScreen(Vector3 world, Matrix viewMatrix) {
    Vector4 clip;
    clip.x = world.x * viewMatrix[0][0] + world.y * viewMatrix[0][1] + 
             world.z * viewMatrix[0][2] + viewMatrix[0][3];
    clip.y = world.x * viewMatrix[1][0] + world.y * viewMatrix[1][1] + 
             world.z * viewMatrix[1][2] + viewMatrix[1][3];
    clip.w = world.x * viewMatrix[3][0] + world.y * viewMatrix[3][1] + 
             world.z * viewMatrix[3][2] + viewMatrix[3][3];
    
    if (clip.w < 0.1f) return Vector2{-1, -1}; // Behind camera
    
    Vector2 ndc;
    ndc.x = clip.x / clip.w;
    ndc.y = clip.y / clip.w;
    
    Vector2 screen;
    screen.x = (screenWidth / 2.0f) * ndc.x + (ndc.x + screenWidth / 2.0f);
    screen.y = -(screenHeight / 2.0f) * ndc.y + (ndc.y + screenHeight / 2.0f);
    
    return screen;
}
```

## 📊 Performance Optimization

- **FPS Limiter**: Reduces CPU usage (default 30 FPS)
- **Efficient entity scanning**: Only processes active players
- **Selective rendering**: Skips off-screen entities
- **Minimal memory allocations**: Reuses buffers where possible

## 🔒 Security Considerations

**This project implements several anti-detection techniques:**
- Kernel driver for memory access (no user-mode hooks)
- Random process name selection
- No WriteProcessMemory calls (read-only ESP mode)
- Kernel-level mouse injection

**However:**
- Anti-cheat systems are sophisticated and continuously updated
- No guarantee against detection
- Use is strongly discouraged in live gameplay

## 🐛 Known Issues

1. **Offset updates required** after game patches - ESP won't show without correct offsets
2. **Driver must be loaded** before application start (reboot clears driver)
3. **Windows Defender** may block kdmapper - add exclusion folder if needed

## 📝 Build Output

After successful compilation:
```
bin/Release/
├── ext.exe        # Main executable (~405 KB)
└── ext.pdb        # Debug symbols (~3.5 MB, for crash analysis)
```

## 🤝 Contributing

This is an educational project. Contributions are welcome for:
- Code improvements
- Documentation enhancements
- Security research findings

**Do not contribute:**
- Bypass techniques for active anti-cheat systems
- Offensive features beyond educational scope

## 📜 License

This project is provided as-is for educational purposes. No warranty or support is provided.

## 🔗 Dependencies

- **ImGui**: Immediate mode GUI library (included)
- **DirectX 11**: Graphics API for overlay
- **Windows SDK**: Windows API development
- **Custom kernel driver**: Memory access (not included)

## 📞 Support

**No support is provided for:**
- Bypassing anti-cheat systems
- Using this in online games
- Driver development or loading

**Educational questions welcome regarding:**
- Game reverse engineering techniques
- Memory scanning concepts
- DirectX overlay development
- Driver communication architecture

---

**Remember**: This software is for learning purposes only. Respect game developers and other players by not using cheats in online games.
