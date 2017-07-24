#include <Pulse400.h>

int pin[] = { 4, 5, 6, 7 };

Esc400 motor[4];

void setup() {

  for ( int m = 0; m < 4; m++ ) {
    motor[m].begin( pin[m] );
  }
  
  delay( 1000 );

  for ( int m = 0; m < 4; m++ ) {
    motor[m].speed( 200 );
  }

}

void loop() {
}
