/*
  Flappy Bird (pitch-controlled) -> VGA output
  Target: Arduino Uno (ATmega328P, 16 MHz)
  VGA resolution: 40 x 30 (very low, drawn using timer interrupts)
  Audio input: A0 (biased to mid-rail, analog)
  Pitch detection: zero-crossing detection on ADC free-run samples
*/

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>

// VGA screen size (logical)
#define SCREEN_W 40
#define SCREEN_H 30

volatile uint8_t PXL_DATA[SCREEN_H][SCREEN_W]; // 1 = background, 0 = pixel ON (as in original)
volatile int lineCounter;
volatile uint8_t skipLine;
volatile uint8_t PXL_OUT;

// Game state (shared between loop and rendering). Keep small and volatile where needed.
volatile int birdY_fixed; // smoothed bird Y (0..SCREEN_H-1), set by audio
volatile uint16_t frameCount; // increments each frame

// Bird and pipes
#define BIRD_X 6    // bird horizontal position (column)
volatile int birdY;      // actual bird row (0..SCREEN_H-1)
volatile float birdYf;   // float for smoothing

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

// Pitch detection variables (ADC)
volatile uint16_t adcSampleCount = 0;
volatile uint16_t zeroCrossings = 0;
volatile int16_t lastSample = 0;
volatile uint32_t adcSampleRate = 125000UL; // approx with prescaler 128 -> 125 kHz
// We'll measure over a small window (e.g. 1024 samples) to estimate freq
#define SAMPLE_WINDOW 1024
volatile uint16_t samplesInWindow = 0;
volatile uint16_t detectedFreqHz = 0; // estimated frequency in Hz (updated in ADC ISR)
volatile uint16_t freqAvailableCounter = 0;

// smoothing / mapping constants
#define MIN_PITCH 150    // Hz - lower bound for mapping
#define MAX_PITCH 2000   // Hz - upper bound for mapping
#define SMOOTHING 0.12   // smoothing factor for bird vertical movement [0..1]

/* --- VGA-related interrupts and timers. Based largely on original code. --- */

// small nop macro
#define nop __asm__ __volatile__ ("nop\n\t")

// Timer1: vertical (VSYNC) generator. ISR(TIMER1_OVF_vect) resets lineCounter each frame.
// Timer2: horizontal (HSYNC) generator and pixel output via OCR2B compare interrupt.

// Tweak: keep types safe for interrupts
ISR(TIMER1_OVF_vect) {
  lineCounter = 0;
}

// Allow sleeping in TIMER2 overflow
ISR(TIMER2_OVF_vect) {
  sei();
  __asm__ __volatile__ ("sleep \n\t");
}

// Pixel output: fires during visible line time, streams one row's columns to PORTD bits
ISR(TIMER2_COMPB_vect) {
  register uint8_t columnCounter;
  register uint8_t *rowPtr;

  switch (PXL_OUT) {
    case 1:
      // Map visible VGA line to our row index (every 16 VGA lines per logical row)
      // same mapping as original: (lineCounter - 35) >> 4
      rowPtr = (uint8_t*)&(PXL_DATA[(lineCounter - 35) >> 4][0]);
      columnCounter = SCREEN_W;
      while (columnCounter--) {
        // Output the pixel byte to PORTD pins (shifted to PD2..PD5)
        // We want the LSB of rowPtr to be on PD2, etc.
        PORTD = ((*(rowPtr++)) << PD2);
        nop;
      }
      nop;
      nop;
      nop;
      PORTD = 0;
      break;
  }

  lineCounter++;
  skipLine = !skipLine;

  if (skipLine && (lineCounter >= 35) && (lineCounter < 515)) {
    PXL_OUT = 1;
  } else {
    PXL_OUT = 0;
  }
}

/* --- ADC free-running ISR for sampling and zero-cross detection --- */
ISR(ADC_vect) {
  // Read ADC (10-bit)
  uint16_t val = ADC; // 0..1023
  // Convert to centered signed value around 0
  int16_t centered = (int16_t)val - 512;
  samplesInWindow++;
  adcSampleCount++;

  // zero-cross detection (positive-going)
  if ((lastSample <= 0) && (centered > 0)) {
    zeroCrossings++;
  }
  lastSample = centered;

  if (samplesInWindow >= SAMPLE_WINDOW) {
    // time for this window = SAMPLE_WINDOW / adcSampleRate seconds
    // number of full cycles = zeroCrossings / 1 (positive-going per cycle)
    // frequency = zeroCrossings / window_time
    float windowTime = ((float)SAMPLE_WINDOW) / (float)adcSampleRate;
    float freq = ((float)zeroCrossings) / windowTime;
    if (freq < 10000.0) {
      detectedFreqHz = (uint16_t)freq;
    } else {
      detectedFreqHz = 0;
    }
    // reset counters
    samplesInWindow = 0;
    zeroCrossings = 0;
    freqAvailableCounter++;
  }
}

/* --- Setup and initialization --- */
void setupTimersAndVGA() {
  cli(); // disable interrupts
  // Setup output pins
  DDRD |= (_BV(PD2) | _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7)); // we'll use PD2..PD7 to be safe (but output uses PD2..PD5)
  PORTD = 0;

  // Configure Timer1 for VSYNC (as original)
  // TCCR1A = 0b00100011; TCCR1B = 0b00011101; TIMSK1 = 0b00000001; OCR1A=259; OCR1B=0;
  TCCR1A = _BV(COM1A0) | _BV(WGM10) | _BV(WGM11); // similar to original
  TCCR1B = _BV(WGM12) | _BV(WGM13) | _BV(CS12) | _BV(CS10); // prescaler 1024 (CS12+CS10)
  TIMSK1 = _BV(TOIE1); // overflow interrupt enable
  OCR1A = 259;
  OCR1B = 0;

  // Configure Timer2 for HSYNC and match compare B for pixel output
  TCCR2A = _BV(COM2B0) | _BV(WGM20) | _BV(WGM21); // Fast PWM
  TCCR2B = _BV(WGM22) | _BV(CS21); // prescaler 8
  TIMSK2 = _BV(TOIE2) | _BV(OCIE2B); // overflow and compare B interrupts
  OCR2A = 63;  // line freq
  OCR2B = 7;   // hsync pulse width

  // Sleep control to allow low-power between lines
  SMCR = _BV(SE);

  // Clear pixel buffer
  for (int r = 0; r < SCREEN_H; r++) for (int c = 0; c < SCREEN_W; c++) PXL_DATA[r][c] = 1;

  lineCounter = 0;
  skipLine = 0;
  PXL_OUT = 0;

  sei(); // enable interrupts
}

/* --- ADC free-run initialization --- */
void setupADC() {
  // A0 analog input configured for ADC free-running
  ADMUX = _BV(REFS0); // AVcc reference, ADC0 channel (A0)
  // Setup ADC: enable, start, auto trigger (free running), interrupt enabled, prescaler 128
  // ADCSRA: ADEN ADATE ADIE ADSC ADPS2:0
  ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADATE) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
  // ADCSRB ADTS bits = 0 for free running
  ADCSRB = 0;
  // small settle delay
  delay(5);
}

/* --- Utility functions --- */

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

/* --- Main setup --- */
void setup() {
  // Serial for debugging (optional)
  Serial.begin(115200);
  randomSeed(analogRead(A3)); // seed RNG from floating pin

  // pins for optional button
  pinMode(12, INPUT_PULLUP);

  // VGA & timers set
  setupTimersAndVGA();

  // ADC free-run set
  setupADC();

  // initial game state
  frameCount = 0;
  resetGame();
}

/* --- Game update: called from loop (non-ISR context) --- */
void updateGameLogic() {
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

/* --- Draw frame to PXL_DATA[][] --- */
void drawFrameToBuffer() {
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
}

/* --- Main loop --- */
void loop() {
  // optional reset button
  if (digitalRead(12) == LOW) {
    resetGame();
    delay(200);
  }

  if (!gameOver) {
    updateGameLogic();
  } else {
    // if game over, keep updating bird from pitch but do not move pipes; allow reset
    // optional auto-reset after long time
  }

  // Render frame
  drawFrameToBuffer();

  // keep frame pacing moderate so the game isn't too fast; tune delay as needed.
  // Note: VGA is driven by hardware interrupts, this delay only controls game speed.
  frameCount++;
  delay(80); // tune between 30..120 ms for speed. Smaller -> faster
}

