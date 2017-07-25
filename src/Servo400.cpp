#include <Pulse400.h>

// attach the given pin to the next free channel channel number or 0 if failure

uint8_t Servo400::attach(int pin ) {
  id_channel = pulse400.attach( pin );
  return id_channel + 1;  
} 

// as above but also sets min and max values for writes. 
          
uint8_t Servo400::attach(int pin, int min, int max ) { 
  id_channel = pulse400.attach( pin );
  this->min = min;
  this->max = max;
  return id_channel + 1;  
} 

// frees the servo channel and releases the pin from Pulse400 control

void Servo400::detach() {
  pulse400.detach( id_channel );
}

// return true if this servo is attached, otherwise false

bool Servo400::attached() {
  return id_channel > -1;
}

// Set PWM frequency to 0, 50 (default), 100, 200 or 400 HZ

void Servo400::frequency( uint16_t frequency ) {
  pulse400.set_frequency( id_channel, pulse400.freq2mask( frequency ) );
}

// Write pulse width in microseconds 

void Servo400::writeMicroseconds(int value) {
  pulse400.set_pulse( id_channel, value );
}

// returns current pulse width in microseconds for this servo

int Servo400::readMicroseconds() {
  return id_channel == -1 ? -1 : map( pulse400.get_pulse( id_channel ), min, max, 1, 1000 );
}
          
// if value is < 200 it's treated as an angle, otherwise as pulse width in microseconds 

void Servo400::write(int value) {
    if ( value < 200 ) {
      writeMicroseconds( map( value, 0, 180, min, max ) ); 
    } else {
      writeMicroseconds( value );
    }
} 
            
// returns current pulse width as an angle between 0 and 180 degrees
            
int Servo400::read() {
  return map( readMicroseconds(), min, max, 0, 180 );
}                        
