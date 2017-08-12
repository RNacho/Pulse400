#include <Pulse400.h>

Rc400 * Rc400::instance; // Only one instance allowed 

void Rc400::pwm( int8_t p0, int8_t p1, int8_t p2, int8_t p3, int8_t p4, int8_t p5  ) { 
  channel[0].pin = p0;
  channel[1].pin = p1;
  channel[2].pin = p2;
  channel[3].pin = p3;
  channel[4].pin = p4;
  channel[5].pin = p5;
  instance = this;
  last_interrupt = micros() - RC400_IDLE_DISCONNECT;
  for ( int ch = 0; ch < RC400_NO_OF_CHANNELS; ch++ ) {
    if ( channel[ch].pin > -1 ) {
      pinMode( channel[ch].pin, INPUT_PULLUP );
    }
  }
#ifdef __AVR_ATmega328P__
  for ( int ch = 0; ch < RC400_NO_OF_CHANNELS; ch++ ) {
    if ( channel[ch].pin > -1 ) set_channel( ch, channel[ch].pin );
  }
#else
  if ( channel[0].pin > -1 ) attachInterrupt( digitalPinToInterrupt( channel[0].pin ), []() { instance->handleInterruptPWM( 0 ); }, CHANGE );  
  if ( channel[1].pin > -1 ) attachInterrupt( digitalPinToInterrupt( channel[1].pin ), []() { instance->handleInterruptPWM( 1 ); }, CHANGE );  
  if ( channel[2].pin > -1 ) attachInterrupt( digitalPinToInterrupt( channel[2].pin ), []() { instance->handleInterruptPWM( 2 ); }, CHANGE );  
  if ( channel[3].pin > -1 ) attachInterrupt( digitalPinToInterrupt( channel[3].pin ), []() { instance->handleInterruptPWM( 3 ); }, CHANGE );  
  if ( channel[4].pin > -1 ) attachInterrupt( digitalPinToInterrupt( channel[4].pin ), []() { instance->handleInterruptPWM( 4 ); }, CHANGE );  
  if ( channel[5].pin > -1 ) attachInterrupt( digitalPinToInterrupt( channel[5].pin ), []() { instance->handleInterruptPWM( 5 ); }, CHANGE );
#endif  
}

void Rc400::ppm( int8_t p0 ) { // Pulse Position Modulation
  for ( int ch = 0; ch < RC400_NO_OF_CHANNELS; ch++ ) {
    channel[ch].pin = -1;
  }
  channel[0].pin = p0;
  instance = this;
  last_interrupt = micros() - RC400_IDLE_DISCONNECT;
  pinMode( channel[0].pin, INPUT_PULLUP );
#ifdef __AVR_ATmega328P__
  // Oops PPM is not yet implemented on UNO!
#else
  attachInterrupt( digitalPinToInterrupt( channel[0].pin ), []() { instance->handleInterruptPPM(); }, RISING );
#endif
}

int Rc400::read( int ch ) {
  return channel[ch].pin > -1 ? channel[ch].value : -1;
}

bool Rc400::connected() {
  cli();
  if ( micros() - last_interrupt >= RC400_IDLE_DISCONNECT ) {
    sei();
    return false;
  } else {
    sei();
    return true;
  }
}

void Rc400::end() {
#ifdef __TEENSY_3X__
  if ( channel[0].pin > -1 ) detachInterrupt( digitalPinToInterrupt( channel[1].pin ) );  
  if ( channel[1].pin > -1 ) detachInterrupt( digitalPinToInterrupt( channel[1].pin ) );  
  if ( channel[2].pin > -1 ) detachInterrupt( digitalPinToInterrupt( channel[2].pin ) );  
  if ( channel[3].pin > -1 ) detachInterrupt( digitalPinToInterrupt( channel[3].pin ) );  
  if ( channel[4].pin > -1 ) detachInterrupt( digitalPinToInterrupt( channel[4].pin ) );  
  if ( channel[5].pin > -1 ) detachInterrupt( digitalPinToInterrupt( channel[5].pin ) );    
#else 
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;
#endif
  for ( int ch = 0; ch < RC400_NO_OF_CHANNELS; ch++ ) {
    channel[ch].pin = -1;
  }
}

#ifndef __TEENSY_3X__

// Code for the Arduino UNO that has shared interrupts per pin register

// Set up the pin change interrupt for a channel/pin combo

void Rc400::set_channel( int ch, int pin ) { 
  byte int_no = pin < 8 ? 2 : ( pin < 14 ? 0 : 1 );
  pinMode( pin, INPUT_PULLUP );
  switch ( int_no ) {
    case 0: // PCINT0 PORTB D8 - D13 (pin 8 - 13 => 6 bits: 0 - 5)
      PCMSK0 |= bit( pin - 8 );
      PCIFR  |= bit( PCIF0 );
      PCICR  |= bit( PCIE0 );
      int_state[0].channel[pin - 8] = ch;
      break;
    case 1: // PCINT1 PORTC A0 - A5 (pin 14 - 19 => 6 bits: 0 - 5)
      PCMSK1 |= bit( pin - 14 );
      PCIFR  |= bit( PCIF1 );
      PCICR  |= bit( PCIE1 );
      int_state[1].channel[pin - 14] = ch;
      break;
    case 2: // PCINT2 PORTD D0 - D7 (pin 0 - 7 => 8 bits: 0 - 7) 
      PCMSK2 |= bit( pin );
      PCIFR  |= bit( PCIF2 );
      PCICR  |= bit( PCIE2 );
      int_state[2].channel[pin] = ch;
      break;
  }
}

// Process pin change interrupts and calculate pulse times in PWM mode 


void Rc400::register_pin_change_pwm( byte int_no, byte int_mask, byte bits ) { 
  byte diff, p;
  if ( ( diff = ( ~int_state[int_no].reg & bits ) & int_mask ) ) { // Pin(s) went high
    p = 0;
    while ( diff ) {
      if ( diff & 1 ) {
        channel[int_state[int_no].channel[p]].last_high = micros(); // 16 bit resolution
      }
      diff >>= 1;
      p++;
    }
  }
  if ( ( diff = ( int_state[int_no].reg & ~bits ) & int_mask ) ) { // Pin(s) went low
    p = 0;
    while ( diff ) {
      if ( diff & 1 ) {
        byte ch = int_state[int_no].channel[p];
        channel[ch].value = micros() - channel[ch].last_high; // 16 bit resolution
      }
      diff >>= 1;
      p++;
    }
  }
  int_state[int_no].reg = bits;  
  last_interrupt = micros();
}

// The Uno's micros() funtion has only a 4 us resolution
// This can be fixed if necessary with a timer interrupt

ISR (PCINT0_vect) { Rc400::instance->register_pin_change_pwm( 0, PCMSK0, PINB ); }
ISR (PCINT1_vect) { Rc400::instance->register_pin_change_pwm( 1, PCMSK1, PINC ); }
ISR (PCINT2_vect) { Rc400::instance->register_pin_change_pwm( 2, PCMSK2, PIND ); }

#else

// Code for Teensy 3.x/LC and any other uController that supports pin change interrupts for any pin  
  
void Rc400::handleInterruptPWM( int ch ) { // ch = physical channel no
  if ( digitalRead( channel[ch].pin ) ) {
    channel[ch].last_high = micros();    
  } else {
    channel[ch].value = micros() - channel[ch].last_high; 
  }
  last_interrupt = micros();
}

void Rc400::handleInterruptPPM() {
  uint32_t delta = micros() - ppm_last_pulse;
  if ( delta > 4000 ) { // Reset pulse_counter on long gap
    ppm_pulse_counter = 0;
  } else {
    if ( ppm_pulse_counter < RC400_NO_OF_CHANNELS ) {
      channel[ppm_pulse_counter].value = delta;
    }        
    ppm_pulse_counter++;
  }
  last_interrupt = ppm_last_pulse = micros();
}

#endif
