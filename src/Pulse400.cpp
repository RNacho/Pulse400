#include <Pulse400.h>

Pulse400 pulse400; // Global object
Pulse400 * Pulse400::instance; // Only one instance allowed (singleton)

#ifdef __TEENSY_3X__
static struct { uint8_t port; uint8_t bit; } teensy_pins[] = { // A/0, B/1, C/2, D/3 ports: LC port 3 & 4 differ
  /* 0  */ 1, 16, // Pin 0 & 1 won't work (would require 32 bit masks)
  /* 1  */ 1, 17,
  /* 2  */ 3,  0,
#ifdef __TEENSY_LC__ 
  /* 3  */ 0,  1, 
  /* 4  */ 0,  2, 
#else   
  /* 3  */ 0, 12, 
  /* 4  */ 0, 13, 
#endif  
  /* 5  */ 3,  7,
  /* 6  */ 3,  4,
  /* 7  */ 3,  2,
  /* 8  */ 3,  3,
  /* 9  */ 2,  3,
  /* 10 */ 2,  4,
  /* 11 */ 2,  6,
  /* 12 */ 2,  7,
  /* 13 */ 2,  5,
  /* 14 */ 3,  1,
  /* 15 */ 2,  0,
  /* 16 */ 1,  0,
  /* 17 */ 1,  1,
  /* 18 */ 1,  3,
  /* 19 */ 1,  2,
  /* 20 */ 3,  5,
  /* 21 */ 3,  6,
  /* 22 */ 2,  1,
  /* 23 */ 2,  2,
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
#ifdef __TEENSY_3X__ 
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
  pins_high_port[0] = 0; // Prepare pin high bitmaps for TEENSY 3.X optimization
  pins_high_port[1] = 0;
  pins_high_port[2] = 0;
  pins_high_port[3] = 0;
  for ( int ch = 0; ch < PULSE400_MAX_CHANNELS; ch++ ) {
    if ( channel[ch].pin != PULSE400_UNUSED ) {
      pins_high_port[teensy_pins[channel[ch].pin].port] |= 1 << teensy_pins[channel[ch].pin].bit;
    }
  }  
  int16_t cnt = 1;
  int16_t last_pw = -1;
  uint16_t bits[4];
  while ( queue_cnt >= 0 ) {
    if ( queue[id_queue][queue_cnt].pw == last_pw ) {
      cnt++;
    } else {
      bits[0] = bits[1] = bits[2] = bits[3] = 0;
      cnt = 1;
    }
    queue[id_queue][queue_cnt].cnt = cnt;
    int id = queue[id_queue][queue_cnt].id;
    bits[teensy_pins[channel[id].pin].port] |= 1 << teensy_pins[channel[id].pin].bit; 
    queue[id_queue][queue_cnt].pins_low_port[0] = bits[0];
    queue[id_queue][queue_cnt].pins_low_port[1] = bits[1];
    queue[id_queue][queue_cnt].pins_low_port[2] = bits[2];
    queue[id_queue][queue_cnt].pins_low_port[3] = bits[3];
    last_pw = queue[id_queue][queue_cnt].pw;
    queue_cnt--;
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
  Pulse400::instance->handleTimerInterrupt();
}

void Pulse400::timer_start( void ) {
  qctl.ptr = PULSE400_START_FLAG;
  instance = this;
#ifdef PULSE400_USE_INTERVALTIMER  
  timer.begin( ESC400PWM_ISR, 2 ); // interval 1 doesn't seem to work on Teensy LC
#else 
  Timer1.initialize( 1 ); 
  Timer1.attachInterrupt( ESC400PWM_ISR );
#endif    
}

void Pulse400::timer_stop( void ) {
#ifdef PULSE400_USE_INTERVALTIMER  
  timer.end();
#else 
  Timer1.detachInterrupt();
#endif    
}

Pulse400& Pulse400::sync( void ) {
  cli();
  if ( qctl.ptr == PULSE400_START_FLAG ) { 
#ifdef PULSE400_USE_INTERVALTIMER    
    handleTimerInterrupt();
#else     
    Timer1.restart();
#endif  
  }
  sei();
  return *this;
}

#if defined( __AVR_ATmega328P__ )

// ISR optimized for Arduino UNO (ATMega328P)
// Arduino: ISR 4.63% duty cycle @8ch, set speed: 840 us

void Pulse400::handleTimerInterrupt( void ) {
  int16_t next_interval = 0;
  queue_t * q = &queue[qctl.active];
  if ( qctl.ptr == PULSE400_END_FLAG ) { 
    if ( qctl.change ) {
      qctl.change = false;
      qctl.active = qctl.active ^ 1;
      q = &queue[qctl.active];
    }
    PORTB |= pins_high_portb; // Arduino UNO optimization: flip pins per bank
    PORTC |= pins_high_portc; // Teensyduino AVR emulation handles this as well 
    PORTD |= pins_high_portd;
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
#ifdef PULSE400_USE_INTERVALTIMER  
  timer.begin( ESC400PWM_ISR, next_interval );
#else 
  Timer1.setPeriod( next_interval ); 
#endif  
}

#elif defined( __TEENSY_3X__ )

// ISR optimized for Teensy 3.x/LC

// Teensy 3.1: ISR 0.5% duty cycle @8ch, set speed: 44 us
// Teensy LC : ISR 1% duty cycle @8ch, set speed: 88 us

void Pulse400::handleTimerInterrupt( void ) { 
  int16_t next_interval = 0;
  queue_t * q = &queue[qctl.active];
  if ( qctl.ptr == PULSE400_START_FLAG ) { 
    if ( qctl.change ) {
      qctl.change = false;
      qctl.active = qctl.active ^ 1;
      q = &queue[qctl.active];
    }
    qctl.ptr = 0;
    GPIOA_PDOR = pins_high_port[0];  
    GPIOB_PDOR = pins_high_port[1];
    GPIOC_PDOR = pins_high_port[2];  
    GPIOD_PDOR = pins_high_port[3];  
    next_interval = (*q)[0].pw + PULSE400_MIN_PULSE;
  } else {    
    uint16_t previous_pw = (*q)[qctl.ptr].pw;
    GPIOA_PCOR = (*q)[qctl.ptr].pins_low_port[0];  
    GPIOB_PCOR = (*q)[qctl.ptr].pins_low_port[1];
    GPIOC_PCOR = (*q)[qctl.ptr].pins_low_port[2];  
    GPIOD_PCOR = (*q)[qctl.ptr].pins_low_port[3];  
    qctl.ptr += (*q)[qctl.ptr].cnt;
    next_interval = (*q)[qctl.ptr].pw - previous_pw;
    if ( (*q)[qctl.ptr].id == PULSE400_END_FLAG ) { 
      next_interval = period_width - previous_pw;
      qctl.ptr = PULSE400_START_FLAG; 
    }
  } 
  timer.begin( ESC400PWM_ISR, next_interval );
}

#else
  
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
#ifdef PULSE400_USE_INTERVALTIMER  
  timer.begin( ESC400PWM_ISR, next_interval );
#else 
  Timer1.setPeriod( next_interval ); 
#endif  
}

#endif