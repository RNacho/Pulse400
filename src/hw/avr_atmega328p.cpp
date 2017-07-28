#include <Pulse400.h>

#if defined( __AVR_ATmega328P__ ) && defined( PULSE400_OPTIMIZE_ARDUINO_UNO ) 

void Pulse400::init_optimization( queue_struct_t queue[], int8_t queue_cnt ) {
  pins_high[REG_B] = pins_high[REG_C] = pins_high[REG_D] = 0; 
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
      if ( channel[ch].pin < 8 )
        pins_high[REG_D] |= 1UL << ( channel[ch].pin );
      else if ( channel[ch].pin < 14 )      
        pins_high[REG_B] |= 1UL << ( channel[ch].pin - 8 );
      else 
        pins_high[REG_C] |= 1UL << ( channel[ch].pin - 14 );        
    }
  }
}

// ISR optimized for Arduino UNO (ATMega328P)
// Arduino: ISR 4.63% duty cycle @8ch, set speed: 840 us

void Pulse400::handleTimerInterrupt( void ) {
  int16_t next_interval = 0;
  queue_t * q = &queue[qctl.active];
  if ( qctl.ptr == PULSE400_END_FLAG ) { 
    if ( qctl.change ) {
      qctl.change = false;
      qctl.active = qctl.active ^ 1;
      q = &queue[qctl.active];
    }
    qctl.ptr = 0;
    PORTB |= pins_high[REG_B]; // Arduino UNO optimization: flip pins per bank
    PORTC |= pins_high[REG_C];  
    PORTD |= pins_high[REG_D];
    next_interval = (*q)[qctl.ptr].pw + PULSE400_MIN_PULSE;
  } else {    
    uint16_t previous_pw = (*q)[qctl.ptr].pw;
    while ( !next_interval ) { // Process equal pulse widths in the same timer interrupt period
      digitalWrite( channel[(*q)[qctl.ptr].id].pin, LOW );
      next_interval = (*q)[++qctl.ptr].pw - previous_pw;
    }
    if ( (*q)[qctl.ptr].id == PULSE400_END_FLAG ) {
      next_interval = period_width - previous_pw;
      qctl.ptr = PULSE400_START_FLAG; 
    }
  } 
  SET_TIMER( next_interval, PULSE400_ISR );
}

#endif
