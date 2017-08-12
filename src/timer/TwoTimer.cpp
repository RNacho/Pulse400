#include "TwoTimer.hpp"

TwoTimer twotimer;

void (*TwoTimer::isr)() = TwoTimer::dummy;

void TwoTimer::initialize() {
  cli();
  // Timer 1
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 65535; // compare match
  TCCR1B |= (1 << WGM12) | (1 << CS10); // CTC mode
  // Timer 2
  TCCR2A = 0;// set entire TCCR2A register to 0
  TCCR2B = 0;// same for TCCR2B
  TCNT2  = 0; // counter 0
  OCR2A = 255;// Initial value
  TCCR2A |= (1 << WGM21); // CTC mode
  TCCR2B |= (1 << CS21); // prescaler 8  
  sei();  
}

void TwoTimer::setPeriod( uint16_t microseconds ) {
  cli();
  if ( microseconds > 127 ) {
    OCR1A = ( F_CPU / 1000000.0 ) * microseconds; 
    TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
    TIMSK2 = 0;
  } else {
    OCR2A = ( F_CPU / 8000000.0 ) * microseconds; 
    TIMSK2 |= (1 << OCIE2A);   // enable timer compare interrupt for timer2
    TIMSK1 = 0; // disable interrupt for timer1
  }
  sei();   
}

void TwoTimer::attachInterrupt( void (*isr)(), uint16_t microseconds ) {
  this->isr = isr;
  setPeriod( microseconds );
}

void TwoTimer::detachInterrupt() {
  TIMSK1 = 0;
  TIMSK2 = 0;
}

void TwoTimer::start( void ) {
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
  TIMSK2 = 0; // Disable timer2
  TCNT1 = 0; // Trigger interrupt on timer1
}

void TwoTimer::restart( void ) {
  start();
}

void TwoTimer::stop( void ) {
  TIMSK1 = 0;  
  TIMSK2 = 0;  
}  

void TwoTimer::dummy() { }

ISR (TIMER1_COMPA_vect) {
  twotimer.isr();
}

ISR (TIMER2_COMPA_vect) {
  twotimer.isr();
}
