#include "VGA.h"

volatile uint8_t PXL_DATA[SCREEN_H][SCREEN_W];
volatile int lineCounter;
volatile uint8_t skipLine;
volatile uint8_t PXL_OUT;

// small nop macro
#define nop __asm__ __volatile__ ("nop\n\t")

// Timer1: vertical (VSYNC) generator.
ISR(TIMER1_OVF_vect) {
  lineCounter = 0;
}

// Allow sleeping in TIMER2 overflow
ISR(TIMER2_OVF_vect) {
  sei();
  __asm__ __volatile__ ("sleep \n\t");
}

// Pixel output: fires during visible line time
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

void setupVGA() {
  cli(); // disable interrupts
  // Setup output pins
  DDRD |= (_BV(PD2) | _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7)); 
  PORTD = 0;

  // Configure Timer1 for VSYNC
  TCCR1A = _BV(COM1A0) | _BV(WGM10) | _BV(WGM11); 
  TCCR1B = _BV(WGM12) | _BV(WGM13) | _BV(CS12) | _BV(CS10); // prescaler 1024
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
