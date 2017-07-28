#include <Pulse400.h>

Pulse400 pulse400; // Global object
Pulse400 * Pulse400::instance; // Only one instance allowed (singleton)

Pulse400::Pulse400( void ) {
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    channel[ch].pin = PULSE400_UNUSED;
    channel[ch].pw = PULSE400_DEFAULT_PULSE - PULSE400_MIN_PULSE;
  }  
  qctl.active = 0;  
  qctl.change = 0;  
}

int8_t Pulse400::attach( int8_t pin, int8_t force_id /* = -1 */ ) {
  if ( pin != PULSE400_UNUSED ) {
    int id_channel = force_id > -1 ? force_id : channel_find( pin ); 
    if ( id_channel != -1 ) {
      pinMode( pin, OUTPUT );
      digitalWrite( pin, LOW );
      int count = channel_count();
      channel[id_channel].pin = pin;
      channel[id_channel].pw = PULSE400_DEFAULT_PULSE - PULSE400_MIN_PULSE;
      update();
      if ( count == 0 ) { // Start the timer as soon as the first channel is created
        timer_start(); 
      }
    }
    return id_channel;
  }
  return -1;
}

Pulse400& Pulse400::detach( int8_t id_channel ) {
  if ( id_channel != PULSE400_UNUSED ) {
    channel[id_channel].pin = PULSE400_UNUSED;
    if ( channel_count() == 0 ) {
      timer_stop();
    } else {
      update();
    }
  }
  return *this;
}

Pulse400& Pulse400::set_pulse( int8_t id_channel, uint16_t pw, bool no_update ) {
  if ( id_channel != PULSE400_UNUSED && channel[id_channel].pin != PULSE400_UNUSED ) {
    pw = constrain( pw, 1, period_width + PULSE400_MIN_PULSE - 1 ) - PULSE400_MIN_PULSE;
    if ( channel[id_channel].pw != pw ) {
      channel[id_channel].pw = pw;
      if ( no_update ) {
        update_cnt++;
      } else {
        if ( update_cnt ) { // Multiple updates pending
          qctl.change = false;
          update(); // Rebuild the whole queue         
        } else { // Single update pending
#if defined( __TEENSY_3X__ )  && defined( PULSE400_OPTIMIZE_TEENSY_3X )
          qctl.change = false;
          update(); // Rebuild the whole queue including Teensy bitmaps        
#else        
          cli(); // Disable interrupts while aborting a possibly pending queue switch
          if ( qctl.change ) { // Queue switch in progress, abort while ints are off        
            qctl.change = false;
            sei(); // Re-enable interrupts
            // And update the non-active queue using itself as a source
            update_queue_entry( qctl.active ^ 1, qctl.active ^ 1, id_channel, pw );  
          } else {
            sei(); // Re-enable interrupts
            // Update the non-active queue using the active queue as a source
            update_queue_entry( qctl.active, qctl.active ^ 1, id_channel, pw );        
          }
#endif          
          qctl.change = true; // Set the qctl.change flag (again)
        }
        update_cnt = 0;
      }
    }
  }
  return *this;
}

int16_t Pulse400::get_pulse( int8_t id_channel ) {
  return id_channel != PULSE400_UNUSED ? channel[id_channel].pw + PULSE400_MIN_PULSE : -1;
}

Pulse400& Pulse400::frequency( uint16_t f ) {
  period_width = ( 1000000 / f ) - PULSE400_MIN_PULSE;
  return *this;
}

// Update/refresh the entire (ALT) queue and set the qctl.change flag 

Pulse400& Pulse400::update() {
  qctl.change = false;
  int queue_cnt = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
      queue[qctl.active ^ 1][queue_cnt].id = ch;
      queue[qctl.active ^ 1][queue_cnt].pw = channel[ch].pw;
      queue_cnt++;
    }
  }
  queue[qctl.active ^ 1][queue_cnt].id = PULSE400_END_FLAG; // Sentinel value
  sort_on_pulse_width( queue[qctl.active ^ 1], queue_cnt );
  init_optimization( queue[qctl.active ^ 1], queue_cnt );
  qctl.change = true;
  return *this;
}

#if defined( PULSE400_OPTIMIZE_STANDARD )

void Pulse400::init_optimization( queue_struct_t queue[], int8_t queue_cnt ) { 
}

#endif

// Update a single entry in the queue

void Pulse400::update_queue_entry( int8_t id_queue_src, int8_t id_queue_dst, int8_t id_channel, uint16_t pw ) {
  int loc = 0; 
  int cnt = 0;
  queue_struct_t tmp;
  channel[id_channel].pw = pw;
  // Copy the ALT queue from the ACT queue and determine length & item location
  while ( queue[id_queue_src][cnt].id != PULSE400_END_FLAG ) {
    if ( queue[id_queue_src][cnt].id == id_channel ) loc = cnt;
    queue[id_queue_dst][cnt] = queue[id_queue_src][cnt]; 
    cnt++;   
  }
  queue[id_queue_dst][cnt] = queue[id_queue_src][cnt]; // Copy sentinel
  queue[id_queue_dst][loc].pw = pw;
  // Must maintain sort orders
  while ( loc > 0 && pw < queue[id_queue_dst][loc - 1].pw ) {
    tmp = queue[id_queue_dst][loc]; // Swap with previous entry
    queue[id_queue_dst][loc] = queue[id_queue_dst][loc - 1];
    queue[id_queue_dst][loc - 1] = tmp;    
    loc--;
  }
  while ( loc < cnt && pw > queue[id_queue_dst][loc + 1].pw ) {
    tmp = queue[id_queue_dst][loc]; // Swap with next entry
    queue[id_queue_dst][loc] = queue[id_queue_dst][loc + 1];
    queue[id_queue_dst][loc + 1] = tmp;    
    loc++;
  }
}

void Pulse400::sort_on_pulse_width( queue_struct_t list[], uint8_t size ) { // Bubble sort ;-) (must benchmark!)
  queue_struct_t temp;
  for ( uint8_t i = 0; i < size; i++ ) {
    for ( uint8_t j = size - 1; j > i; j-- ) {
      if ( channel[list[j].id].pw < channel[list[ j - 1 ].id].pw ) {
        temp = list[ j - 1 ];
        list[ j - 1 ]=list[ j ];
        list[ j ]=temp;
      }
    }
  }
}

int Pulse400::channel_count( void ) {
  int result = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
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
    if ( channel[ch].pin == PULSE400_UNUSED ) {
      last_free = ch;
    }
  }
  return result == -1 ? last_free : result;
}

void PULSE400_ISR( void ) {
#ifdef PULSE400_ENABLE_ISR  
  Pulse400::instance->handleTimerInterrupt();
#endif
}

#if !defined( __TEENSY_3X__ )

void Pulse400::timer_start( void ) {
  instance = this;
  qctl.ptr = PULSE400_START_FLAG;
  Timer1.initialize( 1 ); 
  Timer1.attachInterrupt( PULSE400_ISR );
}

void Pulse400::timer_stop( void ) {
  Timer1.detachInterrupt();
}

Pulse400& Pulse400::sync( void ) {
  cli();
  if ( qctl.ptr == PULSE400_START_FLAG ) { 
    Timer1.restart();
  }
  sei();
  return *this;
}

#endif
  
#if defined( PULSE400_OPTIMIZE_STANDARD )

// Non-optimized ISR

// Teensy 3.1: ISR 0.87% duty cycle @8ch, set speed: 30 us
// Arduino UNO: ISR 7.43% duty cycle @8ch, set speed: 804 us

void Pulse400::handleTimerInterrupt( void ) {
  int16_t next_interval = 0;
  queue_t * q = &queue[qctl.active];
  if ( qctl.ptr == PULSE400_START_FLAG ) { 
    if ( qctl.change ) {
      qctl.change = false;
      qctl.active = qctl.active ^ 1;
      q = &queue[qctl.active];
    }
    qctl.ptr = 0; // Point the queue pointer at the start of the queue
    while( (*q)[qctl.ptr].id != PULSE400_END_FLAG ) {
      digitalWrite( channel[(*q)[qctl.ptr].id].pin, HIGH );
      qctl.ptr++;
    }
    qctl.ptr = 0;
    next_interval = (*q)[qctl.ptr].pw + PULSE400_MIN_PULSE;
  } else {    
    uint16_t previous_pw = (*q)[qctl.ptr].pw;
    while ( !next_interval ) { // Process equal pulse widths in the same timer interrupt period
      digitalWrite( channel[(*q)[qctl.ptr].id].pin, LOW );
      next_interval = (*q)[++qctl.ptr].pw - previous_pw;
    }
    if ( (*q)[qctl.ptr].id == PULSE400_END_FLAG ) { 
      next_interval = period_width - previous_pw;
      qctl.ptr = PULSE400_START_FLAG; 
    }
  } 
  SET_TIMER( next_interval, PULSE400_ISR );
}

#endif
