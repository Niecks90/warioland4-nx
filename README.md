# Wario Land 4 Recomp (Nintendo Switch)

> **Nintendo Switch Homebrew Port (`warioland4-nx`)**  
> Static recompilation of **Wario Land 4** (USA/Europe, `AWAE`) running natively on Nintendo Switch at **60 FPS** in **1280×720** (720p 16:9 widescreen).

The game ROM and Nintendo GBA BIOS are **not included**. You must supply your own legally obtained dumps.

---

## Changelog — v0.1.1

- **Widescreen Gameplay & Smart Auto-Pillarboxing (`16:9 Auto` / `16:9 Force` / `Classic GBA (3:2)`)**:
  - Fixed GBA hardware window bounds (`WIN0H` / `WIN1H`) in `switch/gba_ppu.cpp` so full-width gameplay windows (`x1 == 0` / `x2 >= 240`) extend into the 16:9 margins during gameplay (`GameScreenSubroutine`, `gMainGameMode == 2 || 8`) instead of falling through to `WINOUT` black bars, while keeping inner iris/spotlight effects authentic.
  - Fixed `GbaPpu::deserialize` so loading a save state no longer sticks `g_ws_pillarbox = 1`.
  - Added `wario_extended_view_frame` smart pillarboxing: automatically pillarboxes bitmap modes, no-BG BIOS frames, and static 256px intro/title screens while keeping playable stages and scrolling cutscenes in full `284×160` widescreen.
- **In-Game QoL Quick Menu (Hold `-` ~450 ms)**:
  - Lightweight SDL2 overlay menu (embedded 5×7 font, zero external font assets) that pauses GBA execution and mutes audio cleanly while open.
  - **5 Save State Slots** stored in `sdmc:/switch/warioland4-nx/savestates/slot_N.state` with timestamps and toast feedback.
  - **Live Aspect Ratio / Widescreen** (`16:9 Auto`, `16:9 Force`, `Classic GBA (3:2)`), **Video Filters** (`Nearest`, `Bilinear`, `Sharp 4x`, `GBA LCD Grid`), **GBA Color Correction** (`Raw`, `Backlit`, `Frontlit`, `Unlit`, `Classic`), **FPS + Frame-time HUD**, **VSync**, **Audio**, **CPU Boost**, **Reset Game**, and persistent `config.txt` saving.
  - Preserved short tap on `-` for GBA `SELECT` and `+` + `-` held together to exit cleanly.
- **Custom Application Icon**: Added a custom 256×256 RGB Homebrew Menu icon (`switch/icon.jpg`) and updated NACP metadata (`Wario Land 4` / `0.1.1`).

## Changelog — v0.1.0

- **Initial Nintendo Switch Port (`warioland4-nx`)**: Ported [Zaxaerith/WarioLand4Recomp](https://github.com/Zaxaerith/WarioLand4Recomp) onto our Switch-optimized `gbarecomp` runtime base (`build/warioland4-nx.nro`).
- **Native 720p Fullscreen & 16:9 Widescreen (`284×160` → `1280×720`)**: Enabled 16:9 GBA view expansion (`284×160`, `extra_left = 22`, `extra_right = 22`) filling the Switch's `1280×720` viewport cleanly with hardware-accelerated scaling, plus clean black pillarboxing when `widescreen = 0` (`240×160`).
- **Dynamic Stack-Stub Execution Hook**: Installed `wario_force_interp_hook` for Wario Land 4's runtime-generated IWRAM stack stubs (`0x03007D00..0x03007E00`), removing any dependency on runtime compiler self-healing.
- **Switch Multi-Core & Performance Optimizations**:
  - Multi-threaded scanline PPU (`switch/gba_ppu.cpp`) with band parallelization across cores, wide sprite X-wrapping, window mask fast-path, and affine fast-paths.
  - Emulation thread pinned to Core 1 with a 32 MiB stack.
  - ARM64 NEON RGB24→RGBA32 texture upload (`switch/host_window.cpp`) and Horizon-native `FramePacer` (`svcSleepThread` spin-tail) with `vsync = 0` default.
- **SRAM Persistence & Symbol Support**: Configured 32 KiB SRAM (`SRAM_V112` → `wario_land_4_awae.sav`), 64-shard AOT generation (`game.toml`), and corrected `0x08xxxxxx` function/data symbols (`symbols/`).

---

## SD Card Layout

Create `sdmc:/switch/warioland4_nx/` on your SD card and place the files as follows:

```text
/switch/warioland4_nx/
 ├── warioland4-nx.nro   (Homebrew executable)
 ├── rom.gba             (Wario Land 4 USA/Europe ROM — or "Wario Land 4 (USA, Europe).gba")
 ├── gba_bios.bin        (Retail GBA BIOS — mandatory)
 ├── config.txt          (Auto-created on first launch, updated by the QoL menu)
 └── save/               (Auto-created directory containing wario_land_4_awae.sav)

/switch/warioland4-nx/savestates/
 └── slot_1.state .. slot_5.state   (In-game save states created from the QoL menu)
```

### Required ROM & BIOS Identity

| File | Details | SHA-1 |
|---|---|---|
| **ROM** (`rom.gba` / `Wario Land 4 (USA, Europe).gba`) | USA / Europe (`AWAE`), 8 MiB (`8,388,608` bytes) | `b9fe05a8080e124b67bce6a623234ee3b518a2c1` |
| **GBA BIOS** (`gba_bios.bin`) | Retail GBA BIOS, 16 KiB (`16,384` bytes) | `300c20df6731a33952ded8c436f7f186d25d3492` |

> [!IMPORTANT]
> Always launch homebrew via **Title Override** (hold `R` while launching a retail title from the Switch Home Menu) rather than Applet Mode (Album) for full memory and CPU performance.

---

## Controls

| Switch Control | GBA Action |
|---|---|
| **D-Pad / Left Stick** | Move / Navigate |
| **A** | A (Jump / Confirm) |
| **B** | B (Dash / Attack / Cancel) |
| **L / ZL** | L |
| **R / ZR** | R |
| **+ (Plus)** | Start (Pause / Confirm) |
| **- (Minus) (Tap)** | Select |
| **- (Minus) (Hold ~450 ms)** | Open / Close In-Game QoL Menu (Save States, Aspect, Filters, Colors) |
| **+ and - Held Together** | Exit cleanly to Homebrew Menu |

---

## Configuration (`config.txt`)

On first launch, `sdmc:/switch/warioland4_nx/config.txt` is automatically created (and saved automatically when changed in the QoL menu) with:

```ini
widescreen = 1
video_filter = nearest
color_mode = raw
linear_filter = 0
vsync = 0
uncapped = 0
cpu_boost = 0
show_fps = 0
audio = 1
bios_hle = 1
```

### Options

- **`widescreen`** (`0`, `1`, or `2` / `classic`, `auto`, `force`):
  - `1` / `auto` *(Default)*: `16:9 Auto` — fills `1280×720` (`284×160`) in gameplay and scrolling cutscenes while cleanly letterboxing static 256px BIOS/title screens.
  - `2` / `force`: `16:9 Force` — forces full `284×160` widescreen everywhere.
  - `0` / `classic`: `Classic GBA (3:2)` — authentic centered `240×160` (`1080×720` on 720p) with black pillarboxing.
- **`video_filter`**: `nearest` *(Default)*, `bilinear`, `sharp4x` (`4×` integer prescale + linear crispness), or `lcd` (`Sharp 4×` + subtle GBA LCD pixel grid).
- **`color_mode`**: `raw` *(Default)*, `backlit` (AGS-101/DS), `frontlit` (AGB/SP Frontlight), `unlit` (original reflective AGB-001), or `classic` (desaturated handheld curve).
- **`vsync`** (`0` or `1`):
  - `0` *(Default)*: Uses the Horizon-native `FramePacer` at `59.7275 Hz` without blocking Core 1 on HDMI scanout.
  - `1`: Enables `SDL_RENDERER_PRESENTVSYNC`.
- **`uncapped`** (`0` or `1`): Disables frame pacing when set to `1`.
- **`cpu_boost`** (`0`, `1`, or `2`):
  - `0` *(Default)*: Normal mode (`1020 MHz` CPU, un-throttled GPU/EMC).
  - `1`: FastLoad (`1785 MHz` CPU).
  - `2`: Type2 (`1020 MHz` CPU, minimum GPU).
- **`show_fps`** (`0` or `1`): Displays an on-screen FPS + frame-time (`ms`) HUD overlay.
- **`audio`** (`1` or `0`): Enables or disables audio output.
- **`bios_hle`** (`1` or `0`): `1` *(Default)* enables fast BIOS SWI HLE with recompiled LLE fallback; `0` runs pure recompiled BIOS LLE.

---

## Building from Source

Building requires [devkitPro](https://devkitpro.org/) (`devkitA64`, `libnx`, Switch `SDL2` portlibs), CMake 3.20+, and your own verified `Wario Land 4 (USA, Europe).gba` ROM in the repository root.

```bash
# 1. Build host gba_recompile and generate the 64 C++ shards into generated/
./generator.sh

# 2. Cross-compile and link build/warioland4-nx.nro
./build_switch.sh
```

The final homebrew executable is generated at `build/warioland4-nx.nro`.

---

## Credits

- **Niecks90**: Nintendo Switch port, 720p/16:9 presentation, Switch PPU & Horizon runtime integration.
- **Zaxaerith**: [WarioLand4Recomp](https://github.com/Zaxaerith/WarioLand4Recomp) configuration, discovery metadata, and `wario_force_interp_hook`.
- **mstan**: [gbarecomp](https://github.com/mstan/gbarecomp) static recompilation framework.
- **lilDavid**: [warioland4](https://github.com/lilDavid/warioland4) decompilation and symbol names.
- **devkitPro**: `devkitA64` / `libnx` Nintendo Switch toolchain.

---

## Legal

This is an unofficial, non-commercial preservation and educational project. It is not affiliated with, authorized, or endorsed by Nintendo. All trademarks, game titles, and assets belong to their respective owners. No copyrighted ROMs, BIOS dumps, or ROM-derived generated code are distributed in this repository.
