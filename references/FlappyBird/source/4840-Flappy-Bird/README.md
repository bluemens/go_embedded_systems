# 4840-Flappy-Bird on FPGA (DE1-SoC)

This project implements a simplified version of **Flappy Bird** on the **DE1-SoC FPGA platform**, integrating a VGA display, USB keyboard input, and a Linux kernel device driver. The game logic is implemented in SystemVerilog, and a C-based user-space program interfaces with the custom driver.

---

Team: Ethan Yang, Sijun Li, Zidong Xu, Tianshuo Jin

## Components Overview

### 1. `vga_ball.sv`
- Implements the core Flappy Bird game logic, rendering to VGA output.
- Manages bird movement, scrolling pipes, score, and game states.
- Receives "flap" commands through a memory-mapped register interface.

### 2. `vga_ball.c` (Kernel Driver)
- A Linux platform driver that exposes a `/dev/vga_ball` device.
- Supports ioctl operations to write background color, bird position, and send a flap signal.

### 3. `hello.c`
- User-space C program that:
  - Polls a USB keyboard for input (spacebar for flap, ESC to exit).
  - Sends flap commands to the FPGA by writing to `/dev/vga_ball` via `ioctl`.

### 4. `usbkeyboard.c/.h`
- Uses `libusb` to find and interface with a standard USB HID keyboard.

