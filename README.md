GB-Emu
Cycle-Accurate Game Boy Emulator in C






GB-Emu is a cycle-accurate Nintendo Game Boy (DMG-01) emulator written from scratch in C.
It faithfully emulates the Sharp LR35902 CPU, a FIFO-based PPU pipeline, and a hardware-accurate memory bus, with strict timing synchronization across CPU, PPU, timers, and interrupts.

This project focuses on hardware correctness, low-level system design, and accurate timing behavior, rather than shortcuts or high-level abstractions.

📌 Repository Status

Important Notice
This repository is the final and actively maintained version of the Game Boy emulator project.

The earlier repository
👉 https://github.com/abdkhaleel/GameBoy-Emulator

is deprecated and will not receive further updates.

All future development, fixes, and enhancements are available only in this repository.

🎯 Project Goals

Emulate original DMG hardware behavior, not just game compatibility

Maintain cycle accuracy across all subsystems

Use a clean, modular architecture resembling real hardware blocks

Serve as a learning and demonstration project for systems programming

🚀 Features
🧠 CPU (Sharp LR35902)

Full Fetch–Decode–Execute pipeline

Complete instruction set (Z80 / 8080 hybrid)

Accurate flag behavior and edge cases

Interrupt handling with correct priority and timing

🎨 PPU (Graphics)

Cycle-accurate FIFO pixel pipeline

Background, Window, and Sprite rendering

OAM sprite priority sorting and pixel masking

Accurate LCD modes (OAM Scan, Drawing, HBlank, VBlank)

Hardware-accurate STAT and VBlank interrupts

🧩 Memory System

Unified memory bus architecture

Memory Bank Controller support:

MBC1

MBC3 (with RTC)

MBC5

Battery-backed RAM (save file persistence)

Proper memory mirroring and forbidden regions

⏱️ Timing & Synchronization

Strict synchronization between:

CPU M-Cycles

PPU T-Cycles

Timers and Divider registers

Accurate timer overflow and interrupt timing

No frame-based shortcuts

🔊 Audio (APU)

Digital synthesis of Square Wave channels (CH1 & CH2)

SDL2-based audio buffering

Frequency sweep and envelope handling (partial APU)

🧵 Architecture

Multi-threaded design

Emulation Core (CPU / PPU / APU / Timers)

UI & Rendering thread

Clean separation of hardware components

Designed to scale toward full APU and CGB support

🛠️ Tech Stack

Language: C (C99)

Graphics & Audio: SDL2

Platform: Linux, Windows (via WSL)

Build System: Make

📂 Project Structure (High Level)
src/
 ├── cpu/        # LR35902 CPU core
 ├── ppu/        # FIFO-based PPU pipeline
 ├── apu/        # Audio processing unit
 ├── mmu/        # Memory bus & MBCs
 ├── timers/     # DIV/TIMA logic
 ├── interrupts/ # Interrupt controller
 └── platform/   # SDL2 rendering & audio

▶️ Building & Running
Dependencies

SDL2 development libraries

GCC / Clang

Make

Build
make

Run
./gb-emu path/to/rom.gb


Save files (.sav) are created automatically for supported cartridges.

🧪 Tested With

Pokémon Yellow

The Legend of Zelda: Link’s Awakening

Tetris

Super Mario Land

(Testing focuses on timing correctness, not just boot success.)

📈 Future Work

Full APU implementation (CH3, CH4)

Game Boy Color (CGB) support

Improved audio accuracy

Debugger (CPU/PPU step-through)

Automated test ROM integration (Blargg, Mooneye)

📜 License

This project is released under the MIT License.
See the LICENSE file for details.

👤 Author

Abdul Khaleel
Systems Programming | Distributed Systems | Low-level Architecture
