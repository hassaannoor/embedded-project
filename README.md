# Flappy Sketch (Pitch Controlled)

A Flappy Bird clone for Arduino Uno where the bird's vertical position is controlled by the pitch of your voice (or an audio signal), outputting video to a VGA monitor.

## Hardware Requirements

- **Arduino Uno** (ATmega328P @ 16MHz)
- **VGA Monitor** & Connector (DSUB15)
- **Audio Input** (Microphone amplifier or signal generator)
- **Resistors** for VGA DAC (see Pinout)

## Pinout

| Signal | Arduino Pin | AVR Pin | Notes |
|--------|-------------|---------|-------|
| **Pixel Out** | Digital 2 | PD2 | Connect to VGA Green (and/or Red/Blue) via resistor (e.g. 470Ω) |
| **HSYNC** | Digital 3 | PD3 | Connect to VGA HSYNC (Pin 13) via resistor (e.g. 68Ω) |
| **VSYNC** | Digital 9 | PB1 | Connect to VGA VSYNC (Pin 14) via resistor (e.g. 68Ω) |
| **Audio In** | Analog A0 | PC0 | 0-5V signal, biased to 2.5V |
| **Reset** | Digital 12 | PB4 | Optional button to ground for game reset |

**Note**: The pixel output is 1-bit (On/Off). Connect PD2 to the Green video pin for a green screen, or all three (R, G, B) for white.

## Software Architecture

The project is modularized into three main components:

1.  **VGA (`VGA.h`, `VGA.cpp`)**: Handles Timer1 (VSYNC) and Timer2 (HSYNC) interrupts and outputs pixel data from the buffer.
2.  **Audio (`Audio.h`, `Audio.cpp`)**: Configures the ADC in free-running mode and detects pitch using zero-crossing analysis.
3.  **Game (`Game.h`, `Game.cpp`)**: Contains the game logic, physics, and rendering routines.
4.  **Main (`flappy-sketch.ino`)**: Initializes modules and runs the main loop.

## Usage

1.  Open `flappy-sketch.ino` in the Arduino IDE.
2.  Upload to your Arduino Uno.
3.  Connect the VGA monitor and Audio source.
4.  **Play**: Hum or whistle into the microphone. Higher pitch moves the bird up, lower pitch moves it down. Avoid the pipes!

## Modifications

### Tuning Audio Sensitivity
In `Game.cpp`, adjust the pitch range to match your voice:
```cpp
#define MIN_PITCH 150    // Minimum frequency (Hz) -> Bottom of screen
#define MAX_PITCH 2000   // Maximum frequency (Hz) -> Top of screen
```

### Adjusting Game Speed
In `flappy-sketch.ino`, change the delay in the loop:
```cpp
delay(80); // Lower value = Faster game
```

### Changing Pipe Difficulty
In `Game.cpp`, adjust the spawn timer:
```cpp
if (pipeSpawnTimer > 30) { // Decrease for more frequent pipes
```

### VGA Resolution
The resolution is currently hardcoded to 40x30 logical pixels to fit within the ATmega328P's limited RAM and CPU time. Increasing this significantly is not recommended without careful optimization of the ISRs.
