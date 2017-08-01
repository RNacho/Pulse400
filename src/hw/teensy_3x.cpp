#include <Pulse400.h>

#define PULSE400_MINIMUM_INTERVAL 4

#if defined( __TEENSY_3X__ )  && defined( PULSE400_OPTIMIZE_TEENSY_3X )
static struct { uint8_t port; uint8_t bit; } teensy_pins[] = { 
// A=0, B=1, C=2, D=3, E=4 ports: LC port 3 & 4 differ
  1, 16, // pin 0
  1, 17, // pin 1
  3,  0, // pin 2
#ifdef __TEENSY_LC__ 
  0,  1, // pin 3
  0,  2, // pin 4
#else   
  0, 12, // pin 3
  0, 13, // pin 4
#endif  
  3,  7, // pin 5
  3,  4, // pin 6
  3,  2, // pin 7
  3,  3, // pin 8
  2,  3, // pin 9
  2,  4, // pin 10
  2,  6, // pin 11
  2,  7, // pin 12
  2,  5, // pin 13
  3,  1, // pin 14
  2,  0, // pin 15
  1,  0, // pin 16
  1,  1, // pin 17
  1,  3, // pin 18
  1,  2, // pin 19
  3,  5, // pin 20
  3,  6, // pin 21
  2,  1, // pin 22
  2,  2, // pin 23
  0,  5, // pin 24
  1, 19, // pin 25 (Reg B is 8 bits on LC: not supported on Teensy LC)
  4,  1, // pin 26 (Reg E is not supported)
  2,  9, // pin 27 (Reg C is 8 bits: not supported)
  2,  8, // pin 28 (Reg C is 8 bits: not supported)
  2, 10, // pin 29 (Reg C is 8 bits: not supported)
  2, 11, // pin 30 (Reg C is 8 bits: not supported)
  4,  0, // pin 31 (Reg E: not supported)
  1, 18, // pin 32 
  0,  4, // pin 33
};

// Expand? Reg C 8 -> 16 bits, Reg B 8 -> 32 bits (LC), Reg E 8 bits: 5 bytes * (channels + 1)


void Pulse400::init_optimization( queue_struct_t queue[], int8_t queue_cnt ) {
  // Create a single set of bitmaps for turning pins on
  pins_high.PA = pins_high.PB = pins_high.PC = pins_high.PD = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
      switch ( teensy_pins[channel[ch].pin].port ) {
        case 0: pins_high.PA |= 1UL << teensy_pins[channel[ch].pin].bit; break;
        case 1: pins_high.PB |= 1UL << teensy_pins[channel[ch].pin].bit; break;
        case 2: pins_high.PC |= 1UL << teensy_pins[channel[ch].pin].bit; break;
        case 3: pins_high.PD |= 1UL << teensy_pins[channel[ch].pin].bit; break;
      }
    }
  }  
  reg_struct_t bits;
  int16_t skip_cnt = 1;
  uint16_t last_pw = 0xFFfF;
  queue_cnt--; // Counter points to PULSE400_END_FLAG entry, decrement for the first entry
  while ( queue_cnt >= 0 ) { // Iterate for end to beginning
    // Create pin bitmaps for every step of the queue for turning pins off again
    // Merge bitmaps of entries with (almost) the same pulse width and increment the skip counter
    if ( last_pw - queue[queue_cnt].pw <= PULSE400_MINIMUM_INTERVAL ) { 
      skip_cnt++;
    } else {
      bits.PA = bits.PB = bits.PC = bits.PD = 0;
      skip_cnt = 1;
    }
    queue[queue_cnt].cnt = skip_cnt;
    switch ( teensy_pins[channel[queue[queue_cnt].id].pin].port ) {
      case 0: bits.PA |= 1UL << teensy_pins[channel[queue[queue_cnt].id].pin].bit; break;
      case 1: bits.PB |= 1UL << teensy_pins[channel[queue[queue_cnt].id].pin].bit; break;
      case 2: bits.PC |= 1UL << teensy_pins[channel[queue[queue_cnt].id].pin].bit; break;
      case 3: bits.PD |= 1UL << teensy_pins[channel[queue[queue_cnt].id].pin].bit; break;
    }
    queue[queue_cnt].pins_low = bits;
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
  if ( qctl.next == PULSE400_JMP_HIGH ) { // Set all pins HIGH
    GPIOA_PSOR = pins_high.PA;  
    GPIOB_PSOR = pins_high.PB;
    GPIOC_PSOR = pins_high.PC;  
    GPIOD_PSOR = pins_high.PD;   
    qctl.next = PULSE400_JMP_DEADLINE;
    SET_TIMER( cycle_deadline, PULSE400_ISR );
    return;
  }  
  if ( qctl.next == PULSE400_JMP_DEADLINE ) { // Point of no return
    if ( qctl.change ) {
      qctl.change = false;
      qctl.active = qctl.active ^ 1;
    }
    qctl.next = 0;
    next_interval = ( queue[qctl.active][qctl.next].pw + PULSE400_MIN_PULSE ) - cycle_deadline;
  }
  if ( next_interval == 0 ) { // Pull the pins DOWN, a bunch at a time if needed
    queue_t * q = &queue[qctl.active];  
    uint16_t previous_pw = (*q)[qctl.next].pw;
    GPIOA_PCOR = (*q)[qctl.next].pins_low.PA;  
    GPIOB_PCOR = (*q)[qctl.next].pins_low.PB;
    GPIOC_PCOR = (*q)[qctl.next].pins_low.PC;  
    GPIOD_PCOR = (*q)[qctl.next].pins_low.PD;  
    qctl.next += (*q)[qctl.next].cnt;
    next_interval = (*q)[qctl.next].pw - previous_pw;
    if ( (*q)[qctl.next].id == PULSE400_END_FLAG ) { 
      next_interval = cycle_width - previous_pw;
      qctl.next = PULSE400_JMP_HIGH; 
    }
  } 
  SET_TIMER( next_interval, PULSE400_ISR );
}

#endif
