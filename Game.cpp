#include "Game.h"

// Game state
volatile uint16_t frameCount = 0;
volatile int birdY;      // actual bird row (0..SCREEN_H-1)
volatile float birdYf;   // float for smoothing

#define BIRD_X 6    // bird horizontal position (column)

struct Pipe {
  int x;   // column (0..SCREEN_W-1)
  int gapY; // top of gap (row)
  bool active;
};

#define MAX_PIPES 6
Pipe pipes[MAX_PIPES];
volatile int pipeSpawnTimer;
volatile int score;
volatile bool gameOver;
volatile bool paused;

// smoothing / mapping constants
#define MIN_PITCH 150    // Hz - lower bound for mapping
#define MAX_PITCH 2000   // Hz - upper bound for mapping
#define SMOOTHING 0.12   // smoothing factor for bird vertical movement [0..1]

void spawnPipe() {
  // find inactive slot
  for (int i = 0; i < MAX_PIPES; ++i) {
    if (!pipes[i].active) {
      pipes[i].active = true;
      pipes[i].x = SCREEN_W - 1;
      // gapY between 3 and SCREEN_H-6 to allow headroom
      pipes[i].gapY = random(3, SCREEN_H - 6);
      return;
    }
  }
}

void resetGame() {
  noInterrupts();
  for (int i = 0; i < MAX_PIPES; ++i) {
    pipes[i].active = false;
  }
  pipeSpawnTimer = 0;
  score = 0;
  gameOver = false;
  paused = false;
  birdYf = SCREEN_H / 2;
  birdY = (int)birdYf;
  interrupts();
}

void setupGame() {
  randomSeed(analogRead(A3)); // seed RNG from floating pin
  // pins for optional button
  pinMode(12, INPUT_PULLUP);
  
  frameCount = 0;
  resetGame();
}

void updateGame() {
    // optional reset button
  if (digitalRead(12) == LOW) {
    resetGame();
    delay(200);
  }

  if (gameOver) {
      return;
  }

  // Update detected pitch into bird target
  uint16_t freq = detectedFreqHz; // atomic-ish read (volatile)
  // Map frequency to 0..SCREEN_H-1
  // Clamp first
  if (freq < MIN_PITCH) freq = MIN_PITCH;
  if (freq > MAX_PITCH) freq = MAX_PITCH;
  float norm = (float)(freq - MIN_PITCH) / (float)(MAX_PITCH - MIN_PITCH);
  // invert: higher pitch -> smaller row index (higher on screen)
  float targetYf = (1.0f - norm) * (SCREEN_H - 1);

  // Smooth the bird vertical movement
  birdYf = (SMOOTHING * birdYf) + ((1.0 - SMOOTHING) * targetYf);
  birdY = constrain((int)(birdYf + 0.5f), 0, SCREEN_H - 1);

  // Pipe spawning and movement frequency: spawn approx every N frames
  pipeSpawnTimer++;
  if (pipeSpawnTimer > 30) { // spawn more often for faster gameplay; tune as needed
    spawnPipe();
    pipeSpawnTimer = 0;
  }

  // Move pipes left and check collisions
  for (int i = 0; i < MAX_PIPES; ++i) {
    if (!pipes[i].active) continue;
    pipes[i].x -= 1; // move left by 1 column per loop iteration (tune via delay())
    if (pipes[i].x < -2) { // off screen: deactivate
      pipes[i].active = false;
      continue;
    }
    // Collision check: if bird in same column as pipe
    if ((pipes[i].x <= BIRD_X + 1) && (pipes[i].x + 1 >= BIRD_X)) {
      // bird collides if it's not inside the gap
      if ((birdY < pipes[i].gapY) || (birdY > pipes[i].gapY + 3)) {
        gameOver = true;
      } else {
        // bird inside gap -> score increments when passing center
        if (pipes[i].x == BIRD_X - 1) {
          score++;
        }
      }
    }
  }

  // Ground and ceiling collision
  if (birdY <= 0 || birdY >= SCREEN_H - 1) {
    gameOver = true;
  }
}

void drawGame() {
  // Clear buffer (background 1)
  for (int r = 0; r < SCREEN_H; ++r) {
    for (int c = 0; c < SCREEN_W; ++c) {
      PXL_DATA[r][c] = 1;
    }
  }

  // Draw bird (3x2 simple)
  int by = birdY;
  int bx = BIRD_X;
  for (int r = by - 1; r <= by + 1; ++r) {
    for (int c = bx; c < bx + 2; ++c) {
      if (r >= 0 && r < SCREEN_H && c >= 0 && c < SCREEN_W) {
        PXL_DATA[r][c] = 0;
      }
    }
  }

  // Draw pipes (solid columns with gap)
  for (int i = 0; i < MAX_PIPES; ++i) {
    if (!pipes[i].active) continue;
    int px = pipes[i].x;
    int gap = pipes[i].gapY;
    for (int r = 0; r < SCREEN_H; ++r) {
      if (r >= gap && r <= gap + 3) continue; // gap
      for (int c = px; c < px + 2; ++c) {
        if (c >= 0 && c < SCREEN_W && r >= 0 && r < SCREEN_H) {
          PXL_DATA[r][c] = 0;
        }
      }
    }
  }

  // Draw ground line (optional)
  int groundRow = SCREEN_H - 1;
  for (int c = 0; c < SCREEN_W; ++c) {
    PXL_DATA[groundRow][c] = 0;
  }

  // Draw score at top-left as a bar
  int sc = score % (SCREEN_W - 2);
  for (int c = 0; c < sc; ++c) {
    PXL_DATA[0][c] = 0;
  }

  // If game over, flash a pattern
  if (gameOver) {
    int t = (frameCount >> 2) & 1;
    if (t) {
      // fill whole screen inverted
      for (int r = 1; r < SCREEN_H - 1; ++r)
        for (int c = 1; c < SCREEN_W - 1; ++c)
          PXL_DATA[r][c] = 0;
    }
  }
  
  frameCount++;
}
