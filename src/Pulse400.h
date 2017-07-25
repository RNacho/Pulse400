#pragma once 

#include <Arduino.h>

/* TODO
- Synchronized mode: 
    set master_period_bitmap to 50HZ, then whenever an update is executed 
    while the ISR is in dead time, reset the qptr and the period_pointer and set a new period()
*/

#define PULSE400_MAX_CHANNELS 8 // Maximum value 126

#define PULSE400_0HZ   B00000000
#define PULSE400_50HZ  B00000001
#define PULSE400_100HZ B00010001
#define PULSE400_200HZ B01010101
#define PULSE400_400HZ B11111111

#define PULSE400_NO_OF_PERIODS 8
#define PULSE400_DEFAULT_PULSE 1000
#define PULSE400_PERIOD_WIDTH 2500
#define PULSE400_END_FLAG 127

#define MULTI400_NO_OF_CHANNELS 8

#define PINHIGHD( _pin ) PORTD |= ( 1 << _pin );
#define PINLOWD( _pin ) PORTD &= ~( 1 << _pin );

#if defined(__MKL26Z64__)
  #define __TEENSY_3X__
  #define __TEENSY_LC__
#elif defined(__MK20DX256__)
  #define __TEENSY_3X__
  #define __TEENSY_32__
#elif defined(__MK20DX128__)
  #define __TEENSY_3X__
  #define __TEENSY_30__
#elif defined(__MK64FX512__)
  #define __TEENSY_3X__
  #define __TEENSY_35__
#elif defined(__MK66FX1M0__)
  #define __TEENSY_3X__
  #define __TEENSY_36__
#endif

// For Teensy 3.0/3.1/3.2/3.5/3.6/LC use Teensyduino intervalTimer

#if defined( __TEENSY_3X__ )
  #define PULSE400_USE_INTERVALTIMER
#endif  

#ifndef PULSE400_USE_INTERVALTIMER
  #include <TimerOne.h>
#endif

class Esc400;
class Servo400;
class Pulse400;

extern Pulse400 pulse400;

struct channel_struct_t { 
  volatile int8_t pin = -1;
  volatile uint16_t pulse_width;
};

struct pin_bitmap_struct_t {
  union {
    uint32_t lmask;
    struct { uint8_t dmask, bmask, cmask, dummy; };
  };  
};


// Single ESC frontend for Pulse400: use this to control each motor as a single object

class Esc400 {
  
  public:
  Esc400& begin( int8_t pin );
  Esc400& speed( uint16_t v ); 
  int16_t speed( void ); 
  Esc400& range( uint16_t min, uint16_t max ); 
  Esc400& end( void );
  
  private:
  int16_t id_channel = -1;
  uint16_t min = 1000;
  uint16_t max = 2000;
  
};

// Multiple ESC frontend for Pulse400: use this to control banks of motors with a single object

class Multi400 {
  
  public:
  Multi400& begin( int8_t pin0 = -1, int8_t pin1 = -1, int8_t pin2 = -1, int8_t pin3 = -1, int8_t pin4 = -1, int8_t pin5 = -1, int8_t pin6 = -1, int8_t pin7 = -1 );
  Multi400& setSpeed( int16_t v0, int16_t v1 = -1, int16_t v2 = -1, int16_t v3 = -1, int16_t v4 = -1, int16_t v5 = -1 , int16_t v6 = -1, int16_t v7 = -1 ); 
  Multi400& speed( uint8_t no, int16_t v, bool buffer_mode = false );
  int16_t speed( uint8_t no );
  Multi400& off( void );
  Multi400& range( uint16_t min, uint16_t max ); 
  Multi400& end( void );

  int8_t id_channel[MULTI400_NO_OF_CHANNELS] =  { -1, -1, -1, -1, -1, -1, -1, -1 };
  
  private:
  uint16_t min = 1000;
  uint16_t max = 2000;
  uint8_t last_esc = 0;
  
};

// Servo library compatible frontend for Pulse400: drop in replacement for Servo

class Servo400 {
  
  public:
  uint8_t attach(int pin );           // attach the given pin to the next free channel, sets pinMode, returns channel number or 0 if failure
  uint8_t attach(int pin, int min, int max ); // as above but also sets min and max values for writes. 
  void detach();
  void write(int value);             // if value is < 200 its treated as an angle, otherwise as pulse width in microseconds 
  void writeMicroseconds(int value); // Write pulse width in microseconds 
  int read();                        // returns current pulse width as an angle between 0 and 180 degrees
  int readMicroseconds();            // returns current pulse width in microseconds for this servo (was read_us() in first release)
  bool attached(); // return true if this servo is attached, otherwise false 
  
  private:
  int16_t id_channel = -1;
  uint16_t min = 544;
  uint16_t max = 2400;
  
};

// The back-end 400 Hz PWM library, this is used by the frontend classes 
// The interface is not designed to be used directly by sketches

class Pulse400 {
  
  public:
  int8_t attach( int8_t pin ); // Attaches or resets refresh rate
  Pulse400& detach( int8_t id_channel ); // Detaches and optionally frees timer
  Pulse400& set_pulse( int8_t id_channel, uint16_t pulse_width, bool buffer_mode = false );
  int16_t get_pulse( int8_t id_channel );
  Pulse400& frequency( uint8_t freqmask, int16_t period = 2500 );

  static Pulse400 * instance;  
  void handleInterruptTimer( void );
    
  private:
  int channel_count( void );
  int channel_find( int pin = -1 ); // pin = -1 return first free channel, returns -1 if none found
  void timer_start( void );
  void timer_stop( void );
  void update_queue( int8_t id_queue_src, int8_t id_queue_dst, int8_t id_channel, uint16_t pulse_width );
  void update_queue( int8_t id_queue );
  void init_reg_bitmaps( int8_t id_queue );
  void bubble_sort_on_pulse_width( uint8_t list[], uint8_t size );
#ifdef PULSE400_USE_INTERVALTIMER
  IntervalTimer esc_timer;
#endif  

  public: // Temporary! FIXME
  volatile int8_t period_counter;
  volatile int8_t active_queue = 1;
  volatile bool switch_queue;
  volatile uint8_t buffer_cnt = 0;
  volatile uint16_t period_width = PULSE400_PERIOD_WIDTH;
  volatile int8_t period_mask = PULSE400_400HZ; 

  channel_struct_t channel[PULSE400_MAX_CHANNELS];
  uint8_t queue[2][PULSE400_MAX_CHANNELS + 1] = { { PULSE400_END_FLAG }, { PULSE400_END_FLAG } };
  
  volatile uint8_t * qptr;
#if defined( __AVR_ATmega328P__ ) || defined( __TEENSY_3X__ )
  volatile uint8_t pins_high_portb, pins_high_portc, pins_high_portd;
#endif
};

