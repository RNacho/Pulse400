#include <Pulse400.h>

Pulse400 pulse400; // Global object

Pulse400 * Pulse400::instance; // Only one instance allowed (singleton)

int8_t Pulse400::attach( int8_t pin, uint16_t frequency ) {
  int id_channel = channel_find( pin ); 
  if ( id_channel > -1 ) {
    pinMode( pin, OUTPUT );
    digitalWrite( pin, LOW );
    int count = channel_count();
    channel[id_channel].pin = pin;
    channel[id_channel].pulse_width = PULSE400_DEFAULT_PULSE;
    set_frequency( id_channel, frequency, true );
#ifdef __AVR_ATmega328P__
    channel[id_channel].port = pin < 8 ? &PORTD : ( pin < 14 ? &PORTB : &PORTC ); // obsolete?
    channel[id_channel].port_mask = pin < 8 ? bit( pin ) : ( pin < 14 ? bit( pin - 8 ) : bit( pin - 14 ) );
#endif    
    cli();
    if ( switch_queue ) { // Queue switch in progress, abort while ints are off
      switch_queue = false;
      sei();
      update_queue( active_queue ^ 1 );
    } else {
      sei();
      update_queue( active_queue ^ 1 );      
    }
    switch_queue = true;
    if ( count == 0 ) { // Start the timer as soon as the first channel is created
      timer_start(); 
    }
  }
  return id_channel;
}

Pulse400& Pulse400::detach( int8_t id_channel ) {
  if ( id_channel != -1 ) {
    channel[id_channel].pin = -1;
    if ( channel_count() == 0 ) {
      timer_stop();
    } else {
      cli();
      if ( switch_queue ) { // Queue switch in progress, abort while ints are off
        switch_queue = false;
        sei();
        update_queue( active_queue ^ 1 );
      } else {
        sei();
        update_queue( active_queue ^ 1 );
      }
      switch_queue = true;
    }
  }
  return *this;
}

Pulse400& Pulse400::set_pulse( int8_t id_channel, uint16_t pulse_width, bool buffer_mode ) {
  if ( id_channel != -1 ) {
    pulse_width = constrain( pulse_width, 1, period_width - 1 );
    if ( channel[id_channel].pulse_width != pulse_width ) {
      channel[id_channel].pulse_width = pulse_width;
      if ( buffer_mode ) {
        buffer_cnt++;
      } else {
        if ( buffer_cnt ) { // Multiple updates pending
          switch_queue = false;
          update_queue( active_queue ^ 1 ); // Rebuild the whole queue         
        } else { // Single update pending
          cli(); // Disable interrupts while aborting the pending queue switch
          if ( switch_queue ) { // Queue switch in progress, abort while ints are off        
            switch_queue = false;
            sei(); // Re-enable interrupts
            // And update the non-active queue using itself as a source
            update_queue( active_queue ^ 1, active_queue ^ 1, id_channel, pulse_width );  
          } else {
            sei(); // Re-enable interrupts
            // Update the non-active queue using the active queue as a source
            update_queue( active_queue, active_queue ^ 1, id_channel, pulse_width );        
          }
        }
        switch_queue = true; // Set the switch_queue flag (again)
        buffer_cnt = 0;
      }
    }
  }
  return *this;
}

uint8_t Pulse400::freq2mask( uint16_t frequency ) {
  uint8_t mask = PULSE400_400HZ;
  if ( frequency < 50 ) mask = PULSE400_0HZ;
    else if ( frequency < 100 ) mask = PULSE400_50HZ;
    else if ( frequency < 200 ) mask = PULSE400_100HZ;
    else if ( frequency < 400 ) mask = PULSE400_200HZ;
  return mask;  
}

Pulse400& Pulse400::set_frequency( int8_t id_channel, uint8_t mask, bool buffer_mode /* = false */ ) {
  channel[id_channel].period_mask = mask;
#ifdef __AVR_ATmega328P__
  if ( !buffer_mode ) {    
    switch_queue = false;
    init_reg_bitmaps( active_queue ^ 1 );
    switch_queue = true;
  }
#endif
  return *this;
}

Pulse400& Pulse400::set_period( uint16_t period_width ) {
  this->period_width = period_width;
  return *this;
}

int cmp_by_pulse_width( const void *a, const void *b ) { 
    struct queue_struct_t *ia = (struct queue_struct_t *)a;
    struct queue_struct_t *ib = (struct queue_struct_t *)b;
    return ia->pulse_width - ib->pulse_width; 
}

#ifdef __AVR_ATmega328P__        

void Pulse400::init_reg_bitmaps( int8_t id_queue ) { 
    int q = 0;
    for ( int c = 0; c < PULSE400_NO_OF_PERIODS; c++ ) period_bitmap[c].lmask = 0L;
    while ( queue[id_queue][q].id_channel != PULSE400_END_FLAG ) {
      uint8_t id_channel = queue[id_queue][q].id_channel;
      for ( int c = 0; c < PULSE400_NO_OF_PERIODS; c++ ) {
        if ( ( channel[id_channel].period_mask & ( 1 << c ) ) > 0 )   
          period_bitmap[c].lmask |= ( 1L << channel[id_channel].pin );
      }
      q++;
    }            
} 
#endif

// Update/refresh the entire queue 

void Pulse400::update_queue( int8_t id_queue ) {
  int queue_cnt = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin > -1 ) {
      queue[id_queue][queue_cnt].id_channel = ch;
      queue[id_queue][queue_cnt].pulse_width = channel[ch].pulse_width;
#ifdef __AVR_ATmega328P__        
      queue[id_queue][queue_cnt].lmask = ( 1L << channel[ch].pin );
#endif
      queue_cnt++;
    }
  }
  queue[id_queue][queue_cnt].id_channel = PULSE400_END_FLAG; // Sentinel value
  queue[id_queue][queue_cnt].pulse_width = period_width; 
  qsort( queue[id_queue], queue_cnt, sizeof( struct queue_struct_t ), cmp_by_pulse_width );
#ifdef __AVR_ATmega328P__
  init_reg_bitmaps( id_queue );
#endif    
}

// Update one entry in the queue

void Pulse400::update_queue( int8_t id_queue_src, int8_t id_queue_dst, int8_t id_channel, uint16_t pulse_width ) {
  int loc = 0; 
  int cnt = 0;
  queue_struct_t tmp;
  channel[id_channel].pulse_width = pulse_width;
  // Copy the ALT queue from the ACT queue and determine length & item location
  while ( queue[id_queue_src][cnt].id_channel != PULSE400_END_FLAG ) {
    if ( queue[id_queue_src][cnt].id_channel == id_channel ) loc = cnt;
    queue[id_queue_dst][cnt] = queue[id_queue_src][cnt]; 
    cnt++;   
  }
  queue[id_queue_dst][cnt] = queue[id_queue_src][cnt]; // Copy sentinel
  queue[id_queue_dst][loc].pulse_width = pulse_width; // Set the new pulse width
  // Must maintain sort orders
  while ( loc > 0 && pulse_width < queue[id_queue_dst][loc - 1].pulse_width ) {
    tmp = queue[id_queue_dst][loc]; // Swap with previous entry
    queue[id_queue_dst][loc] = queue[id_queue_dst][loc - 1];
    queue[id_queue_dst][loc - 1] = tmp;    
    loc--;
  }
  while ( loc < cnt && pulse_width > queue[id_queue_dst][loc + 1].pulse_width ) {
    tmp = queue[id_queue_dst][loc]; // Swap with next entry
    queue[id_queue_dst][loc] = queue[id_queue_dst][loc + 1];
    queue[id_queue_dst][loc + 1] = tmp;    
    loc++;
  }
#ifdef __AVR_ATmega328P__
  init_reg_bitmaps( id_queue_dst );
#endif      
}

int Pulse400::channel_count( void ) {
  int result = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin > -1 ) {
      result++;
    }
  }
  return result;
}

// channel_find( pin ) - returns the first matching channel or the last free one or -1

int Pulse400::channel_find( int pin ) { 
  int result = -1;
  int last_free = -1;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin == pin ) {
      result = ch;
    }
    if ( channel[ch].pin == -1 ) {
      last_free = ch;
    }
  }
  return result == -1 ? last_free : result;
}

void ESC400PWM_ISR( void ) {
  Pulse400::instance->handleInterruptTimer();
}

Pulse400& Pulse400::sync() { // There's a 9 ms delay on this! Can't get it to work for now...
  cli();
  qptr = NULL;
  period_counter = 7;
  sei();
#ifdef PULSE400_USE_INTERVALTIMER  
  esc_timer.begin( ESC400PWM_ISR, 1 );
#else 
  Timer1.initialize( 0 ); 
  Timer1.restart(); 
  Timer1.setPeriod( 0 ); 
#endif
  return *this;
}

void Pulse400::timer_start( void ) {
  qptr = NULL;
  instance = this;
  period_counter = -1;
#ifdef PULSE400_USE_INTERVALTIMER  
  esc_timer.begin( ESC400PWM_ISR, 1 );
#else 
  Timer1.initialize( 1 ); 
  Timer1.attachInterrupt( ESC400PWM_ISR );
#endif    
}

void Pulse400::timer_stop( void ) {
#ifdef PULSE400_USE_INTERVALTIMER  
  esc_timer.end();
#else 
  Timer1.detachInterrupt();
#endif    
}

#ifdef __AVR_ATmega328P__

void Pulse400::handleInterruptTimer( void ) {
  int16_t next_interval = 0;   
  pin_bitmap_struct_t bitmap;
  if ( qptr == NULL || qptr->id_channel == PULSE400_END_FLAG ) {  // Start of period: set all pins HIGH
    if ( switch_queue ) {
      switch_queue = false;
      active_queue = active_queue ^ 1;
    }
    qptr = queue[active_queue]; // Point the queue pointer at the start of the active queue
    bitmap.lmask = period_bitmap[++period_counter & B00000111].lmask; 
    PORTD |= bitmap.dmask;
    PORTB |= bitmap.bmask;
    PORTC |= bitmap.cmask;
    next_interval = qptr->pulse_width;
  } else { // Set one or more pins LOW again in turn
    uint16_t previous_pulse_width = qptr->pulse_width;
    while ( !next_interval ) { // Process equal pulse widths in the same timer interrupt period
      bitmap.lmask = qptr->lmask;
      PORTD &= ~bitmap.dmask;
      PORTB &= ~bitmap.bmask;
      PORTC &= ~bitmap.cmask;
      qptr++;
      next_interval = qptr->pulse_width - previous_pulse_width;
    }
  }  
  Timer1.setPeriod( next_interval ); 
}

#else 

void Pulse400::handleInterruptTimer( void ) {
  int16_t next_interval = 0;   
  if ( qptr == NULL || qptr->id_channel == PULSE400_END_FLAG ) { 
    if ( switch_queue ) {
      switch_queue = false;
      active_queue = active_queue ^ 1;
    }
    qptr = queue[active_queue]; // Point the queue pointer at the start of the queue
    period_counter = ++period_counter & B00000111;
    for ( uint8_t i = 0; ; i++ ) {
      if ( qptr->id_channel == PULSE400_END_FLAG ) break;
      channel_struct_t *c = &channel[qptr->id_channel];
      if ( ( 1 << ( period_counter & B0000111 ) ) & c->period_mask ) {
        digitalWrite( c->pin, HIGH );   // This costs 7.43 us per pin!
      }
      qptr++;
    }
    qptr = queue[active_queue];
    next_interval = qptr->pulse_width;
  } else {
    uint16_t previous_pulse_width = qptr->pulse_width;
    while ( !next_interval ) { // Process equal pulse widths in the same timer interrupt period
      channel_struct_t *c = &channel[qptr->id_channel];
      digitalWrite( c->pin, LOW );
      qptr++;
      next_interval = qptr->pulse_width - previous_pulse_width;
    }
  }
  
#ifdef PULSE400_USE_INTERVALTIMER  
  esc_timer.begin( ESC400PWM_ISR, next_interval );
#else 
  Timer1.setPeriod( next_interval ); 
#endif
}

#endif
