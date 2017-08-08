#include <Pulse400.h>

Multi400& Multi400::begin( int8_t pin0, int8_t pin1, int8_t pin2, int8_t pin3, int8_t pin4, int8_t pin5, int8_t pin6, int8_t pin7 ) {
  pulse400.attach( pin0, 0 );
  pulse400.attach( pin1, 1 );
  pulse400.attach( pin2, 2 );
  pulse400.attach( pin3, 3 );
  pulse400.attach( pin4, 4 );
  pulse400.attach( pin5, 5 );
  pulse400.attach( pin6, 6 );
  pulse400.attach( pin7, 7 );
  return *this;
}

Multi400& Multi400::set( int16_t v0, int16_t v1, int16_t v2, int16_t v3, int16_t v4, int16_t v5, int16_t v6, int16_t v7 ) {
  speed( 0, v0, true );
  speed( 1, v1, true );
  speed( 2, v2, true );
  speed( 3, v3, true );
  speed( 4, v4, true );
  speed( 5, v5, true );
  speed( 6, v6, true );
  speed( 7, v7, true );
  pulse400.update();
  if ( pulse_sync ) pulse400.sync();  
  return *this;
}

Multi400& Multi400::speed( uint8_t no, int16_t v, bool no_update ) {
  if ( v > -1 && !disabled ) {
    pulse400.pulse( no, map( constrain( v, 0, 1000 ), 0, 1000, min, max ), no_update );
  }
  return *this;
}

int16_t Multi400::speed( uint8_t no ) {
  int v = pulse400.pulse( no );
  return v == -1 ? -1 : map( v, min, max, 0, 1000 );
} 

Multi400& Multi400::off( void ) {
  set( 0, 0, 0, 0, 0, 0, 0, 0 );
  return *this;
}

Multi400& Multi400::enabled( bool v ) {
  off();
  disabled = !v;
  return *this;
}

Multi400& Multi400::autosync( bool v /* = true */ ) {
  pulse_sync = v;  
  return *this;
}

Multi400& Multi400::sync() {
  pulse400.sync();  
  return *this;
}

Multi400& Multi400::frequency( uint16_t f ) {
  pulse400.frequency( f );
  return *this;
}

Multi400& Multi400::outputRange( uint16_t min, uint16_t max, int16_t minPulse /* -1 */) {
  this->min = min;
  this->max = max;
  off();
  pulse400.minPulse( minPulse > -1 ? minPulse : min );
  return *this;
}

Multi400& Multi400::end( void ) {
  for ( int i = 0; i < MULTI400_NO_OF_CHANNELS; i++ ) 
    pulse400.detach( i );
  return *this;
}
