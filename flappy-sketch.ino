/*
  Flappy Bird (pitch-controlled) -> VGA output
  Target: Arduino Uno (ATmega328P, 16 MHz)
  VGA resolution: 40 x 30 (very low, drawn using timer interrupts)
  Audio input: A0 (biased to mid-rail, analog)
  Pitch detection: zero-crossing detection on ADC free-run samples
*/

#include <Arduino.h>
#include "VGA.h"
#include "Audio.h"
#include "Game.h"

/* --- Main setup --- */
void setup() {
  // Serial for debugging (optional)
  Serial.begin(115200);

  // VGA & timers set
  setupVGA();

  // ADC free-run set
  setupAudio();

  // initial game state
  setupGame();
}

/* --- Main loop --- */
void loop() {
  updateGame();

  // Render frame
  drawGame();

  // keep frame pacing moderate so the game isn't too fast; tune delay as needed.
  // Note: VGA is driven by hardware interrupts, this delay only controls game speed.
  delay(80); // tune between 30..120 ms for speed. Smaller -> faster
}
