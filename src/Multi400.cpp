#include <Pulse400.h>

Multi400& Multi400::begin( int8_t pin0, int8_t pin1, int8_t pin2, int8_t pin3, int8_t pin4, int8_t pin5, int8_t pin6, int8_t pin7 ) {
  if ( pin0 > -1 ) id_channel[0] = pulse400.attach( pin0 );
  if ( pin1 > -1 ) id_channel[1] = pulse400.attach( pin1 );
  if ( pin2 > -1 ) id_channel[2] = pulse400.attach( pin2 );
  if ( pin3 > -1 ) id_channel[3] = pulse400.attach( pin3 );
  if ( pin4 > -1 ) id_channel[4] = pulse400.attach( pin4 );
  if ( pin5 > -1 ) id_channel[5] = pulse400.attach( pin5 );
  if ( pin6 > -1 ) id_channel[6] = pulse400.attach( pin6 );
  if ( pin7 > -1 ) id_channel[7] = pulse400.attach( pin7 );
  for ( int i = 0; i < MULTI400_NO_OF_CHANNELS; i++ ) 
    if ( id_channel[i] > -1 && i > last_esc ) last_esc = i;
  return *this;
}

Multi400& Multi400::setSpeed( int16_t v0, int16_t v1, int16_t v2, int16_t v3, int16_t v4, int16_t v5, int16_t v6, int16_t v7 ) {
  speed( 0, v0, last_esc != 0 );
  speed( 1, v1, last_esc != 1 );
  speed( 2, v2, last_esc != 2 );
  speed( 3, v3, last_esc != 3 );
  speed( 4, v4, last_esc != 4 );
  speed( 5, v5, last_esc != 5 );
  speed( 6, v6, last_esc != 6 );
  speed( 7, v7, last_esc != 7 );  
  return *this;
}

Multi400& Multi400::speed( uint8_t no, int16_t v, bool buffer_mode ) {
  if ( id_channel[no] > -1 && v > -1 ) {
    pulse400.set_pulse( id_channel[no], map( constrain( v, 0, 1000 ), 0, 1000, min, max ), buffer_mode );
  }
  return *this;
}

int16_t Multi400::speed( uint8_t no ) {
  return id_channel[no] == -1 ? -1 : map( pulse400.get_pulse( id_channel[no] ), min, max, 1, 1000 );
} 

Multi400& Multi400::off( void ) {
  for ( int i = 0; i < MULTI400_NO_OF_CHANNELS; i++ ) 
    if ( id_channel[i] > -1 )
      speed( id_channel[i], 0, last_esc != i );
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
