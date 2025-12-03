#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// Pitch detection variables
extern volatile uint16_t detectedFreqHz;

// Setup function
void setupAudio();

#endif
