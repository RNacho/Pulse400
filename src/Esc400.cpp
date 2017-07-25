#include <Pulse400.h>

Esc400& Esc400::begin( int8_t pin, uint16_t frequency /* = 400 */ ) {
  id_channel = pulse400.attach( pin, pulse400.freq2mask( frequency ) );
  return *this;
}

Esc400& Esc400::frequency( uint16_t frequency ) {
  pulse400.set_frequency( id_channel, pulse400.freq2mask( frequency ) );
  return *this;
}

Esc400& Esc400::speed( uint16_t v ) {
  pulse400.set_pulse( id_channel, map( constrain( v, 0, 1000 ), 0, 1000, min, max ) );
  return *this;
}

int16_t Esc400::speed() {
  return id_channel == -1 ? -1 : map( pulse400.get_pulse( id_channel ), min, max, 1, 1000 );
}

Esc400& Esc400::range( uint16_t min, uint16_t max, uint16_t period ) {
  this->min = min;
  this->max = max;
  pulse400.set_period( period );
  return *this;
}

Esc400& Esc400::end( void ) {
  pulse400.detach( id_channel );
  id_channel = -1;
  return *this;
}
