#include <Pulse400.h>

#if defined( __TEENSY_3X__ )  && defined( PULSE400_OPTIMIZE_TEENSY_3X )
static struct { uint8_t port; uint8_t bit; } teensy_pins[] = { // A/0, B/1, C/2, D/3 ports: LC port 3 & 4 differ
  /* 0  */ 1, 16, // Pin 0 & 1 won't work (would require 32 bit masks)
  /* 1  */ 1, 17,
  /* 2  */ 3,  0,
#ifdef __TEENSY_LC__ 
  /* 3  */ 0,  1, 
  /* 4  */ 0,  2, 
#else   
  /* 3  */ 0, 12, 
  /* 4  */ 0, 13, 
#endif  
  /* 5  */ 3,  7,
  /* 6  */ 3,  4,
  /* 7  */ 3,  2,
  /* 8  */ 3,  3,
  /* 9  */ 2,  3,
  /* 10 */ 2,  4,
  /* 11 */ 2,  6,
  /* 12 */ 2,  7,
  /* 13 */ 2,  5,
  /* 14 */ 3,  1,
  /* 15 */ 2,  0,
  /* 16 */ 1,  0,
  /* 17 */ 1,  1,
  /* 18 */ 1,  3,
  /* 19 */ 1,  2,
  /* 20 */ 3,  5,
  /* 21 */ 3,  6,
  /* 22 */ 2,  1,
  /* 23 */ 2,  2,
};


void Pulse400::init_optimization( queue_struct_t queue[], int8_t queue_cnt ) {
  // Create a single set of bitmaps for turning pins on
  pins_high[3] = pins_high[2] = pins_high[1] = pins_high[0] = 0; 
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
      pins_high[teensy_pins[channel[ch].pin].port] |= 1 << teensy_pins[channel[ch].pin].bit;
    }
  }  
  uint16_t bits[4];
  int16_t skip_cnt = 1;
  int16_t last_pw = -1;
  queue_cnt--; // Counter points to PULSE400_END_FLAG entry, decrement for the first entry
  while ( queue_cnt >= 0 ) { // Iterate for end to beginning
    // Create pin bitmaps for every step of the queue for turning pins off again
    // Merge bitmaps of entries with the same pulse width and increment the skip counter
    if ( queue[queue_cnt].pw == last_pw ) {
      skip_cnt++;
    } else {
      bits[REG_A] = bits[REG_B] = bits[REG_C] = bits[REG_D] = 0;
      skip_cnt = 1;
    }
    queue[queue_cnt].cnt = skip_cnt;
    bits[teensy_pins[channel[queue[queue_cnt].id].pin].port] |= 
        1 << teensy_pins[channel[queue[queue_cnt].id].pin].bit; 
    queue[queue_cnt].pins_low[REG_A] = bits[REG_A];
    queue[queue_cnt].pins_low[REG_B] = bits[REG_B];
    queue[queue_cnt].pins_low[REG_C] = bits[REG_C];
    queue[queue_cnt].pins_low[REG_D] = bits[REG_D];
    last_pw = queue[queue_cnt].pw;
    queue_cnt--;
  } 
}

// ISR optimized for Teensy 3.x/LC

// Teensy 3.1: ISR 0.5% duty cycle @8ch, set speed: 44 us
// Teensy 3.1: ISR 0.42% duty cycle @8ch, set speed: 44 us (FASTRUN/PRIO 0, 0.44% error)
// Teensy LC : ISR 1% duty cycle @8ch, set speed: 88 us

FASTRUN void Pulse400::handleTimerInterrupt( void ) { 
  int16_t next_interval = 0;
  queue_t * q = &queue[qctl.active];
  if ( qctl.ptr == PULSE400_START_FLAG ) { 
    if ( qctl.change ) {
      qctl.change = false;
      qctl.active = qctl.active ^ 1;
      q = &queue[qctl.active];
    }
    qctl.ptr = 0;
    GPIOA_PSOR = pins_high[REG_A];  
    GPIOB_PSOR = pins_high[REG_B];
    GPIOC_PSOR = pins_high[REG_C];  
    GPIOD_PSOR = pins_high[REG_D]; 
    next_interval = (*q)[0].pw + PULSE400_MIN_PULSE;
  } else {    
    uint16_t previous_pw = (*q)[qctl.ptr].pw;
    GPIOA_PCOR = (*q)[qctl.ptr].pins_low[REG_A];  
    GPIOB_PCOR = (*q)[qctl.ptr].pins_low[REG_B];
    GPIOC_PCOR = (*q)[qctl.ptr].pins_low[REG_C];  
    GPIOD_PCOR = (*q)[qctl.ptr].pins_low[REG_D];  
    qctl.ptr += (*q)[qctl.ptr].cnt;
    next_interval = (*q)[qctl.ptr].pw - previous_pw;
    if ( (*q)[qctl.ptr].id == PULSE400_END_FLAG ) { 
      next_interval = period_width - previous_pw;
      qctl.ptr = PULSE400_START_FLAG; 
    }
  } 
  SET_TIMER( next_interval, PULSE400_ISR );
}

#endif
