#include "Audio.h"

volatile uint16_t detectedFreqHz = 0;
volatile uint16_t adcSampleCount = 0;
volatile uint16_t zeroCrossings = 0;
volatile int16_t lastSample = 0;
volatile uint32_t adcSampleRate = 125000UL;
volatile uint16_t samplesInWindow = 0;
volatile uint16_t freqAvailableCounter = 0;

#define SAMPLE_WINDOW 1024

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

void setupAudio() {
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
