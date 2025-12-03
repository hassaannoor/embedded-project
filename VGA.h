#ifndef VGA_H
#define VGA_H

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// VGA screen size (logical)
#define SCREEN_W 40
#define SCREEN_H 30

// Pixel buffer: 1 = background, 0 = pixel ON
extern volatile uint8_t PXL_DATA[SCREEN_H][SCREEN_W];

// Setup function
void setupVGA();

#endif
