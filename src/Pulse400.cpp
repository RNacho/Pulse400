#include <Pulse400.h>

Pulse400 pulse400; // Global object
Pulse400 * Pulse400::instance; // Only one instance allowed (singleton)

#ifdef __TEENSY_3X__
static struct { char port; uint8_t bit; } teensy_pins[] = { // A, B, C, D ports: LC could be different!
  /* 0  */ 'B', 16, // PIN kolom mag weg!
  /* 1  */ 'B', 17,
  /* 2  */ 'D',  0,
#ifdef __TEENSY_LC__ 
  /* 3  */ 'A',  1, 
  /* 4  */ 'A',  2, 
#else   
  /* 3  */ 'A', 12, 
  /* 4  */ 'A', 13, 
#endif  
  /* 5  */ 'D',  7,
  /* 6  */ 'D',  4,
  /* 7  */ 'D',  2,
  /* 8  */ 'D',  3,
  /* 9  */ 'C',  3,
  /* 10 */ 'C',  4,
  /* 11 */ 'C',  6,
  /* 12 */ 'C',  7,
  /* 13 */ 'C',  5,
  /* 14 */ 'D',  1,
  /* 15 */ 'C',  0,
  /* 16 */ 'B',  0,
  /* 17 */ 'B',  1,
  /* 18 */ 'B',  3,
  /* 19 */ 'B',  2,
  /* 20 */ 'D',  5,
  /* 21 */ 'D',  6,
  /* 22 */ 'C',  1,
  /* 23 */ 'C',  2,
};
#endif


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
      digitalWrite( pin, LOW ); // Using digitalWriteFast here crashes Teensy LC repeatably
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
          cli(); // Disable interrupts while aborting the pending queue switch
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

void Pulse400::bubble_sort_on_pulse_width( queue_struct_t list[], uint8_t size ) {
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

// Update/refresh the entire queue and set the switch flag 

Pulse400& Pulse400::update() {
  qctl.change = false;
  int queue_cnt = 0;
  int id_queue = qctl.active ^ 1;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
      queue[id_queue][queue_cnt].id = ch;
      queue[id_queue][queue_cnt].pw = channel[ch].pw;
      queue_cnt++;
    }
  }
  queue[id_queue][queue_cnt].id = PULSE400_END_FLAG; // Sentinel value
  bubble_sort_on_pulse_width( queue[id_queue], queue_cnt );
#if defined( __AVR_ATmega328P__ ) 
  pins_high_portb = 0; // Prepare pin high bitmaps for UNO optimization
  pins_high_portc = 0;
  pins_high_portd = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
      if ( channel[ch].pin < 8 )
        pins_high_portd |= 1UL << ( channel[ch].pin );
      else if ( channel[ch].pin < 14 )      
        pins_high_portb |= 1UL << ( channel[ch].pin - 8 );
      else 
        pins_high_portc |= 1UL << ( channel[ch].pin - 14 );        
    }
  }
#endif
#ifdef __TEENSY_3X__
  pins_high_porta = 0; // Prepare pin high bitmaps for TEENSY 3.X optimization
  pins_high_portb = 0;
  pins_high_portc = 0;
  pins_high_portd = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {   
      switch( teensy_pins[channel[ch].pin].port ) {
        case 'A' :
          pins_high_porta |= 1 << teensy_pins[channel[ch].pin].bit;
          break;
        case 'B' :
          pins_high_portb |= 1 << teensy_pins[channel[ch].pin].bit;
          break;  
        case 'C' :
          pins_high_portc |= 1 << teensy_pins[channel[ch].pin].bit;
          break;
        case 'D' :
          pins_high_portd |= 1 << teensy_pins[channel[ch].pin].bit;
          break;
      }
    }
  }  
#endif
  qctl.change = true;
  return *this;
}

// Update one entry in the queue

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

void ESC400PWM_ISR( void ) {
  Pulse400::instance->handleInterruptTimer();
}

void Pulse400::timer_start( void ) {
  qctl.ptr = 0;
  instance = this;
#ifdef PULSE400_USE_INTERVALTIMER  
  esc_timer.begin( ESC400PWM_ISR, 2 ); // interval 1 doesn't work on Teensy LC
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
  queue_t * q = &queue[qctl.active];
  if ( qctl.ptr == PULSE400_END_FLAG || queue[qctl.active][qctl.ptr].id == PULSE400_END_FLAG ) { 
    if ( qctl.change ) {
      qctl.change = false;
      qctl.active = qctl.active ^ 1;
      q = &queue[qctl.active];
    }
#if defined( __AVR_ATmega328P__ ) 
    PORTB |= pins_high_portb; // Arduino UNO optimization: flip pins per bank
    PORTC |= pins_high_portc; // Teensyduino AVR emulation handles this as well 
    PORTD |= pins_high_portd;
#elif defined( __TEENSY_3X__ ) 
    GPIOA_PDOR = pins_high_porta;  // Teensy 3.X optimization
    GPIOB_PDOR = pins_high_portb;
    GPIOC_PDOR = pins_high_portc;  
    GPIOD_PDOR = pins_high_portd;  
#else
    qctl.ptr = 0; // Point the queue pointer at the start of the queue
    while( (*q)[qctl.ptr].id != PULSE400_END_FLAG ) {
      DIGITALWRITE( channel[(*q)[qctl.ptr].id].pin, HIGH );
      qctl.ptr++;
    }
#endif      
    qctl.ptr = 0;
    next_interval = (*q)[qctl.ptr].pw + PULSE400_MIN_PULSE;
  } else {
    uint16_t previous_pw = (*q)[qctl.ptr].pw;
    while ( !next_interval ) { // Process equal pulse widths in the same timer interrupt period
      DIGITALWRITE( channel[(*q)[qctl.ptr].id].pin, LOW );
      next_interval = (*q)[++qctl.ptr].pw - previous_pw;
    }
    if ( (*q)[qctl.ptr].id == PULSE400_END_FLAG ) 
      next_interval = period_width - previous_pw;
  } 
#if defined( PULSE400_USE_INTERVALTIMER )  
  esc_timer.begin( ESC400PWM_ISR, next_interval );
#else 
  Timer1.setPeriod( next_interval ); 
#endif
}

