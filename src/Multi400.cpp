#include <Pulse400.h>

Multi400& Multi400::begin( int8_t pin0, int8_t pin1, int8_t pin2, int8_t pin3, int8_t pin4, int8_t pin5, int8_t pin6, int8_t pin7 ) {
  id_channel[0] = pulse400.attach( pin0 );
  id_channel[1] = pulse400.attach( pin1 );
  id_channel[2] = pulse400.attach( pin2 );
  id_channel[3] = pulse400.attach( pin3 );
  id_channel[4] = pulse400.attach( pin4 );
  id_channel[5] = pulse400.attach( pin5 );
  id_channel[6] = pulse400.attach( pin6 );
  id_channel[7] = pulse400.attach( pin7 );
  return *this;
}

Multi400& Multi400::setSpeed( int16_t v0, int16_t v1, int16_t v2, int16_t v3, int16_t v4, int16_t v5, int16_t v6, int16_t v7 ) {
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
  if ( id_channel[no] > -1 && v > -1 ) {
    pulse400.pulse( id_channel[no], map( constrain( v, 0, 1000 ), 0, 1000, min, max ), no_update );
  }
  return *this;
}

int16_t Multi400::speed( uint8_t no ) {
  return id_channel[no] == -1 ? -1 : map( pulse400.pulse( id_channel[no] ), min, max, 1, 1000 );
} 

Multi400& Multi400::off( void ) {
  for ( int i = 0; i < MULTI400_NO_OF_CHANNELS; i++ ) 
    if ( id_channel[i] > -1 )
      speed( id_channel[i], 0, true );
  pulse400.update();  
  return *this;
}

Multi400& Multi400::autosync( bool v /* = true */ ) {
  pulse_sync = v;  
  return *this;
}

Multi400& Multi400::frequency( uint16_t f ) {
  pulse400.frequency( f );
  return *this;
}

Multi400& Multi400::range( uint16_t min, uint16_t max ) {
  this->min = min;
  this->max = max;
  return *this;
}

Multi400& Multi400::end( void ) {
  for ( int i = 0; i < MULTI400_NO_OF_CHANNELS; i++ ) 
    if ( id_channel[i] > -1 ) {
      pulse400.detach( id_channel[i] );
      id_channel[i] = -1;
    }
  return *this;
}
