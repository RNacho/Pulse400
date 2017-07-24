#include <Pulse400.h>

int pin[] = { 4, 5, 6, 7 };

Multi400 motors;

void setup() {

  motors.begin( pin[0], pin[1], pin[2], pin[3] );
  
  delay( 1000 );

  motors.setSpeed( 200, 200, 200, 200 );

}

void loop() {
}
