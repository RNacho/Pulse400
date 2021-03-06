#pragma once 

// BUGS:
// Timer(s) need a minimum # of micros between interrupts, 4 for teenys, 50 (?!) for UNO? Fix this?

#include <Arduino.h>
#include <timer/TwoTimer.hpp>

// Configure number of channels here

#define PULSE400_MAX_CHANNELS 8 // Maximum value: 31
#define MULTI400_NO_OF_CHANNELS 8 // Maximum value: 31
#define RC400_NO_OF_CHANNELS 6

// Turn options on/off for debugging/testing/development

#define PULSE400_OPTIMIZE_ARDUINO_UNO
#define PULSE400_OPTIMIZE_TEENSY_3X
#define PULSE400_ENABLE_ISR

#define PULSE400_DEFAULT_PULSE 1000
#define PULSE400_MIN_PULSE 360
#define PULSE400_PERIOD_MAX 2500
#define PULSE400_END_FLAG 31
#define PULSE400_JMP_HIGH 32 // fits in qctl.next (6 bit = 0..63)
#define PULSE400_JMP_DEADLINE 33
#define PULSE400_UNUSED 31

#define RC400_IDLE_DISCONNECT 100000

#define PINHIGHD( _pin ) PORTD |= ( 1 << _pin );
#define PINLOWD( _pin ) PORTD &= ~( 1 << _pin );

// Identify Teensy models

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
  #define SET_TIMER( _interval, _func ) timer.begin( _func, _interval )
#else  
  #include <TimerOne.h>
  #define SET_TIMER( _interval, _func ) Timer1.setPeriod( _interval ) 
#endif

#undef PULSE400_OPTIMIZE_STANDARD
#if !defined( __TEENSY_3X__ ) || !defined( PULSE400_OPTIMIZE_TEENSY_3X ) 
  #if !defined( __AVR_ATmega328P__ ) || !defined( PULSE400_OPTIMIZE_ARDUINO_UNO )
    #define PULSE400_OPTIMIZE_STANDARD    
  #endif
#endif

class Esc400;
class Servo400;
class Multi400;
class Pulse400;

extern void PULSE400_ISR( void );

struct channel_struct_t { 
  uint16_t pin :5; 
  uint16_t pw : 11;
};

#if defined( __TEENSY_3X__ ) && defined( PULSE400_OPTIMIZE_TEENSY_3X )    

struct reg_struct_t {
#ifdef __TEENSY_LC__ 
  volatile uint16_t PA;
  volatile uint8_t PB;
  volatile uint8_t PC;
  volatile uint8_t PD;
#else  
  volatile uint16_t PA;
  volatile uint32_t PB;
  volatile uint8_t PC;
  volatile uint8_t PD;
#endif  
};

#elif defined( __AVR_ATmega328P__ )

struct reg_struct_t {
  volatile uint8_t PB;
  volatile uint8_t PC;
  volatile uint8_t PD;
};

#endif

struct queue_struct_t { 
  volatile uint16_t id : 5; 
  volatile uint16_t pw : 11;
#if defined( __TEENSY_3X__ ) && defined( PULSE400_OPTIMIZE_TEENSY_3X )    
  volatile uint8_t cnt;
  reg_struct_t pins_low;
#endif
};

typedef queue_struct_t queue_t[PULSE400_MAX_CHANNELS + 1];

// Single ESC frontend for Pulse400: use this to control each motor as a single object

class Esc400 {
  
  public:
  Esc400& begin( Pulse400&, int8_t pin );
  Esc400& speed( uint16_t v ); 
  int16_t speed( void ); 
  Esc400& outputRange( uint16_t min, uint16_t max ); 
  Esc400& end( void );
  Esc400& frequency( uint16_t f );
  
  private:
  Pulse400 * pulse400;
  int16_t id_channel = -1;
  uint16_t min = 1000;
  uint16_t max = 2000;
  
};

// Multiple ESC frontend for Pulse400: use this to control banks of motors with a single object

class Multi400 {
  
  public:
  Multi400( Pulse400& pulse400 );
  Multi400& begin( int8_t pin0 = -1, int8_t pin1 = -1, int8_t pin2 = -1, int8_t pin3 = -1, int8_t pin4 = -1, int8_t pin5 = -1, int8_t pin6 = -1, int8_t pin7 = -1 );
  Multi400& set( int16_t v0, int16_t v1 = -1, int16_t v2 = -1, int16_t v3 = -1, int16_t v4 = -1, int16_t v5 = -1 , int16_t v6 = -1, int16_t v7 = -1 ); 
  Multi400& speed( uint8_t no, int16_t v, bool no_update = false );
  int16_t speed( uint8_t no );
  Multi400& off( void );
  Multi400& outputRange( uint16_t min, uint16_t max, int16_t minPulse = -1 ); 
  Multi400& end( void );
  Multi400& autosync( bool v = true );
  Multi400& sync();
  Multi400& frequency( uint16_t f );
  Multi400& enabled( bool v );
  
  private:
  Pulse400 * pulse400;
  bool pulse_sync, disabled;
  uint16_t min = 1000;
  uint16_t max = 2000;
  
};

// Servo library compatible frontend for Pulse400: drop in replacement for Servo

class Servo400 {
  
  public:
  uint8_t attach( Pulse400& pulse400, int pin );           // attach the given pin to the next free channel, sets pinMode, returns channel number or 0 if failure
  uint8_t attach( Pulse400& pulse400, int pin, int min, int max ); // as above but also sets min and max values for writes. 
  void detach();
  void write(int value);             // if value is < 200 its treated as an angle, otherwise as pulse width in microseconds 
  void writeMicroseconds(int value); // Write pulse width in microseconds 
  int read();                        // returns current pulse width as an angle between 0 and 180 degrees
  int readMicroseconds();            // returns current pulse width in microseconds for this servo (was read_us() in first release)
  bool attached(); // return true if this servo is attached, otherwise false 
  void frequency( uint16_t f );
  
  private:
  Pulse400 * pulse400;
  int16_t id_channel = -1;
  uint16_t min = 544;
  uint16_t max = 2400;
  
};

// The back-end 400 Hz PWM library, this is used by the frontend classes 
// The interface is not designed to be used directly by sketches

class Pulse400 {
  
  public:
  Pulse400();
  int8_t attach( int8_t pin, int8_t force_id = -1 ); // Attaches pin
  Pulse400& detach( int8_t id_channel ); // Detaches and optionally frees timer
  Pulse400& pulse( int8_t id_channel, uint16_t pulse_width, bool no_update = false );
  int16_t pulse( int8_t id_channel );
  Pulse400& update( void );
  Pulse400& frequency( uint16_t f );
  Pulse400& minPulse( int16_t f = 360 );
  Pulse400& sync( void );

  static Pulse400 * instance;  
  void handleTimerInterrupt( void );
    
  private:
  int channel_count( void );
  int channel_find( int pin = -1 ); // pin = -1 returns first free channel, returns -1 if none found
  void timer_start( void );
  void timer_stop( void );
  void update_queue_entry( queue_struct_t src[], queue_struct_t dst[], int8_t id_channel, uint16_t pw );
  void init_optimization( queue_struct_t queue[], int8_t queue_cnt );
  void sort_on_pulse_width( queue_struct_t list[], uint8_t size );
  void quicksort_on_pulse_width( queue_struct_t list[], int first, int last );
#ifdef PULSE400_USE_INTERVALTIMER
  IntervalTimer timer;
#endif  

  public: // Temporary! FIXME
  struct {
    volatile uint8_t next : 6; // max 64!
    volatile uint8_t active : 1;
    volatile uint8_t change : 1;
  } qctl;
  volatile uint8_t update_cnt = 0;
  volatile uint16_t cycle_deadline = PULSE400_MIN_PULSE;
  volatile uint16_t cycle_width = PULSE400_PERIOD_MAX - PULSE400_MIN_PULSE;

  channel_struct_t channel[PULSE400_MAX_CHANNELS];
  queue_t queue[2] = { { { PULSE400_END_FLAG } }, { { PULSE400_END_FLAG } } };
  
#if defined( __AVR_ATmega328P__ ) && defined( PULSE400_OPTIMIZE_ARDUINO_UNO )
  volatile reg_struct_t pins_high;
#elif defined( __TEENSY_3X__ ) && defined( PULSE400_OPTIMIZE_TEENSY_3X )
  volatile reg_struct_t pins_high;
#endif

};

typedef struct {
    uint8_t pin;
    uint16_t value;
    uint32_t last_high;
} rc400_channel_struct;

#ifndef __TEENSY_3X__
typedef struct {
  byte reg;
  byte channel[8];
} rc400_int_struct;
#endif


class Rc400 {
 public:
  void pwm( int8_t p0, int8_t p1 = -1, int8_t p2 = -1, int8_t p3 = -1, int8_t p4 = -1, int8_t p5 = -1 );
  void ppm( int8_t p0 );
  int read( int ch );
  bool connected();
  void end();
  
  static Rc400 * instance;
  void handleInterruptPWM( int pch );
  void handleInterruptPPM();
  void register_pin_change_pwm( byte int_no, byte int_mask, byte bits );

 private:
  void set_channel( int ch, int pin );
  rc400_channel_struct volatile channel[RC400_NO_OF_CHANNELS];
#ifndef __TEENSY_3X__    
  rc400_int_struct volatile int_state[3];
#endif
  uint8_t volatile ppm_pulse_counter;
  uint32_t volatile ppm_last_pulse;
  uint32_t volatile last_interrupt;
    
};
