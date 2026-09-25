# DDR Arcade — ESP32 Dance Pad + Real-Time Game Engine

A physical Dance Dance Revolution cabinet: a custom 4-panel FSR dance pad
wired to an ESP32 that renders falling notes, scoring, and combo tracking in real time.

Built as a full hardware-to-software pipeline — sensor input, embedded
firmware, audio output, serial protocol, and a real-time rendering loop —
from scratch.

## Why we built this

We wanted hands-on experience across the full embedded systems stack: analog
sensor input, real-time signal processing on a microcontroller, a
communication protocol between hardware and a host machine, and a
performant game loop consuming that data live. This project touches
firmware, electronics, serial communication, and systems-level
C.

## How it works

```
[4x FSR pads] --> [ESP32] --> USB Serial "HIT:0..3" --> [Laptop: SDL2 game]
```

- **ESP32 firmware** polls 4 force-sensitive resistors (Left / Down / Up /
  Right), applies a per-pad pressure threshold, drives an I2S amplifier +
  speaker for audio feedback, and emits a HIT message over USB
  serial on every valid press.
- **Laptop game engine** (C + SDL2) renders falling notes, a hit line,
  live score, and combo streak. It listens on the ESP32's serial port and
  scores a hit each time a `HIT:x` message arrives. Keyboard input
  (A/S/W/D) is wired in parallel as a fallback, so the visuals can be
  developed and tested without the physical pad connected.

## Tech stack

| Layer              | Tech                                  |
|---------------------|----------------------------------------|
| Microcontroller     | ESP32 (C / PlatformIO)     |
| Sensors             | 4x Force-Sensitive Resistors (FSR)     |
| Communication       | USB Serial (custom `HIT` protocol) |
| Game engine         | C, SDL2, SDL2_ttf                      |
| Planned display     | ST7735 TFT (SPI)                       |

## Repo structure

```
firmware/         ESP32 code — FSR reading, I2S speaker output, serial reporting
laptop_visual/    C/SDL2 game — falling-note visuals, scoring, serial listener
docs/             Wiring diagrams, build photos
```

