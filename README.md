# Aim Trainer

A lightweight 3D aim trainer written in C++ with [raylib](https://www.raylib.com/). It runs on desktop PCs, old GPUs, and small boards like the Raspberry Pi 5, on Linux (x64 and ARM) and Windows.

Made by [Habibig04](https://github.com/Habibig04).

## Features

- You stand in one spot and aim at targets in front of you, like a real range
- 4 maps: Arena, Firing Range, Apartment (targets sit in the windows of the building across the street) and Dust Alley (crates and a doorway, CS-style)
- 8 game modes: Static, Moving, Reflex, Bots, 30s, 60s, 120s and Endless
- Sphere targets and humanoid bots (headshots are worth 2 points)
- Stats: score, accuracy, streak, targets per minute, average time to hit
- Best score saved for every map and mode
- Graphics presets: High, Medium and Potato, plus automatic Potato if your FPS is low
- Full crosshair editor (style, colour, size, thickness, gap, opacity, outline) with live preview
- Sensitivity, FOV, volume, target colour and FPS limit settings
- Borderless fullscreen that adapts to any screen (F11) and a resizable window
- Animated main menu with a live 3D background
- Built-in sound effects (no asset files needed)
- Works on desktop OpenGL 3.3, desktop OpenGL 2.1 and OpenGL ES 2.0

## Controls

| Key | Action |
|-----|--------|
| Mouse | Aim |
| Left click | Shoot |
| P or Esc | Pause |
| F11 | Toggle fullscreen |

In the pause menu: **P** resume, **R** restart, **F** finish and see results, **Esc** back to the menu.

## Game modes

| Mode | Description |
|------|-------------|
| Static | Stationary targets, no time limit |
| Moving | Strafing targets, no time limit |
| Reflex | Targets shrink and vanish if you are too slow |
| Bots | Strafing humanoid bots, aim for the head |
| 30 / 60 / 120 Seconds | Timed runs with static targets |
| Endless | Moving targets, no limit |

Untimed modes end when you choose **Finish** from the pause menu.

## Requirements

- A C++17 compiler
- CMake 3.14 or newer
- Git (CMake downloads raylib 6.0 automatically on the first build, so you need internet)
- A GPU with OpenGL 2.1 / OpenGL ES 2.0 or newer

## Install

Get the source first:

```bash
git clone https://github.com/Habibig04/Aim-Trainer-in-C.git
cd Aim-Trainer-in-C
```


### How the build works

raylib is compiled for one OpenGL version at build time, so the game can't switch versions while running. `build.sh` builds up to three versions (OpenGL 3.3, OpenGL 2.1 and OpenGL ES 2.0) and `run.sh` starts the best one that works on your machine, falling back to the next one if the window can't be created. You don't need to know which one your GPU needs.

An ARM build will not run on x64 and the other way round, so build on each machine you want to play on.

### Linux (x64)

Install the build dependencies.

Debian / Ubuntu / Kali:
```bash
sudo apt install -y build-essential cmake git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libegl-dev libgles-dev
```

Fedora:
```bash
sudo dnf install -y gcc-c++ cmake git libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel mesa-libEGL-devel mesa-libGLES-devel
```

Arch:
```bash
sudo pacman -S --needed base-devel cmake git libx11 libxrandr libxinerama libxcursor libxi mesa
```

Build and run:
```bash
chmod +x build.sh run.sh
./build.sh
./run.sh
```

### Linux (ARM, Raspberry Pi 5 and similar)

The Pi 5 GPU does not support the OpenGL 3.3 core profile, so the game runs on OpenGL ES 2.0. The launcher handles this automatically.

Install the same dependencies as the x64 section above (Debian-based systems such as Raspberry Pi OS and Kali use the `apt` command), then:

```bash
chmod +x build.sh run.sh
./build.sh
./run.sh
```

The 3.3 build will fail to start on a Pi, and that is expected. `run.sh` then continues to the ES 2.0 build. The first build takes a few minutes because raylib is compiled three times.

### Windows

1. Install [CMake](https://cmake.org/download/) and a compiler: either [Visual Studio](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload, or MinGW-w64.
2. Install [Git](https://git-scm.com/download/win).
3. Open a terminal in the project folder and run:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

4. Run the game:

```powershell
.\build\Release\aim_trainer.exe
```

With MinGW (`-G "MinGW Makefiles"`), the executable is at `build\aim_trainer.exe`. Windows builds use desktop OpenGL 3.3 by default. For a very old GPU, add `-DRAYLIB_GL=2.1` to the first `cmake` command.

### Build a single version manually

If you only want one OpenGL version, you can skip the scripts:

```bash
cmake -S . -B build -DRAYLIB_GL=3.3
cmake --build build -j4
./build/aim_trainer
```

`RAYLIB_GL` accepts `AUTO` (default), `3.3`, `2.1`, `"ES 2.0"` and `"ES 3.0"`. `AUTO` picks `ES 2.0` on ARM and `3.3` everywhere else. To use a raylib you already have installed, add `-DUSE_SYSTEM_RAYLIB=ON`.

## Settings and saves

The game creates two small files in the folder you run it from:

- `settings.ini` for all your settings, including the crosshair
- `scores.ini` for your best score on each map and mode

Delete them to reset everything. You may want to add them to your `.gitignore`.

## Troubleshooting

**`GLX: Failed to create context: GLXBadFBConfig` or `Failed to initialize Window`**
Your GPU or driver can't create the OpenGL version that build was made for. Use `./run.sh`, which tries the other builds automatically. If you built manually, rebuild with `-DRAYLIB_GL="ES 2.0"` (ARM) or `-DRAYLIB_GL=2.1` (old GPUs and virtual machines).

**Running in a virtual machine**
Enable 3D acceleration in the VM settings. Without it, the launcher falls back to software rendering, which is slower.

**Low FPS**
Set Graphics quality to Medium or Potato in Settings, or lower the FPS limit. Potato removes shadows, glow and particles and uses simpler models. The game switches to Potato by itself if it measures under 45 FPS.

**`cmake` can't download raylib**
The first configure needs internet access to fetch raylib from GitHub.

## Credits

- Made by [Habibig04](https://github.com/Habibig04)
- Built with [raylib](https://www.raylib.com/)
