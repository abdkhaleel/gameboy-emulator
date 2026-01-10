# GB-Emu: Cycle-Accurate Game Boy Emulator in C

![Language](https://img.shields.io/badge/language-C-blue.svg) ![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20(WSL)-lightgrey.svg) ![Library](https://img.shields.io/badge/library-SDL2-red.svg)

**GB-Emu** is a Game Boy (DMG) emulator written from scratch in C. It implements the Sharp LR35902 CPU, a FIFO-based PPU (Picture Processing Unit), and a memory-bus architecture that accurately simulates hardware timing and interrupts.

This project demonstrates low-level systems programming concepts including bitwise manipulation, memory mapping, hardware synchronization, and digital audio synthesis.


## 🚀 Features

*   **CPU Core:** Complete implementation of the Fetch-Decode-Execute cycle for the Sharp LR35902 (Z80/8080 hybrid).
*   **PPU (Graphics):** Cycle-accurate Pixel FIFO pipeline supporting Backgrounds, Window, and Sprites (OAM) with priority sorting.
*   **Memory Management:** 
    *   Memory Bank Controllers (MBC1, MBC3, MBC5) allowing support for complex games like *Pokemon Yellow* and *Legend of Zelda*.
    *   Battery-backed RAM persistence (Save files).
    *   Real-Time Clock (RTC) support for MBC3.
*   **Audio:** Digital synthesis of Square Waves (Channels 1 & 2) using SDL2 audio buffers.
*   **Timing:** Strict synchronization between CPU Machine Cycles (M-Cycles) and PPU/Timer Clock Cycles (T-Cycles).
*   **Architecture:** Multi-threaded design separating the Emulation Core from the UI Rendering thread