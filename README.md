# ttygfx

**ttygfx** is a tiny, header-only terminal graphics library written in C.  
It uses ANSI **Truecolor escape sequences** to treat the terminal as a pixel grid, where a pair of characters (`"  "`) acts as a single pixel.

Designed to be:
- 📦 Single-header
- ⚡ Simple and fast
- 🧠 Pure C (no dependencies)1
- 🎨 24-bit Truecolor

---

## Features

- Truecolor pixel drawing (RGB)
- Cursor-based positioning
- Animated and static draw modes
- Bresenham-style line drawing
- Header-only (`static inline`)
- No buffers, no heap allocation

---

## How It Works

- Uses ANSI escape sequences for cursor movement and color
- Each pixel is rendered using `"  "` (two spaces)
- Truecolor via:
  - Foreground: `\033[38;2;<r>;<g>;<b>m`
  - Background: `\033[48;2;<r>;<g>;<b>m`
- Cursor positioning via `\033[<y>;<x>H`

---

## Requirements

- ANSI-compatible terminal
- Truecolor support
- C compiler (clang or gcc)

Tested on:
- Linux
- Android (Termux)

---

## Installation

Just include the header:

```c
#include "ttygfx.h"
```
a
No linking or complex build systems required.

## Core Types

