# GB-Emu: Cycle-Accurate Game Boy Emulator

A cycle-accurate Nintendo Game Boy (DMG-01) emulator written from scratch in C. This project faithfully recreates the original Game Boy hardware architecture, featuring a fully emulated Sharp LR35902 CPU, a FIFO-based pixel pipeline PPU, and a hardware-accurate memory bus with strict timing synchronization across all subsystems.

> **Focus:** Hardware correctness, low-level systems programming, and accurate timing behavior -- not high-level abstractions or per-game compatibility hacks.

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
  - [High-Level Design](#high-level-design)
  - [CPU (Sharp LR35902)](#cpu-sharp-lr35902)
  - [PPU (Pixel Processing Unit)](#ppu-pixel-processing-unit)
  - [Memory & Bus System](#memory--bus-system)
  - [APU (Audio Processing Unit)](#apu-audio-processing-unit)
  - [Timer & Divider](#timer--divider)
  - [DMA Controller](#dma-controller)
  - [Interrupt System](#interrupt-system)
  - [Input Handling](#input-handling)
  - [Cartridge & MBC Support](#cartridge--mbc-support)
- [Project Structure](#project-structure)
- [File Reference](#file-reference)
- [Build Instructions](#build-instructions)
- [Usage](#usage)
- [Controls](#controls)
- [Compatibility](#compatibility)
- [Debugging Features](#debugging-features)
- [Technical Specifications](#technical-specifications)
- [Future Roadmap](#future-roadmap)
- [Contributors](#contributors)
- [License](#license)

---

## Overview

This emulator replicates the original Nintendo Game Boy (DMG-01) released in 1989. Unlike higher-level emulators that prioritize getting games running through shortcuts, this project aims for hardware-level accuracy by:

- Emulating individual CPU M-cycles and PPU T-cycles
- Implementing a FIFO-based pixel rendering pipeline matching real hardware
- Maintaining strict timing synchronization between CPU, PPU, timer, and audio
- Supporting Memory Bank Controllers (MBC) for different cartridge types
- Providing battery-backed RAM persistence for save data

The codebase is designed as both a functional emulator and an educational reference for systems programming, low-level architecture, and emulator development.

---

## Architecture

### High-Level Design

The emulator follows a modular architecture that mirrors the actual Game Boy hardware layout. The system is split into two threads:

```
+------------------+         +------------------+
|   Emulation Core |         |   UI / Rendering |
|   (CPU Thread)   |         |   (Main Thread)  |
|                  |         |                  |
|  +------------+  |         |  +------------+  |
|  |    CPU     |  |         |  |   SDL2     |  |
|  |  LR35902   |  |         |  |  Windows   |  |
|  +------------+  |         |  |  Renderer  |  |
|       |          |         |  +------------+  |
|  +------------+  |         |        |         |
|  |    Bus     |  |         |  +------------+  |
|  |  (Memory)  |<--------->|  |  Frame     |  |
|  +------------+  | shared  |  |  Buffer    |  |
|       |          | video   |  +------------+  |
|  +------------+  | buffer  |  +------------+  |
|  |    PPU     |  |         |  |  Debug     |  |
|  | (FIFO      |  |         |  |  Window    |  |
|  |  Pipeline) |  |         |  +------------+  |
|  +------------+  |         +------------------+
|       |          |
|  +------------+  |
|  |    APU     |  |
|  |  (Audio)   |  |
|  +------------+  |
|       |          |
|  +------------+  |
|  |   Timer    |  |
|  +------------+  |
|       |          |
|  +------------+  |
|  |    DMA     |  |
|  +------------+  |
+------------------+
```

The emulation core runs on a dedicated pthread, executing the CPU fetch-decode-execute loop while ticking the PPU, timer, APU, and DMA on each cycle. The main thread handles SDL2 window events and renders frames.

---

### CPU (Sharp LR35902)

The CPU is a custom 8-bit Sharp LR35902 processor -- a hybrid between the Intel 8080 and Zilog Z80, with some instructions removed and others added.

**Files:** `lib/cpu.c`, `lib/cpu_fetch.c`, `lib/cpu_proc.c`, `lib/cpu_util.c`, `lib/instructions.c`, `include/cpu.h`, `include/instructions.h`

**Key Features:**
- Full fetch-decode-execute pipeline
- Complete instruction set (all 256 primary opcodes + 256 CB-prefixed opcodes)
- 8-bit registers: A, B, C, D, E, F, H, L
- 16-bit register pairs: AF, BC, DE, HL, SP, PC
- Flag register with Zero (Z), Subtract (N), Half-carry (H), and Carry (C) flags
- Accurate flag behavior including edge cases (DAA, half-carry)
- Halt and Stop modes
- Interrupt Master Enable (IME) with correct timing (EI is delayed by one instruction)

**Instruction Categories:**
| Category | Instructions |
|----------|-------------|
| Load | LD, LDH (0x40-0x7F, 0xE0-0xF3) |
| Arithmetic | ADD, ADC, SUB, SBC, INC, DEC |
| Logic | AND, OR, XOR, CP, CPL, DAA, SCF, CCF |
| Shift/Rotate | RLCA, RRCA, RLA, RRA, RLC, RRC, RL, RR, SLA, SRA, SWAP, SRL |
| Bit Operations | BIT, SET, RES (CB prefix) |
| Jump | JP, JR, CALL, RET, RETI, RST |
| Stack | PUSH, POP |
| Control | NOP, HALT, STOP, DI, EI |

**Addressing Modes:**
- `AM_IMP` -- Implied (no operands)
- `AM_R` -- Register
- `AM_R_R` -- Register to register
- `AM_R_D8` -- Register + 8-bit immediate
- `AM_R_D16` -- Register + 16-bit immediate
- `AM_MR_R` -- Memory (register) = register
- `AM_R_MR` -- Register = memory (register)
- `AM_MR` -- Memory via register
- `AM_HLI_R` / `AM_R_HLI` -- HL indirect with post-increment
- `AM_HLD_R` / `AM_R_HLD` -- HL indirect with post-decrement
- `AM_A8_R` / `AM_R_A8` -- Zero-page (0xFF00 + 8-bit offset)
- `AM_A16_R` / `AM_R_A16` -- 16-bit absolute address
- `AM_HL_SPR` -- HL = SP + signed 8-bit

**CPU State (`cpu_context`):**
```c
typedef struct {
    cpu_registers regs;        // A, F, B, C, D, E, H, L, PC, SP
    u16 fetched_data;          // Data fetched for current instruction
    u16 mem_dest;              // Memory destination address
    bool dest_is_mem;          // True if destination is memory
    u8 cur_opcode;             // Current opcode byte
    instruction *cur_inst;     // Pointer to current instruction
    bool halted;               // CPU halt state
    bool stepping;             // Single-step debug mode
    bool int_master_enabled;   // Interrupt Master Enable flag
    bool enabling_ime;         // IME will be set after next instruction
    u8 ie_register;            // Interrupt Enable register (0xFFFF)
    u8 int_flags;              // Pending interrupt flags
} cpu_context;
```

---

### PPU (Pixel Processing Unit)

The PPU implements a cycle-accurate FIFO pixel pipeline that replicates the real Game Boy's rendering behavior.

**Files:** `lib/ppu.c`, `lib/ppu_sm.c`, `lib/ppu_pipeline.c`, `lib/lcd.c`, `include/ppu.h`, `include/lcd.h`, `include/ppu_sm.h`

**PPU Modes (LCD STAT):**
| Mode | Duration | Description |
|------|----------|-------------|
| OAM Scan | 80 T-cycles | Searches OAM for sprites on current scanline |
| Pixel Transfer | Variable | FIFO-based pixel shifting to LCD |
| H-Blank | Variable | Horizontal blanking period |
| V-Blank | 4560 T-cycles | Vertical blanking (10 scanlines) |

**Key Features:**
- FIFO-based pixel pipeline with 8-stage fetcher
- Background and Window layer rendering
- Sprite (OBJ) rendering with priority sorting
- OAM DMA transfer support
- LCDC register controls (BG enable, OBJ enable, Window enable, etc.)
- STAT interrupts (HBlank, VBlank, OAM, LYC=LY)
- Proper sprite limits (max 40 per screen, 10 per scanline)
- DMG-style 4-shade grayscale palette

**Display Specifications:**
- Resolution: 160 x 144 pixels
- Background: 256 x 256 pixels (32x32 tiles)
- Tile size: 8x8 pixels
- Colors: 4 shades (white, light gray, dark gray, black)

**FIFO Pipeline States:**
1. `FS_TILE` -- Read tile number from tile map
2. `FS_DATA0` -- Read low byte of tile data
3. `FS_DATA1` -- Read high byte of tile data
4. `FS_IDLE` -- Wait state
5. `FS_PUSH` -- Push pixels to FIFO

**LCD Controller Registers:**
| Register | Address | Description |
|----------|---------|-------------|
| LCDC | 0xFF40 | LCD control (enable, layers, tile map select) |
| STAT | 0xFF41 | LCD status (mode, interrupt select) |
| SCY | 0xFF42 | Scroll Y |
| SCX | 0xFF43 | Scroll X |
| LY | 0xFF44 | Current scanline (0-153) |
| LYC | 0xFF45 | LY compare |
| DMA | 0xFF46 | OAM DMA source address |
| BGP | 0xFF47 | Background palette |
| OBP0 | 0xFF48 | Object palette 0 |
| OBP1 | 0xFF49 | Object palette 1 |
| WY | 0xFF4A | Window Y position |
| WX | 0xFF4B | Window X position |

---

### Memory & Bus System

The memory bus implements the full 64KB Game Boy address space with proper mirroring, access control, and component routing.

**Files:** `lib/bus.c`, `lib/ram.c`, `include/bus.h`, `include/ram.h`

**Memory Map:**
```
0x0000 - 0x3FFF : ROM Bank 0 (16 KB, fixed)
0x4000 - 0x7FFF : ROM Bank N (16 KB, switchable via MBC)
0x8000 - 0x97FF : VRAM / Tile RAM (8 KB)
0x9800 - 0x9BFF : Background Map 1
0x9C00 - 0x9FFF : Background Map 2
0xA000 - 0xBFFF : Cartridge RAM (8 KB, external, battery-backed)
0xC000 - 0xCFFF : WRAM Bank 0 (4 KB)
0xD000 - 0xDFFF : WRAM Bank 1-7 (CGB only, not implemented)
0xE000 - 0xFDFF : Echo RAM (mirror of 0xC000-0xDDFF)
0xFE00 - 0xFE9F : OAM (Object Attribute Memory, 160 bytes)
0xFEA0 - 0xFEFF : Unused (returns 0)
0xFF00 - 0xFF7F : I/O Registers
0xFF80 - 0xFFFE : HRAM (High RAM / Zero Page, 127 bytes)
0xFFFF          : Interrupt Enable Register
```

**Bus Interface:**
```c
u8 bus_read(u16 address);           // Read 8-bit value from address
void bus_write(u16 address, u8 value);  // Write 8-bit value to address
u16 bus_read16(u16 address);        // Read 16-bit value (little-endian)
void bus_write16(u16 address, u16 value); // Write 16-bit value
```

**Memory Components:**
- **WRAM** (`ram.c`): 8KB working RAM at 0xC000-0xDFFF
- **HRAM** (`ram.c`): 127 bytes high RAM at 0xFF80-0xFFFE
- **VRAM** (`ppu.c`): 8KB video RAM at 0x8000-0x9FFF
- **OAM** (`ppu.c`): 160 bytes sprite attribute memory at 0xFE00-0xFE9F

---

### APU (Audio Processing Unit)

The APU generates Game Boy audio through digital synthesis.

**Files:** `lib/apu.c`, `include/apu.h`

**Implemented:**
- Square Wave Channel 1 (CH1) with frequency sweep
- Square Wave Channel 2 (CH2)
- Envelope volume control
- SDL2-based audio buffering

**Not Yet Implemented:**
- Wave Channel (CH3)
- Noise Channel (CH4)

---

### Timer & Divider

The timer system generates periodic interrupts for game timing.

**Files:** `lib/timer.c`, `include/timer.h`

**Registers:**
| Register | Address | Description |
|----------|---------|-------------|
| DIV | 0xFF04 | Divider register (increments at 16384 Hz) |
| TIMA | 0xFF05 | Timer counter |
| TMA | 0xFF06 | Timer modulo (reload value) |
| TAC | 0xFF07 | Timer control (enable + frequency select) |

**Timer Frequencies (TAC select):**
| Bits | Frequency |
|------|-----------|
| 00 | 4096 Hz |
| 01 | 262144 Hz |
| 10 | 65536 Hz |
| 11 | 16384 Hz |

The divider is driven by the system clock and increments on every 64th cycle. When TIMA overflows, it triggers a Timer interrupt and reloads from TMA.

---

### DMA Controller

Handles OAM DMA transfers for copying sprite data to the PPU.

**Files:** `lib/dma.c`, `include/dma.h`

When 0xFF46 (DMA register) is written, the DMA controller copies 160 bytes from the specified source address (high byte = written value) to OAM (0xFE00-0xFE9F). The transfer takes 640 cycles (160 bytes x 4 cycles each) with a 2-cycle startup delay.

During DMA transfer, the CPU cannot access OAM (reads return 0xFF, writes are ignored).

---

### Interrupt System

The Game Boy has five maskable interrupt sources, prioritized in this order:

**Files:** `lib/interrupts.c`, `include/interrupts.h`

| Interrupt | Bit | Vector | Trigger |
|-----------|-----|--------|---------|
| V-Blank | 0 | 0x0040 | LCD enters V-Blank mode |
| LCD STAT | 1 | 0x0048 | LCD state matches STAT selection |
| Timer | 2 | 0x0050 | TIMA overflow |
| Serial | 3 | 0x0058 | Serial transfer complete (not implemented) |
| Joypad | 4 | 0x0060 | Button pressed |

**Interrupt handling sequence:**
1. Interrupt source sets its bit in IF register (0xFF0F)
2. If IME is enabled and both IE and IF bits are set, CPU services interrupt
3. IME is reset (disabled)
4. Current PC is pushed to stack
5. CPU jumps to corresponding interrupt vector address
6. RETI instruction re-enables IME

---

### Input Handling

**Files:** `lib/gamepad.c`, `include/gamepad.h`

The Game Boy has 8 buttons mapped to a 2x4 matrix controlled by the JOYP register (0xFF00):

| Key | Game Boy Button |
|-----|----------------|
| Z | B |
| X | A |
| Enter | START |
| Tab | SELECT |
| Up Arrow | UP |
| Down Arrow | DOWN |
| Left Arrow | LEFT |
| Right Arrow | RIGHT |

The gamepad controller supports selecting either the direction pad or the button group for reading.

---

### Cartridge & MBC Support

**Files:** `lib/cart.c`, `include/cart.h`

The cartridge loader reads ROM files, parses the header, and sets up the appropriate Memory Bank Controller.

**ROM Header Structure:**
```c
typedef struct {
    u8 entry[4];           // Entry point instructions
    u8 logo[0x30];         // Nintendo logo data
    char title[16];        // Game title
    u16 new_lic_code;      // New licensee code
    u8 sgb_flag;           // SGB support flag
    u8 type;               // Cartridge type (determines MBC)
    u8 rom_size;           // ROM size indicator
    u8 ram_size;           // RAM size indicator
    u8 dest_code;          // Destination code
    u8 lic_code;           // Old licensee code
    u8 version;            // ROM version
    u8 checksum;           // Header checksum
    u16 global_checksum;   // Global checksum
} rom_header;
```

**Supported Cartridge Types:**

| Type | Description |
|------|-------------|
| ROM ONLY | No MBC |
| MBC1 | Memory Bank Controller 1 |
| MBC1+RAM | MBC1 with RAM |
| MBC1+RAM+BATTERY | MBC1 with battery-backed RAM |
| MBC3 | Memory Bank Controller 3 |
| MBC3+RAM | MBC3 with RAM |
| MBC3+RAM+BATTERY | MBC3 with battery-backed RAM |
| MBC3+TIMER+BATTERY | MBC3 with real-time clock |
| MBC5 | Memory Bank Controller 5 |
| MBC5+RAM | MBC5 with RAM |
| MBC5+RAM+BATTERY | MBC5 with battery-backed RAM |
| MBC5+RUMBLE | MBC5 with rumble (rumble not emulated) |
| MBC5+RUMBLE+RAM | MBC5 with rumble + RAM |
| MBC5+RUMBLE+RAM+BATTERY | MBC5 with rumble + battery RAM |

**MBC1 Features:**
- ROM banking (up to 125 banks / 2MB)
- RAM banking (up to 4 banks / 32KB)
- Advanced banking mode

**MBC3 Features:**
- ROM banking (up to 128 banks / 2MB)
- RAM banking (up to 4 banks / 32KB)
- Real-Time Clock (RTC) with 4 registers

**MBC5 Features:**
- ROM banking (up to 512 banks / 8MB) with 9-bit bank number
- RAM banking (up to 16 banks / 128KB)

**Battery Save System:**
- Save data is written to `<rom_file>.battery`
- Automatically saved at 60-second intervals
- Loaded automatically when ROM is started

---

## Project Structure

```
gameboy-emulator/
|
|-- include/              # Header files
|   |-- common.h          # Common types and macros
|   |-- cpu.h             # CPU context and interface
|   |-- instructions.h    # Instruction definitions
|   |-- ppu.h             # PPU context and FIFO pipeline
|   |-- lcd.h             # LCD controller registers
|   |-- ppu_sm.h          # PPU state machine functions
|   |-- apu.h             # Audio processing unit
|   |-- bus.h             # Memory bus interface
|   |-- cart.h            # Cartridge and ROM header
|   |-- ram.h             # Working RAM and HRAM
|   |-- timer.h           # Timer registers
|   |-- dma.h             # OAM DMA controller
|   |-- interrupts.h      # Interrupt system
|   |-- gamepad.h         # Input handling
|   |-- ui.h              # SDL2 user interface
|   |-- emu.h             # Main emulator context
|   |-- dbg.h             # Debug utilities
|   |-- stack.h           # Stack operations
|   |-- io.h              # I/O register handling
|
|-- lib/                  # Library source files
|   |-- instructions.c    # Instruction decode table (0x00-0xFF)
|   |-- cpu.c             # CPU main loop and step
|   |-- cpu_fetch.c       # Instruction fetch & addressing modes
|   |-- cpu_proc.c        # Instruction execution processors
|   |-- cpu_util.c        # CPU register read/write utilities
|   |-- ppu.c             # PPU core and VRAM/OAM access
|   |-- ppu_sm.c          # PPU state machine (OAM/XFER/HBLANK/VBLANK)
|   |-- ppu_pipeline.c    # FIFO pixel pipeline
|   |-- lcd.c             # LCD controller emulation
|   |-- apu.c             # Audio synthesis
|   |-- bus.c             # Memory bus routing
|   |-- cart.c            # Cartridge loader and MBC logic
|   |-- ram.c             # WRAM and HRAM emulation
|   |-- timer.c           # Timer and divider
|   |-- dma.c             # DMA transfer logic
|   |-- interrupts.c      # Interrupt dispatch
|   |-- gamepad.c         # Input state machine
|   |-- ui.c              # SDL2 rendering and event handling
|   |-- emu.c             # Emulator orchestration, threading
|   |-- dbg.c             # Debug logging
|   |-- stack.c           # Stack push/pop operations
|   |-- io.c              # I/O register read/write
|
|-- gbemu/                # Main application
|   |-- main.c            # Program entry point
|
|-- tests/                # Unit tests
|   |-- check_gbe.c       # Check framework test suite
|
|-- cmake/                # CMake configuration
|   |-- config.h.in       # Build configuration template
|   |-- sdl2/             # SDL2 find modules
|
|-- .vscode/              # VS Code settings
|-- CMakeLists.txt        # Main CMake configuration
|-- LICENSE               # MIT License
|-- NotoSansMono-Medium.ttf # Font for debug UI
```

---

## File Reference

### Core Emulation

| File | Lines (approx) | Description |
|------|----------------|-------------|
| `lib/instructions.c` | 500+ | Complete instruction decode table mapping all 256 opcodes to their type, addressing mode, registers, conditions, and parameters. Also includes instruction name lookup and disassembly formatting. |
| `lib/cpu.c` | 100+ | CPU initialization with post-boot register values, main step function implementing fetch-decode-execute cycle, halt handling, and interrupt checks. |
| `lib/cpu_fetch.c` | 200+ | Data fetching for all 21 addressing modes. Handles register reads, memory reads, immediate values, HL auto-increment/decrement, and zero-page access. |
| `lib/cpu_proc.c` | 600+ | Instruction execution for all instruction types: load/store, arithmetic, logic, rotates/shifts, bit operations, jumps/calls, stack operations, and control instructions. |
| `lib/cpu_util.c` | 100+ | Register read/write utilities for both 8-bit and 16-bit registers, flag manipulation. |

### Graphics

| File | Lines (approx) | Description |
|------|----------------|-------------|
| `lib/ppu.c` | 100+ | PPU initialization, tick dispatcher, VRAM/OAM read/write, video buffer management. |
| `lib/ppu_sm.c` | 200+ | PPU state machine implementing OAM scan, pixel transfer, H-blank, and V-blank modes with proper timing and STAT interrupts. |
| `lib/ppu_pipeline.c` | 300+ | FIFO pixel pipeline implementation: tile fetching, sprite fetching, pixel merging, background/window/sprite layering. |
| `lib/lcd.c` | 100+ | LCD controller register emulation, palette management, DMA trigger on 0xFF46 writes. |

### Memory & I/O

| File | Lines (approx) | Description |
|------|----------------|-------------|
| `lib/bus.c` | 100+ | Memory bus routing table directing reads/writes to the correct component (cart, PPU VRAM, WRAM, HRAM, I/O registers). |
| `lib/cart.c` | 500+ | ROM file loading, header parsing, checksum verification, MBC1/MBC3/MBC5 banking logic, RTC emulation, battery save/load. |
| `lib/ram.c` | 50+ | 8KB WRAM and 127B HRAM simple read/write. |
| `lib/io.c` | 100+ | I/O register read/write handling, gamepad register, serial port stubs. |
| `lib/dma.c` | 50+ | OAM DMA transfer with 2-cycle delay and 160-byte copy loop. |
| `lib/timer.c` | 50+ | Divider increment, timer tick with selectable frequency, overflow detection, interrupt generation. |

### Audio & Input

| File | Lines (approx) | Description |
|------|----------------|-------------|
| `lib/apu.c` | 150+ | Square wave generation for CH1 and CH2, frequency sweep, envelope volume, SDL2 audio callback. |
| `lib/gamepad.c` | 50+ | Button state tracking, JOYP register reading with direction/button group selection. |

### System & UI

| File | Lines (approx) | Description |
|------|----------------|-------------|
| `lib/emu.c` | 80+ | Main emulator context, pthread creation for CPU thread, cycle stepping (1 M-cycle = 4 T-cycles), frame synchronization. |
| `lib/ui.c` | 250+ | SDL2 initialization, main game window rendering (4x scale), debug tile viewer window, keyboard event handling. |
| `lib/interrupts.c` | 50+ | Interrupt request function, interrupt handler that dispatches to correct vector addresses. |
| `lib/stack.c` | 50+ | Stack push/pop for 8-bit and 16-bit values. |
| `lib/dbg.c` | 20+ | Debug output stub functions. |

---

## Build Instructions

### Prerequisites

- **C compiler:** GCC or Clang (C99 standard)
- **CMake:** Version 2.8 or higher
- **SDL2:** Development libraries (`libsdl2-dev`)
- **SDL2_ttf:** Development libraries (`libsdl2-ttf-dev`)
- **Check:** Unit testing framework (`check`)
- **pthread:** POSIX threads (usually included)

#### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake libsdl2-dev libsdl2-ttf-dev check
```

#### Fedora/RHEL

```bash
sudo dnf install gcc cmake SDL2-devel SDL2_ttf-devel check-devel
```

#### Arch Linux

```bash
sudo pacman -S base-devel cmake sdl2 sdl2_ttf check
```

#### macOS

```bash
brew install cmake sdl2 sdl2_ttf check
```

### Building

```bash
# Clone the repository
git clone https://github.com/abdkhaleel/gameboy-emulator.git
cd gameboy-emulator

# Create build directory
mkdir build && cd build

# Generate build files
cmake ..

# Build the project
make

# Run tests
make test
```

The compiled binary will be located at `build/gbemu/gbemu`.

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Debug` | Build configuration (Debug/Release) |

### Windows Build

For Windows, the project expects SDL2 and SDL2_ttf libraries in a `../windows_deps/` directory relative to the project root. Update the paths in `CMakeLists.txt` if needed, or use a package manager like vcpkg:

```bash
vcpkg install sdl2 sdl2-ttf
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake ..
```

---

## Usage

### Running the Emulator

```bash
./gbemu <path_to_rom_file>
```

### Example

```bash
./gbemu tetris.gb
./gbemu "pokemon red.gb"
```

### Save Files

Games that use battery-backed RAM will automatically create save files:
- Save file location: `<rom_file>.battery`
- Saves are written automatically every 60 seconds
- Saves are loaded automatically when the ROM starts

---

## Controls

| Keyboard | Game Boy |
|----------|----------|
| X | A Button |
| Z | B Button |
| Enter | START |
| Tab | SELECT |
| Up Arrow | D-Pad Up |
| Down Arrow | D-Pad Down |
| Left Arrow | D-Pad Left |
| Right Arrow | D-Pad Right |

---

## Compatibility

The following titles have been tested for timing correctness and playability:

| Game | Status | Notes |
|------|--------|-------|
| Pokemon Yellow | Working | MBC5, save support |
| Pokemon Red | Working | MBC3, save support |
| Pokemon Blue | Working | MBC3, save support |
| The Legend of Zelda: Link's Awakening | Working | MBC1, save support |
| Tetris | Working | ROM only, no save |
| Super Mario Land | Working | MBC1 |

**Mappers Tested:**
- MBC1 (Zelda, Super Mario Land)
- MBC3 (Pokemon Red/Blue)
- MBC5 (Pokemon Yellow)

---

## Debugging Features

The emulator includes a debug tile viewer window that displays all 384 tiles currently in VRAM:

- **Main Window:** 1024x768 (renders the 160x144 Game Boy screen at 4x scale)
- **Debug Window:** Shows all tiles from VRAM in a 16x24 grid layout

Tiles are rendered using the current background palette (BGP register), allowing you to inspect graphics data in real-time.

Enable CPU debug tracing by setting `CPU_DEBUG 1` in `lib/cpu.c` -- this prints each executed instruction with register state to stdout.

---

## Technical Specifications

### Emulated Hardware

| Specification | Value |
|---------------|-------|
| CPU | Sharp LR35902 (8-bit, Z80/8080 hybrid) |
| Clock Speed | 4.194304 MHz |
| RAM | 8 KB WRAM |
| Video RAM | 8 KB |
| Screen Resolution | 160 x 144 pixels |
| Colors | 4 shades of gray |
| Max Sprites | 40 per screen, 10 per scanline |
| Sprite Sizes | 8x8 or 8x16 |
| Audio Channels | 4 (2 square wave implemented) |

### Timing

| Parameter | Value |
|-----------|-------|
| CPU Machine Cycle | 4 clock ticks (T-cycles) |
| Horizontal Line | 456 T-cycles |
| Visible Lines | 144 |
| V-Blank Lines | 10 |
| Total Lines per Frame | 154 |
| T-cycles per Frame | 70,224 |
| Frame Rate | ~59.73 FPS |

### Code Statistics

| Language | Approximate Usage |
|----------|-------------------|
| C | ~62% |
| CMake | ~38% |

---

## Future Roadmap

### Audio
- [ ] Wave Channel (CH3) implementation
- [ ] Noise Channel (CH4) implementation
- [ ] Full audio mixing and frame sequencer
- [ ] Audio output on all platforms

### Graphics
- [ ] Game Boy Color (CGB) support
- [ ] Color palette emulation
- [ ] VRAM bank switching

### Debugging
- [ ] Integrated debugger with CPU step-through
- [ ] PPU state inspection
- [ ] Memory viewer/editor
- [ ] Breakpoint support

### Testing
- [ ] Blargg's CPU instruction test ROM integration
- [ ] Mooneye test suite integration
- [ ] Automated regression testing

### Platform Support
- [ ] Windows native build (MinGW/MSVC)
- [ ] macOS build optimization

---

## License

This project is released under the MIT License. See the [LICENSE](LICENSE) file for full details.

---

## Acknowledgments

- [Pan Docs](https://gbdev.io/pandocs/) -- The definitive Game Boy technical reference
- [gbdev.io](https://gbdev.io/) -- Game Boy development community resources
- The emulator development community for test ROMs and documentation

---

## Related Repositories

This is the actively maintained version of the emulator. The earlier repository (`abdkhaleel/GameBoy-Emulator-old`) is deprecated and will not receive further updates.
