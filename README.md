Pulse400 - High speed 400 Hz Arduino ESC & Servo library. 

Generates 400 Hz PWM control pulses for servos and multicopter electronic speed controllers (ESC). By varying the pulse and period width frequencies of more than 1 Khz can be obtained if the receiving hardware can handle the signals. Can control 12 ESC's and/or Servo's at the same time at different frequencies.

Uses the Arduino TimerOne library.

Frontends:

- Esc400: Control motor (esc) as a single object
- Multi400: Control multiple motors (bank) as a single object
- Servo400: Drop in replacement for the Arduino Servo library

Not fully functional yet.

~~~c++
#include <Pulse400.h>

Multi400 esc;

setup() {

  esc.begin( 4, 5, 6, 7 );
  delay( 1000 );
  esc.setSpeed( 300, 300, 300, 300 );
  delay( 1000 );
  esc.setSpeed( 500, 500, 500, 500 );
  delay( 1000 );
  esc.setSpeed( 0, 0, 0, 0 );
  delay( 1000 );
  esc.end();
  
}

void loop() {

}


~~~