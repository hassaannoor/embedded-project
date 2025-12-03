#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include "VGA.h"
#include "Audio.h"

// Game setup and loop functions
void setupGame();
void updateGame();
void drawGame();

// Global game state (if needed externally, otherwise kept internal to Game.cpp)
// For now, we keep them internal or expose via functions if necessary.

#endif
