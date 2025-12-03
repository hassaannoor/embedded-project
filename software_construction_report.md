# Software Construction Principles Report

This report analyzes the refactoring of the `flappy-sketch` project, highlighting how the new structure adheres to fundamental software construction principles.

## 1. Modularity
**Principle**: Breaking a complex system into smaller, manageable, and independent modules.

**Application**:
The monolithic `flappy-sketch.ino` file was decomposed into three distinct modules:
- **VGA Module (`VGA.h/cpp`)**: Dedicated solely to video signal generation and pixel buffer management.
- **Audio Module (`Audio.h/cpp`)**: Dedicated to ADC configuration and pitch detection algorithms.
- **Game Module (`Game.h/cpp`)**: Dedicated to game rules, state management, and rendering logic.

**Benefit**: Each module can be developed, tested, and understood in isolation. For example, the `Audio` module could be reused in a different project requiring pitch detection without dragging in VGA code.

## 2. Separation of Concerns (SoC)
**Principle**: Distinct sections of the computer program should address distinct concerns.

**Application**:
- **Hardware Drivers vs. Application Logic**:
    - The **VGA** and **Audio** modules act as hardware drivers. They interact directly with registers (`TCCR1A`, `ADMUX`, etc.) and interrupts.
    - The **Game** module contains the high-level application logic (bird physics, collision detection). It doesn't need to know *how* a pixel is put on the screen, only that it *can* put a pixel on the screen via the buffer.
- **Main Sketch as Coordinator**: The `.ino` file now acts purely as an entry point and coordinator, calling `setup()` and `update()` functions, rather than containing implementation details.

**Benefit**: Changes to the hardware implementation (e.g., switching to a different display driver) would not require changing the game logic, and vice versa.

## 3. Information Hiding (Encapsulation)
**Principle**: Hiding the internal details of a module and exposing only what is necessary.

**Application**:
- **Internal State**: Variables like `lineCounter` (VGA) or `zeroCrossings` (Audio) are defined in their respective `.cpp` files and are not exposed in the `.h` files. They are internal implementation details.
- **Public Interface**: The header files (`.h`) expose only the necessary functions (`setupVGA`, `setupAudio`, `setupGame`) and essential shared data (`PXL_DATA`, `detectedFreqHz`).

**Benefit**: Reduces the cognitive load for developers using these modules and prevents accidental modification of internal state from outside the module.

## 4. Abstraction
**Principle**: Reducing complexity by hiding unnecessary details.

**Application**:
- The `Game` module interacts with `Audio` via a high-level abstraction: `detectedFreqHz`. It doesn't deal with raw ADC samples or zero-crossing math.
- The `Game` module interacts with `VGA` via the `PXL_DATA` buffer. It draws to a logical grid, abstracting away the complex timing requirements of the VGA signal generation.

**Benefit**: Allows the game logic to be written in terms of the problem domain (frequency, pixels) rather than the solution domain (interrupt vectors, register bit-shifting).

## 5. Single Responsibility Principle (SRP)
**Principle**: A module or class should have only one reason to change.

**Application**:
- If we want to change the VGA resolution, we only modify `VGA.h/cpp`.
- If we want to change the pitch detection algorithm (e.g., to FFT), we only modify `Audio.cpp`.
- If we want to change the game rules (e.g., add a new obstacle), we only modify `Game.cpp`.

**Benefit**: Makes the codebase more robust and easier to maintain, as changes are localized and less likely to introduce regression bugs in unrelated areas.
