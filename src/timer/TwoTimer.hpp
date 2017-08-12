#pragma once

// Simplified TimerOne/TimerTwo alternative that uses Timer1 and Timer2 to 
// switch quickly between short and long delays
//
// Timer1 on its own can't switch quickly from a 1000 usec delay to a 10 usec delay. By combining
// Timer1 for long ( > 127 usec) delays with Timer2 for short ( <= 127 usec) delays that becomes possible
//
// This Timer class is limited to delays between 1 and 4096 microseconds

#include <Arduino.h>

class TwoTimer;

extern TwoTimer twotimer;

class TwoTimer {
 public:
  void initialize( void );
  void setPeriod( uint16_t microseconds );  
  void attachInterrupt( void (*isr)(), uint16_t microseconds );
  void detachInterrupt();
  void start( void );
  void restart( void );
  void stop( void );  
  static void (*isr)();
  static void dummy();
};
