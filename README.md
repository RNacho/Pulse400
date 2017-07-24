## Pulse400 - High speed 400 Hz Arduino ESC & Servo library. ##

Generates 400 Hz PWM control pulses for servos and multicopter electronic speed controllers (ESC). By varying the pulse and period width frequencies of more than 1 Khz can be obtained if the receiving hardware can handle the signals. Can control 12 ESC's and/or Servo's at the same time at different frequencies.

Uses the Arduino TimerOne library.

Provides the following frontend classes

- Esc400: Control motor (esc) as a single object
- Multi400: Control multiple motors (bank) as a single object
- Servo400: Drop in replacement for the Arduino Servo library

Example

~~~c++
#include <Pulse400.h>

Multi400 esc;

setup() {
  esc.begin( 4, 5, 6, 7 );
  delay( 1000 );
  esc.setSpeed( 300, 300, 300, 300 );
  delay( 1000 );
  esc.setSpeed( 500, 500, 500, 500 );
  delay( 1000 );
  esc.setSpeed( 0, 0, 0, 0 );
  delay( 1000 );
  esc.end();
}

void loop() {
}
~~~

### WARNING ###

This is quite new software and there are undoubtedly bugs in it. If the PWM signal stops and your quadcopter crashes on your neighbours new Ferrari, or worse, his newborn child, I assume no responsibility at all. The same applies if your expensive ESCs or brushless motors can fire and burn to a crisp. See the LICENSE file for more info.

### Platform dependencies & prerequisites ###

Pulse400 was developed on an Arduino UNO, a Teensy LC and a Teensy 3.1 so it should at least run on those platforms. It uses the Arduino TimerOne library, so it should run all platforms that support them. On the Teensy 3.x/LC platform it uses the intervalTimer class provided with Teensyduino.

### Installation ###

Install the TimerOne library with the Arduino library manager ( Sketch > Include Library > Manage libraries > ). 

Download the Pulse400 library from [here](https://github.com/tinkerspy/Pulse400/archive/master.zip). Unzip it in your Arduino libraries folder and rename the directory from Pulse400-master to just Pulse400. Restart the Arduino IDE and you should be able to open and run the examples.

### Principle of operation ###

The standard Arduino Servo library can be sued to control electronic speed controllers for multicopters, but it's not really suited for it. It runs at 50Hz and is difficult to improve because the pulses are generated in a sequential manner (using multiple timers if needed). A standard ESC PWM pulse takes up 2.5 milliseconds so outputting 4 PWM signals in sequence takes up 10ms and would require at most a 100 Hz signal.

Pulse400 generates the pulses at the same time from only a single timer. 8 sections of 2.5 ms are generated sequentially in a pattern fitting exactly in 20ms (50Hz). By switching each of the sections on or off according to a bitmap a signal of varying frequency can be generated.

|       | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|-------|---|---|---|---|---|---|---|---|
| 0Hz   | - | - | - | - | - | - | - | - |
| 50Hz  | 1 | - | - | - | - | - | - | - |
| 100Hz | 1 | - | - | - | 1 | - | - | - |
| 200Hz | 1 | - | 1 | - | 1 | - | 1 | - |
| 400Hz | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

The Pulse400 class is not meant to be used directly in a sketch. Instead use one (or more) of the Esc400/Multi400/Servo400 frontend classes. They work together fine, so you can use a Multi400 object to control your quadcopter's 4 motors at 400 Hz while controlling you camera pan & tilt with two Servo400 objects at 50 Hz.

The Esc400 and Multi400 classes allow you to manipulate the minimum and maximum pulse width that is used and they also allow you to tune the period setting. By changing the period setting from the default 2500 a lower value and adapting the minimum and maximum pulse to fit within that period you can increase the frequency even further. See below for an explanation.

### The Esc400 class ###

The Esc400 class controls one PWM channel, so you basically create one for each motor. 

| Method | Description | 
|-----------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| begin( int8_t pin, uint16_t frequency = 400 | Initializes the object and attaches it to a pin. Optionally sets the PWM frequency for this object to 0 (off), 50, 100, 200 and 400 (default). |
| frequency( uint16_t v ) | Sets the PWM frequency for this object to 0 (off), 50, 100, 200 and 400 (default). |
| speed( uint16_t v ) | Sets the speed for the ESC, value must be between 0 (min throttle) and 1000 (max throttle) |
| speed() | Retrieves the current speed. |
| range( uint16_t min, uint16_t min,uint16_t period = 2500 ) | Defines the mapping of the min - max throttle value (0 - 1000) to a pulse length in microseconds. The optional third argument sets the length of the total pulse period. Setting this to a lower value than 2500 will allow use of higher than 400 Hz frequencies. See below. |
| end() | Detaches the object from the pin.|

Note that changing the period argument for range has a global effect. All PWM streams, even those created by Multi400 or Servo400 will be affected by it.

#### Example code ####

~~~c++
#include <Pulse400.h>

int pin[] = { 4, 5, 6, 7 };

Esc400 motor[4];

void setup() {
  for ( int m = 0; m < 4; m++ ) 
    motor[m].begin( pin[m] );
  delay( 1000 );
  for ( int m = 0; m < 4; m++ )
    motor[m].speed( 200 );
  delay( 1000 );
  for ( int m = 0; m < 4; m++ )
    motor[m].speed( 0 );
}

void loop() {
}
~~~


### The Multi400 class ###

Use this class to control up to 8 motors (bank) as a single object. Setting the motor speeds for multiple motors is a lot more efficient than setting it for one motor at a time (as the Esc400 class does). To be able to efficiently generate a waveform the Pulse400 class must maintain a list (queue) of steps (pin or or off) to execute. This list must be sorted by pulse width. For each change in speed the list must be inspected and ossible resorted which is relatively expensive in MCU time. By using the Multi400 class and setting the speeds for all motors at once, the generator only needs to sort the list once per bank and not once per motor.

| Method | Description | 
|-----------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| begin( int8_t pin0, int8_t pin1, ..., int8_t pin7 ) | Initializes the object and attaches it to a series of pins. All arguments but the first are optional, specify at least 1 and at most 8 escs/motors. |
| frequency( uint16_t v ) | Sets the PWM frequency for this object to 0 (off), 50, 100, 200 and 400 (default). |
| setSpeed( int16_t v0, int16_t v0,..., int16_t v7 ) | Sets the speed for all ESCs at the same time, value must be between 0 (min throttle) and 1000 (max throttle). |
| speed( uint8_t no, uint16_t v ) | Sets the speed for the ESC identified by 'no', value must be between 0 (min throttle) and 1000 (max throttle). |
| speed( uint8_t no )| Retrieves the current speed for ESC 'no'.|
| range( uint16_t min, uint16_t min,uint16_t period = 2500 ) | Defines the mapping of the min - max throttle value (0 - 1000) to a pulse length in microseconds. The optional third argument sets the length of the total pulse period. Setting this to a lower value than 2500 will allow use of higher than 400 Hz frequencies. See below. |
| end() | Detaches the object from the attached pins |

Note that changing the period argument for range has a global effect. All PWM streams, even those created by Esc400 or Servo400 will be affected by it.

### The Servo400 class ###

The Servo400 class strives to be an exact copy of the standard Arduino Servo class. The default PWM frequency is 50Hz, but that can be changed with the (new) ~~~frequency()~~~ method.

| Method | Description | 
|-----------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| uint8_t attach(int pin) | Attach the given pin to the next free channel, sets pinMode, returns channel number or 0 if failure |
| uint8_t attach(int pin, int min, int max ) | As above but also sets min and max values for writes |
| void frequency( uint16_t v ) | Sets the PWM frequency to 0, 50 (default), 100, 200 or 400 HZ (this method is not in the original Servo library) |
| void detach() | Detaches the object from the pin |
| void write(int value) | if value is < 200 its treated as an angle, otherwise as pulse width in microseconds |
| void writeMicroseconds(int value) | Write pulse width in microseconds |
| int read() | Returns current pulse width as an angle between 0 and 180 degrees |
| int readMicroseconds() | returns current pulse width in microseconds for this servo |
| bool attached() | return true if this servo is attached, otherwise false |
  

### Making it even faster ###



