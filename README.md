# Aeria Sorround Upmix for OBS Studio

[![Version](https://img.shields.io/badge/version-0.1.3-blue.svg)](https://github.com/aeriacreative/obs-aeria-surroundupmix)
[![OBS Studio](https://img.shields.io/badge/OBS%20Studio-30%20|%2031%20|%2032+-green.svg)](https://obsproject.com)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg)]()
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg)](LICENSE)
[![Creator](https://img.shields.io/badge/Created%20by-Aeria%20Creative-00d2ff.svg)](https://linktr.ee/aeriacreative)

**Aeria Sorround Upmix** is a professional real-time surround sound upmixing and multi-channel audio management plugin for **OBS Studio**. It transforms standard mono and stereo audio sources into rich 5.1 and 7.1 surround soundscapes with intuitive VST-inspired visual controls.

Developed by [**Aeria Creative**](https://linktr.ee/aeriacreative).

---

## ✨ Key Features

### 🎧 High-Fidelity Audio Upmixing Engine
- **Mid/Side Acoustic Decomposition:** Extracts clean center dialog and dynamic stereo width.
- **Center Channel Extraction:** Adjustable center spread with front speaker subtraction to avoid phase cancellation.
- **Dedicated LFE Subwoofer Channel:** Generated using a 2nd-order resonant low-pass biquad filter with configurable cutoff frequency (default: 120 Hz).
- **Decorrelated Ambience:** Applies psychoacoustic delay lines (15ms) to rear and side channels for a natural sense of space without comb filtering.
- **Native C++ Performance:** DSP runs entirely on native audio threads with zero UI latency and minimal CPU overhead.

### 🎛️ Versatile Processing Modes
1. **Auto (Detect OBS Output):** Automatically matches your OBS Master Audio channel layout.
2. **Force 5.1 Surround:** Outputs 6 discrete channels (`L`, `R`, `C`, `LFE`, `RL`, `RR`).
3. **Force 7.1 Surround:** Outputs 8 discrete channels (`L`, `R`, `C`, `LFE`, `RL`, `RR`, `SL`, `SR`).
4. **Direct Pass-Through:** Preserves original channel routing while providing full per-channel gain faders and panning.
5. **Mono Surround (Speaker Balance Test):** Sums active input signals and broadcasts identical levels across all surround channels — ideal for speaker placement, room acoustics, and volume calibration.

### 🖥️ Interactive Native Qt6 Docks

#### 1. Sorround Control Dock
- **Interactive 2D Circular Panner:** Drag the puck to position the sound stage front/rear and left/right. Double-click inside to instantly reset to center `(0.00, 0.00)`.
- **Per-Channel Gain Faders:** Adjust gain from `-30.0 dB` to `+30.0 dB` with fine 0.1 dB resolution. Double-click any fader to reset to `0.0 dB`.
- **Peak & RMS Meters:** Real-time dual metering with clip warnings that decay smoothly to silence when playback is paused.
- **Solo Channel Preview & Lock (Latch):**
  - *Click & Hold:* Temporarily solo any speaker node or channel strip.
  - *Double-Click:* Lock/Latch solo on a channel for uninterrupted listening.
  - *Single-Click:* Release solo lock.
- **Filter Power Switch (`⏻`):** Quickly toggle the active filter on or off without opening source filters.
- **Filter Settings Shortcut (`⚙`):** Opens the OBS source filter dialog directly.
- **Jump to Active Scene Source (`⤵`):** Intelligently switches the dock focus to the audio source currently active in your live program scene (falls back to global audio if none).
- **Flexible UI Splitter:** Resizable vertical divider between the circular panner and channel faders.

#### 2. Sorround Monitor Dock
- Global master monitoring dock to inspect audio distribution and active channel activity across your OBS setup.
- Symmetrical channel labels with centered meter bars and glowing audio nodes.

---

## 📦 Installation

### Pre-built Binary (Recommended)
1. Download the latest release `.zip` (`aeria-surroundupmix-vX.X.X-windows-x64.zip`) from the Releases section.
2. Close **OBS Studio** if it is running.
3. Extract the contents directly into your OBS Studio root directory (e.g. `C:\Program Files\obs-studio\`):
   ```text
   obs-studio/
   ├── obs-plugins/
   │   └── 64bit/
   │       ├── aeria-surroundupmix.dll
   │       └── aeria-surroundupmix.pdb
   └── data/
       └── obs-plugins/
           └── aeria-surroundupmix/
               └── locale/
                   ├── en-US.ini
                   └── id-ID.ini
   ```
4. Launch OBS Studio.

---

## 🚀 Quick Start Guide

1. **Configure OBS Audio Output:**
   - In OBS Studio, open **Settings** → **Audio**.
   - Set **Channels** to **5.1** or **7.1**.
   - Restart OBS if prompted.

2. **Add the Filter:**
   - Right-click any audio source in the **Audio Mixer** (e.g. Desktop Audio, Media Source, Game Capture) → select **Filters**.
   - Click `+` under Audio Filters and select **Sorround Upmix**.
   - Click **Open Sorround Control** inside the filter properties.

3. **Access Docks:**
   - Go to top menu: **Docks** → **Sorround Control** or **Sorround Monitor**.
   - Dock the panels anywhere in your OBS workspace.

---

## 🛠️ Building from Source

### Prerequisites
- Windows 10/11 64-bit
- Visual Studio 2022 (with Desktop development with C++ & MSVC v143)
- CMake 3.25 or newer
- PowerShell 5.1+

### Build Steps
1. Clone this repository:
   ```powershell
   git clone https://github.com/aeriacreative/obs-aeria-surroundupmix.git
   cd obs-aeria-surroundupmix
   ```
2. Build and package using the automated PowerShell script:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\build_and_install.ps1
   ```
   The script will download pre-built OBS Studio dependencies, configure CMake, compile `RelWithDebInfo`, and output the final ZIP package into the `dist/` folder.

---

## 📂 Project Structure

```text
aeria-surroundupmix/
├── cmake/                      # CMake build scripts and helper modules
├── data/
│   └── locale/                 # Localization files (en-US, id-ID)
├── src/
│   ├── audio/
│   │   ├── channel-layout.hpp  # Surround channel definitions and mappings
│   │   ├── level-meter.*       # Peak & RMS audio metering engine
│   │   ├── upmix-engine.*      # Core surround upmix DSP & biquad filters
│   │   └── surround-filter.*   # OBS source filter interface
│   ├── frontend/
│   │   ├── channel-strip-widget.*    # Individual channel gain fader & meter
│   │   ├── circular-speaker-widget.* # 2D circular panner and speaker nodes
│   │   ├── control-dock.*            # Main Sorround Control dock widget
│   │   ├── monitor-dock.*            # Master Sorround Monitor dock widget
│   │   └── dock-manager.*            # OBS dock registry and lifecycle manager
│   ├── state/
│   │   └── filter-state.*      # Filter parameter management & ref-counting
│   └── plugin-main.cpp         # OBS module entry point
├── build_and_install.ps1       # Automated build & install script
├── buildspec.json              # OBS plugin build specification
├── CMakeLists.txt              # CMake project definition
├── LICENSE                     # GNU General Public License v2.0
└── README.md                   # Project documentation
```

---

## 🛡️ License

This project is licensed under the **GNU General Public License v2.0 (GPL-2.0)** - see the [LICENSE](LICENSE) file for details.  
Compatible with the Open Broadcaster Software (OBS Studio) licensing terms.

---

## 💡 Credits & Support

Developed by **Aeria Creative**  
- 🌐 Linktree & Socials: [linktr.ee/aeriacreative](https://linktr.ee/aeriacreative)  
- 📧 Contact: aeriacreative@gmail.com
