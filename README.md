# DDR Arcade — ESP32 Pad + Laptop Visuals

Physical FSR dance pad + speaker, driven by an ESP32, with game visuals
shown on a laptop screen (TFT display currently non-functional — this is
the working fallback).

## How it works

```
[4x FSR pads] --> [ESP32] --> USB Serial "HIT:0..3" --> [Laptop: SDL2 game]
                      |
                      v
              [I2S Amp + Speaker]
                (plays music/tones)
```

- The **ESP32** reads pressure from 4 FSR pads (Left/Down/Up/Right),
  drives the I2S amp + speaker for audio, and sends a `HIT:<lane>` message
  over USB serial every time a pad is pressed.
- The **laptop program** shows the falling-note visuals, hit line, score,
  and combo. It listens on the ESP32's serial port and increases score
  each time a `HIT:x` message arrives. Keyboard (A/S/W/D) is also wired
  up as a fallback for testing the visuals without the pad connected.

## Repo structure

```
firmware/           ESP32 code — FSR reading + I2S speaker + serial reporting
laptop_visual/       Laptop C/SDL2 code — visuals, score, serial listener
docs/                 Wiring diagrams, photos
```

## Setup

### 1. Firmware (ESP32)
- Open `firmware/` in PlatformIO or Arduino IDE
- Update FSR pin numbers and `FSR_THRESHOLD` in `main.cpp` to match your build
- Flash to the ESP32
- Open Serial Monitor at 115200 baud to confirm `HIT:0` / `HIT:1` etc. print when you press each pad

### 2. Laptop visuals
- Requires SDL2 + SDL2_ttf (see build instructions in `laptop_visual/ddr_visual.c` header comment)
- **Before building**, open `ddr_visual.c` and set `SERIAL_PORT` to match your ESP32's COM port (check Windows Device Manager → Ports)
- Build and run:
  ```bash
  gcc ddr_visual.c -o ddr_visual.exe -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lm
  ./ddr_visual.exe
  ```
- If the serial port can't be opened, the game still runs in keyboard-only
  mode (A/S/W/D) — useful for testing visuals without the pad connected

## Status / Known limitations

- TFT display (ST7735) is non-functional — laptop display is the current fallback
- FSR thresholds need per-pad calibration (pressure-sensitive resistors vary)
- *(update this section as your build progresses)*
