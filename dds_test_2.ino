/*
 Test code for a DDS based LFO
 generates noise at the same time
 */
#include <avr/pgmspace.h>
#include <avr/io.h> 
#include <avr/interrupt.h>
#define NOISE_PIN 8
uint32_t lfsr = 1; //32 bit LFSR, must be non-zero to start

// table of 256 sine values / one sine period / stored in flash memory
const char sine256[] PROGMEM = {
  127,130,133,136,139,143,146,149,152,155,158,161,164,167,170,173,176,178,181,184,187,190,192,195,198,200,203,205,208,210,212,215,217,219,221,223,225,227,229,231,233,234,236,238,239,240,
  242,243,244,245,247,248,249,249,250,251,252,252,253,253,253,254,254,254,254,254,254,254,253,253,253,252,252,251,250,249,249,248,247,245,244,243,242,240,239,238,236,234,233,231,229,227,225,223,
  221,219,217,215,212,210,208,205,203,200,198,195,192,190,187,184,181,178,176,173,170,167,164,161,158,155,152,149,146,143,139,136,133,130,127,124,121,118,115,111,108,105,102,99,96,93,90,87,84,81,78,
  76,73,70,67,64,62,59,56,54,51,49,46,44,42,39,37,35,33,31,29,27,25,23,21,20,18,16,15,14,12,11,10,9,7,6,5,5,4,3,2,2,1,1,1,0,0,0,0,0,0,0,1,1,1,2,2,3,4,5,5,6,7,9,10,11,12,14,15,16,18,20,21,23,25,27,29,31,
  33,35,37,39,42,44,46,49,51,54,56,59,62,64,67,70,73,76,78,81,84,87,90,93,96,99,102,105,108,111,115,118,121,124
};

// LFO stuff
uint32_t lfoPhaccu;   // phase accumulator
uint32_t lfoTword_m;  // dds tuning word m
uint8_t lfoCnt;      // var inside interrupt

float lfoControlVoltage;
float lfoFreq;
enum lfoWaveTypes {
  RAMP,
  SAW,
  TRI,
  SINE,
  SQR
};
lfoWaveTypes lfoWaveform;

void setup() {
  pinMode(11, OUTPUT); //PWM output
  pinMode(7, OUTPUT); // test pin
  pinMode(NOISE_PIN, OUTPUT);
  // timer 2 phase accurate PWM, no prescaling, non inverting mode channel A
  TCCR2A = _BV(COM2A1) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  // timer 2 interrupt
  TIMSK2 = _BV(TOIE2);
}

void getLfoFreq() {
  // read ADC to calculate the required DDS tuning word, log scale between 0.01Hz and 10Hz approx
  float lfoControlVoltage = analogRead(0) * 11/1024; //gives 11 octaves range 0.01Hz to 10Hz
  lfoTword_m = 1369 * pow(2.0, lfoControlVoltage); //1369 sets the lowest frequency to 0.01Hz
}

void getLfoWaveform() {
  // read ADC to get the LFO wave type
  int adcVal = analogRead(1);
  if (adcVal < 128) {
    lfoWaveform = TRI;
  } else if (adcVal < 384) {
    lfoWaveform = RAMP;
  } else if (adcVal < 640) {
    lfoWaveform = SAW;
  } else if (adcVal < 896) {
    lfoWaveform = SINE;
  } else {
    lfoWaveform = SQR;
  }
}

void loop() {
  getLfoFreq();
  getLfoWaveform();
}

SIGNAL(TIMER2_OVF_vect) {
  PORTD = 0x80; // to test timing
  // handle noise signal. Set or clear noise pin PB0 (digital pin 8)
  unsigned lsb = lfsr & 1;
  if (lsb) {
    PORTB |= 1;
  } else {
    PORTB &= ~1;
  }
  // advance LFSR
  lfsr >>= 1;
  if (lsb) {
    lfsr ^= 0xA3000000u;
  }
  // handle LFO DDS  
  lfoPhaccu += lfoTword_m; // increment phase accumulator
  lfoCnt=lfoPhaccu >> 24;  // use upper 8 bits for phase accu as frequency information
  switch (lfoWaveform) {
    case RAMP:
      OCR2A = lfoCnt;
      break;
    case SAW:
      OCR2A = 255 - lfoCnt;
      break;
    case TRI:
      if (lfoCnt & 0x80) {
        OCR2A = 254 - ((lfoCnt & 0x7F) << 1); //ramp down
      } else {
        OCR2A = lfoCnt << 1; //ramp up
      }
      break;
    case SINE:
      // sine wave from table
      OCR2A = pgm_read_byte_near(sine256 + lfoCnt);
      break;
    case SQR:
      if (lfoCnt & 0x80) {
        OCR2A = 255;
      } else {
        OCR2A = 0;
      }
      break;
    default:
      break;
  }
  PORTD = 0; // to test timing
}

