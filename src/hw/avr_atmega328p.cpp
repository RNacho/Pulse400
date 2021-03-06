#include <Pulse400.h>

// Solutions for switching interval:
// - Continually switch from CTC mode 4 to CTC mode 12: tried, no improvement
// - Switch from Timer1 (long intervals) to Timer2 (short intervals)?
// - Use capture timer mode???

#if defined( __AVR_ATmega328P__ ) && defined( PULSE400_OPTIMIZE_ARDUINO_UNO ) 

void Pulse400::init_optimization( queue_struct_t queue[], int8_t queue_cnt ) {
  pins_high.PB = pins_high.PC = pins_high.PD = 0; 
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
      if ( channel[ch].pin < 8 )
        pins_high.PD |= 1UL << ( channel[ch].pin );
      else if ( channel[ch].pin < 14 )      
        pins_high.PB |= 1UL << ( channel[ch].pin - 8 );
      else 
        pins_high.PC |= 1UL << ( channel[ch].pin - 14 );        
    }
  }
}

// ISR optimized for Arduino UNO (ATMega328P)
// Arduino: ISR 4.63% duty cycle @8ch, set speed: 840 us

void Pulse400::handleTimerInterrupt( void ) {
  int16_t next_interval = 0;
  queue_t * q = &queue[qctl.active];
  if ( qctl.next == PULSE400_JMP_HIGH ) { // Set all pins HIGH
    PORTB |= pins_high.PB; // Arduino UNO optimization: flip pins per bank
    PORTC |= pins_high.PC;  
    PORTD |= pins_high.PD;
    qctl.next = PULSE400_JMP_DEADLINE;
    next_interval = cycle_deadline;
    SET_TIMER( cycle_deadline, PULSE400_ISR );
    return;
  } 
  if ( qctl.next == PULSE400_JMP_DEADLINE ) { 
    if ( qctl.change ) {
      qctl.change = false;
      qctl.active = qctl.active ^ 1;
    }
    qctl.next = 0;
    next_interval = ( queue[qctl.active][qctl.next].pw + PULSE400_MIN_PULSE ) - cycle_deadline;
  } 
  if ( next_interval == 0 ) {    
    uint16_t previous_pw = (*q)[qctl.next].pw;
    while ( !next_interval ) { // Process equal pulse widths in the same timer interrupt period
      digitalWrite( channel[(*q)[qctl.next].id].pin, LOW );
      next_interval = (*q)[++qctl.next].pw - previous_pw;
    }
    if ( (*q)[qctl.next].id == PULSE400_END_FLAG ) {
      next_interval = cycle_width - previous_pw;
      qctl.next = PULSE400_JMP_HIGH; 
    }
  } 
  SET_TIMER( next_interval, PULSE400_ISR );
}

#endif
