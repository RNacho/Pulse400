#include <Pulse400.h>

Esc400& Esc400::begin( int8_t pin ) {
  id_channel = pulse400.attach( pin );
  return *this;
}

Esc400& Esc400::speed( uint16_t v ) {
  pulse400.pulse( id_channel, map( constrain( v, 0, 1000 ), 0, 1000, min, max ) );
  return *this;
}

int16_t Esc400::speed() {
  return id_channel == -1 ? -1 : map( pulse400.pulse( id_channel ), min, max, 1, 1000 );
}

Esc400& Esc400::outputRange( uint16_t min, uint16_t max ) {
  this->min = min;
  this->max = max;
  return *this;
}

Esc400& Esc400::frequency( uint16_t f ) {
  pulse400.frequency( f );
  return *this;
}

Esc400& Esc400::end( void ) {
  pulse400.detach( id_channel );
  id_channel = -1;
  return *this;
}
