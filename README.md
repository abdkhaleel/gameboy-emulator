
# GB-Emu: Cycle-Accurate Game Boy Emulator

GB-Emu is a cycle-accurate Nintendo Game Boy (DMG-01) emulator written from scratch in C. It faithfully emulates the Sharp LR35902 CPU, a FIFO-based PPU pipeline, and a hardware-accurate memory bus, maintaining strict timing synchronization across the CPU, PPU, timers, and interrupts.

This project focuses on hardware correctness, low-level system design, and accurate timing behavior, rather than high-level abstractions or per-game hacks.

## 📌 Repository Status

**Important Notice:** This repository is the final and actively maintained version of the Game Boy emulator project.

The earlier repository (`abdkhaleel/GameBoy-Emulator`) is **deprecated** and will not receive further updates. All future development, fixes, and enhancements will be published exclusively in this repository.

## 🎯 Project Goals

*   **Hardware Accuracy:** Emulate original DMG hardware behavior rather than targeting specific game compatibility.
*   **Cycle Accuracy:** Maintain strict timing across all subsystems.
*   **Modular Design:** Use a clean architecture that mirrors real hardware blocks.
*   **Educational Value:** Serve as a reference for systems programming and emulator development.

## 🚀 Features

### 🧠 Central Processing Unit (Sharp LR35902)
*   Full Fetch–Decode–Execute pipeline.
*   Complete instruction set implementation (Z80/8080 hybrid).
*   Accurate flag behavior and edge case handling.
*   Interrupt handling with correct priority and timing execution.

### 🎨 Pixel Processing Unit (PPU)
*   Cycle-accurate FIFO pixel pipeline implementation.
*   Support for Background, Window, and Sprite rendering.
*   OAM sprite priority sorting and pixel masking.
*   Accurate LCD mode switching (OAM Scan, Drawing, HBlank, VBlank).
*   Hardware-accurate STAT and VBlank interrupt generation.

### 🧩 Memory System
*   Unified memory bus architecture.
*   Proper memory mirroring and handling of forbidden regions.
*   **Memory Bank Controller (MBC) Support:**
    *   MBC1
    *   MBC3 (including Real-Time Clock)
    *   MBC5
*   Battery-backed RAM support for save file persistence.

### ⏱️ Timing & Synchronization
*   Strict synchronization between:
    *   CPU M-Cycles
    *   PPU T-Cycles
    *   Timer and Divider registers
*   Accurate timer overflow and interrupt timing.
*   Implementation avoids frame-based shortcuts in favor of cycle-stepping.

### 🔊 Audio (APU)
*   Digital synthesis of Square Wave channels (CH1 & CH2).
*   SDL2-based audio buffering.
*   Frequency sweep and envelope handling.

### 🧵 System Architecture
*   **Multi-threaded Design:** Separation of the Emulation Core (CPU/PPU/APU/Timers) and the UI/Rendering thread.
*   Clean separation of hardware components.
*   Designed to scale toward full APU and Game Boy Color (CGB) support.

## 🛠️ Technical Stack

*   **Language:** C (C99 Standard)
*   **Graphics & Audio:** SDL2
*   **Platform:** Linux, Windows (via WSL)
*   **Build System:** Make

## ▶️ Build Instructions

### Prerequisites
Ensure the following are installed on your system:
*   GCC or Clang compiler
*   Make
*   SDL2 development libraries

### Building the Project
Run the following command in the root directory:

```bash
make
```

### Running the Emulator
Execute the binary with the path to a Game Boy ROM:

```bash
./emu path/to/rom.gb
```

*Note: Save files (.sav) are created automatically for cartridges that support battery-backed RAM.*

## 🧪 Compatibility

The emulator has been tested with the following titles, specifically focusing on timing correctness rather than simple boot success:

*   Pokémon Yellow
*   The Legend of Zelda: Link’s Awakening
*   Tetris
*   Super Mario Land

## 📈 Future Work

*   Full APU implementation (Wave and Noise channels: CH3, CH4).
*   Game Boy Color (CGB) support.
*   Improved audio accuracy.
*   Integrated Debugger (CPU/PPU step-through).
*   Automated test ROM integration (Blargg, Mooneye).

## 📜 License

This project is released under the MIT License. Please refer to the [LICENSE](LICENSE) file for details.

## 👤 Author

**Abdul Khaleel**
*Systems Programming | Distributed Systems | Low-level Architecture*
