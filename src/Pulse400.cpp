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

int16_t Pulse400::get_pulse( int8_t id_channel ) {
  return id_channel > -1 ? channel[id_channel].pulse_width : -1;
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
  period_mask = mask;
  return *this;
}

Pulse400& Pulse400::set_period( uint16_t period_width ) {
  this->period_width = period_width;
  return *this;
}

void Pulse400::bubble_sort_on_pulse_width( uint8_t list[], uint8_t size ) {
    int temp;
    for ( uint8_t i = 0; i < size; i++ ) {
        for ( uint8_t j = size - 1; j > i; j-- ) {
            if ( channel[list[j]].pulse_width < channel[list[ j - 1 ]].pulse_width ) {
                temp = list[ j - 1 ];
                list[ j - 1 ]=list[ j ];
                list[ j ]=temp;
            }
        }
    }
}

// Update/refresh the entire queue 

void Pulse400::update_queue( int8_t id_queue ) {
  int queue_cnt = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin > -1 ) {
      queue[id_queue][queue_cnt] = ch;
      queue_cnt++;
    }
  }
  queue[id_queue][queue_cnt] = PULSE400_END_FLAG; // Sentinel value
  
  // FIXME FIXME FIXME
  bubble_sort_on_pulse_width( queue[id_queue], queue_cnt );
}

// Update one entry in the queue

void Pulse400::update_queue( int8_t id_queue_src, int8_t id_queue_dst, int8_t id_channel, uint16_t pulse_width ) {
  int loc = 0; 
  int cnt = 0;
  uint8_t tmp;
  channel[id_channel].pulse_width = pulse_width;
  // Copy the ALT queue from the ACT queue and determine length & item location
  while ( queue[id_queue_src][cnt] != PULSE400_END_FLAG ) {
    if ( queue[id_queue_src][cnt] == id_channel ) loc = cnt;
    queue[id_queue_dst][cnt] = queue[id_queue_src][cnt]; 
    cnt++;   
  }
  queue[id_queue_dst][cnt] = queue[id_queue_src][cnt]; // Copy sentinel
  // Must maintain sort orders
  while ( loc > 0 && pulse_width < channel[queue[id_queue_dst][loc - 1]].pulse_width ) {
    tmp = queue[id_queue_dst][loc]; // Swap with previous entry
    queue[id_queue_dst][loc] = queue[id_queue_dst][loc - 1];
    queue[id_queue_dst][loc - 1] = tmp;    
    loc--;
  }
  while ( loc < cnt && pulse_width > channel[queue[id_queue_dst][loc + 1]].pulse_width ) {
    tmp = queue[id_queue_dst][loc]; // Swap with next entry
    queue[id_queue_dst][loc] = queue[id_queue_dst][loc + 1];
    queue[id_queue_dst][loc + 1] = tmp;    
    loc++;
  }
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

void Pulse400::handleInterruptTimer( void ) {
  int16_t next_interval = 0;   
  if ( qptr == NULL || *qptr == PULSE400_END_FLAG ) { 
    if ( switch_queue ) {
      switch_queue = false;
      active_queue = active_queue ^ 1;
    }
    qptr = queue[active_queue]; // Point the queue pointer at the start of the queue
    period_counter = ++period_counter & B00000111;
    while( *qptr != PULSE400_END_FLAG ) {
      channel_struct_t *c = &channel[*qptr];
      if ( ( 1 << ( period_counter & B0000111 ) ) & period_mask ) {
        digitalWrite( c->pin, HIGH );   
      }
      qptr++;
    }
    qptr = queue[active_queue];
    next_interval = channel[*qptr].pulse_width;
  } else {
    uint16_t previous_pulse_width = channel[*qptr].pulse_width;
    while ( !next_interval ) { // Process equal pulse widths in the same timer interrupt period
      digitalWrite( channel[*qptr].pin, LOW );
      next_interval = channel[*( ++qptr )].pulse_width - previous_pulse_width;
    }
    if ( *qptr == PULSE400_END_FLAG ) 
      next_interval = period_width - previous_pulse_width;
  }  
#ifdef PULSE400_USE_INTERVALTIMER  
  esc_timer.begin( ESC400PWM_ISR, next_interval );
#else 
  Timer1.setPeriod( next_interval ); 
#endif
}

