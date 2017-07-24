#include <Pulse400.h>

Multi400& Multi400::begin( int8_t pin0, int8_t pin1, int8_t pin2, int8_t pin3, int8_t pin4, int8_t pin5, int8_t pin6, int8_t pin7, uint16_t frequency ) {
  if ( pin0 > -1 ) id_channel[0] = pulse400.attach( pin0, pulse400.freq2mask( frequency ) );
  if ( pin1 > -1 ) id_channel[1] = pulse400.attach( pin1, pulse400.freq2mask( frequency ) );
  if ( pin2 > -1 ) id_channel[2] = pulse400.attach( pin2, pulse400.freq2mask( frequency ) );
  if ( pin3 > -1 ) id_channel[3] = pulse400.attach( pin3, pulse400.freq2mask( frequency ) );
  if ( pin4 > -1 ) id_channel[4] = pulse400.attach( pin4, pulse400.freq2mask( frequency ) );
  if ( pin5 > -1 ) id_channel[5] = pulse400.attach( pin5, pulse400.freq2mask( frequency ) );
  if ( pin6 > -1 ) id_channel[6] = pulse400.attach( pin6, pulse400.freq2mask( frequency ) );
  if ( pin7 > -1 ) id_channel[7] = pulse400.attach( pin7, pulse400.freq2mask( frequency ) );
  for ( int i = 0; i < MULTI400_NO_OF_CHANNELS; i++ ) 
    if ( id_channel[i] > -1 && i > last_esc ) last_esc = i;
  return *this;
}

Multi400& Multi400::frequency( uint16_t frequency ) {
  for ( int i = 0; i < MULTI400_NO_OF_CHANNELS; i++ ) 
    if ( id_channel[i] > -1 ) 
      pulse400.set_frequency( id_channel[i], pulse400.freq2mask( frequency ) );
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
    current_speed[no] = constrain( v, 0, 1000 );
    pulse400.set_pulse( id_channel[no], map( current_speed[no], 0, 1000, min, max ), buffer_mode );
  }
  return *this;
}

int16_t Multi400::speed( uint8_t no ) {
  return id_channel[no] == -1 ? -1 : current_speed[no];
}

Multi400& Multi400::range( uint16_t min, uint16_t max, uint16_t period /* = PULSE400_PERIOD_WIDTH */ ) {
  this->min = min;
  this->max = max;
  pulse400.set_period( period );
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
