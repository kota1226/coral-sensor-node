// unPhone.h /////////////////////////////////////////////////////////////////
// core definitions and includes /////////////////////////////////////////////

#ifndef UNPHONE_H
#define UNPHONE_H
#define UNPHONE_TCA_INJECTED

// build flags ///////////////////////////////////////////////////////////////
#ifndef UNPHONE_SPIN                            // board version
#  define UNPHONE_SPIN 9 // default hardware spin to 9 if not set by the build
#endif      // context (though _should_ be in boards.txt and/or <device>.json)
#ifndef UNPHONE_UI0                             // the simple GFX-based UI
#  define UNPHONE_UI0 1
#endif
#ifndef UNPHONE_LORA                            // LoRaWAN / TTN support
#  define UNPHONE_LORA 1
#endif
#ifndef UNPHONE_FACTORY_MODE                    // factory test mode
#  define UNPHONE_FACTORY_MODE 1                // (NOTE: requires UI0)
#endif

// dependencies //////////////////////////////////////////////////////////////
#include <Arduino.h>           // include this first to pick up pins_arduino.h
#include <stdint.h>                             // integer types
#include <Wire.h>                               // I²C comms on the Arduino
#include <SPI.h>                                // the SPI bus
#include <Adafruit_Sensor.h>                    // base for sensor abstraction
#include <SdFat.h>                              // SD card & FAT filesystem
#if UNPHONE_UI0 == 1
#  include <Adafruit_GFX.h>                     // core graphics library
#  include <Adafruit_HX8357.h>                  // TFT LCD (alt: TFT_eSPI)
#endif
#include <XPT2046_Touchscreen.h>                // touch screen & VBAT sensor
#if UNPHONE_SPIN == 7                           // the accelerometer sensor
#  include <Adafruit_LSM9DS1.h>
#elif UNPHONE_SPIN >= 9
#  include <Adafruit_LSM6DS3TRC.h>
#endif

// the main API //////////////////////////////////////////////////////////////
class unPhone {
public:
  unPhone();                                    // construction
  unPhone(const char *);                        // construction with app name
  static unPhone *me; // access singleton ptr in static contexts after constr

  void begin();                                 // initialise hardware
  uint8_t version();                            // hardware revision
  const char *getMAC();                         // the ESP MAC address

#if UNPHONE_SPIN == 7 // version 7 pindefs ///////////////////////////////////
  static const uint8_t LCD_RESET        =  1 | 0x40; // if |0x40 is on TCA9555
  static const uint8_t BACKLIGHT        =  2 | 0x40;
  static const uint8_t LCD_CS           =  3 | 0x40;
  static const uint8_t LCD_DC           = 21;
  static const uint8_t LORA_CS          =  4 | 0x40;
  static const uint8_t LORA_RESET       =  5 | 0x40;
  static const uint8_t TOUCH_CS         =  6 | 0x40;
  static const uint8_t LED_RED          =  8 | 0x40;
  static const uint8_t POWER_SWITCH     = 10 | 0x40;
  static const uint8_t SD_CS            = 11 | 0x40;
  static const uint8_t BUTTON1          = 33;   // left button
  static const uint8_t BUTTON2          = 35;   // middle button
  static const uint8_t BUTTON3          = 34;   // right button
  static const uint8_t IR_LEDS          = 13;   // the IR LED pins
  static const uint8_t EXPANDER_POWER   =  2;   // enable exp when high
#elif UNPHONE_SPIN >= 9 // version 9+ pindefs ////////////////////////////////
  static const uint8_t LCD_RESET        = 46;
  static const uint8_t BACKLIGHT        =  2 | 0x40;
  static const uint8_t LCD_CS           = 48;
  static const uint8_t LCD_DC           = 47;
  static const uint8_t LORA_CS          = 44;
  static const uint8_t LORA_RESET       = 42;
  static const uint8_t TOUCH_CS         = 38;
  static const uint8_t LED_RED          = 13;
  static const uint8_t POWER_SWITCH     = 18;
  static const uint8_t SD_CS            = 43;
  static const uint8_t BUTTON1          = 45;   // left button
  static const uint8_t BUTTON2          =  0;   // middle button
  static const uint8_t BUTTON3          = 21;   // right button
  static const uint8_t IR_LEDS          = 12;   // the IR LED pins
  static const uint8_t EXPANDER_POWER   =  0 | 0x40; // enable exp brd if high
#endif
  static const uint8_t VIBE             =  7 | 0x40;
  static const uint8_t LED_GREEN        =  9 | 0x40;
  static const uint8_t LED_BLUE         = 13 | 0x40;
  static const uint8_t USB_VSENSE       = 14 | 0x40;

  bool button1();                               // register...
  bool button2();                               // ...button...
  bool button3();                               // ...presses

  void vibe(bool);                              // vibe motor on or off
  void ir(bool);                                // IR LEDs on or off

  bool powerSwitchIsOn();                       // is power switch turned on?
  bool usbPowerConnected();                     // is USB power connected?
  void checkPowerSwitch();                      // if pwr switch off, shut down
  void turnPeripheralsOff();                    // shut down periphs
  void wakeOnPowerSwitch();                     // wake interrupt on pwr switch
  void printWakeupReason();                     // what woke us up?

  void *uiCont;                                 // the UI controller
  void redraw();                                // redraw the UI
  void provisioned();                           // call when provisioning done
  void uiLoop();                                // allow the UI to run

  void recoverI2C();                            // deal with i2c hangs
#if UNPHONE_FACTORY_MODE == 1
  bool factoryTestMode();                       // read factory test mode
  void factoryTestMode(bool);                   // toggle factory test mode
  void factoryTestSetup();                      // factory test mode setup
  void factoryTestLoop();                       // factory test mode loop
#endif

  // the touch screen, display and accelerometer ////////////////////////////
#if UNPHONE_UI0 == 1
  Adafruit_HX8357 *tftp;                        // UI0 uses Adafruit LCD lib
#else
  void *tftp;                                   // void* so can use eg TFT_eSPI
#endif
  XPT2046_Touchscreen *tsp;
#if UNPHONE_SPIN == 7
  Adafruit_LSM9DS1 *accelp;
#elif UNPHONE_SPIN >= 9
  Adafruit_LSM6DS3TRC *accelp;
#endif
  void getAccelEvent(sensors_event_t *);        // spin-agnostic accelerometer
  void backlight(bool);                         // backlight on or off
  void expanderPower(bool);                     // expander board power on/off

  // SD card filesystem //////////////////////////////////////////////////////
  SdFat *sdp;

  // calibration data for converting raw touch data to screen coordinates ////
  static const uint16_t TS_MINX =  300;
  static const uint16_t TS_MAXX = 3800;
  static const uint16_t TS_MINY =  500;
  static const uint16_t TS_MAXY = 3750;

  // the RGB LED /////////////////////////////////////////////////////////////
  void rgb(uint8_t red, uint8_t green, uint8_t blue);

  // LoRa radio //////////////////////////////////////////////////////////////
  void loraSetup();                             // init the LoRa board
  void loraLoop();                              // service lora transactions
  void loraSend(const char *, ...);             // send (TTN) LoRaWAN message
  static const uint8_t LORA_PAYLOAD_LEN = 101;  // max payload bytes (+ '\0')
#if UNPHONE_SPIN == 7
  static const uint8_t LMIC_DIO0 = 39;
  static const uint8_t LMIC_DIO1 = 26;
#elif UNPHONE_SPIN >= 9
  static const uint8_t LMIC_DIO0 = 10;
  static const uint8_t LMIC_DIO1 = 11;
#endif

  // power management chip API ///////////////////////////////////////////////
  static const byte BM_I2CADD   = 0x6b; // the chip lives here on I²C
  static const byte BM_WATCHDOG = 0x05; // charge end/timer cntrl register
  static const byte BM_OPCON    = 0x07; // misc operation control reg
  static const byte BM_STATUS   = 0x08; // system status register
  static const byte BM_VERSION  = 0x0a; // vendor/part/revision status reg
  float batteryVoltage();               // get the battery voltage
  void setShipping(bool value);         // tells BM chip to shut down
  void setRegister(byte address, byte reg, byte value); //
  byte getRegister(byte address, byte reg);             // I²C...
  void write8(byte address, byte reg, byte value);      // ...helpers
  byte read8(byte address, byte reg);                   //

  // a small, rotating, persistent store  (using Preferences API) ////////////
  void beginStore();                    // set up small persistent store area
  void store(const char *);             // save a value
  void printStore();                    // play back the list of saved strings
  void clearStore();                    // clear it (note doesn't empty nvs!)
  static const uint8_t STORE_SIZE = 10; // max strings stored; must be <=255/2

  // misc utilities //////////////////////////////////////////////////////////
  const char *appName;                  // name for the firmware (eg unPhone9)
  const char *buildTime;                // build date / time
}; // class unPhone //////////////////////////////////////////////////////////


/*
The unPhone has a [TCA9555](https://www.ti.com/product/TCA9555) IO expansion
chip that is controlled over I2C and to which the SPI chip select (CS), reset
and etc. lines of many of the modules were connected in versions before spin 9
(from 9+ only the LEDs, expander board power and the vibration motor are on
the TCA). To use these connections the IO expander has to be told to trigger
those lines. This meant that the available libraries for the modules also
needed to be adapted to talk to the TCA, e.g. when doing digitalWrite or
pinMode. We did this by injecting code to call our own versions of these
functions (defined below) and setting the second highest bit of the pin number
high to signal those pins that are controlled via the TCA9555. In later
versions we don't need to patch the libraries any more, as only our own code
is talking to the TCA. In any case, the unPhoneTCA class below manages the
chip. (It used to be called IOExpander but was renamed to prevent confusing it
with the unPhone expansion board, an additional PCB that connects to the main
board by ribbon cable.)

Usage notes: call `unPhoneTCA::begin()` to initialise, after `Wire.begin()`,
then to interface with the IO Expander pins you can do e.g.:
`unPhoneTCA::digitalWrite(unPhone::LED_RED, LOW)`
*/
class unPhoneTCA { ///////////////////////////////////////////////////////////
  public:
    static const uint8_t i2c_address = 0x26;
    static uint16_t directions;    // cache current state of ports after first
    static uint16_t output_states; // read of the values during initialisation
    static void begin();
    static void pinMode(uint8_t pin, uint8_t mode);       // if you change...
    static void digitalWrite(uint8_t pin, uint8_t value); // ...these, also...
    static uint8_t digitalRead(uint8_t pin); // ...change bin/lib-injector.cpp
  private:
    static uint16_t readRegisterWord(uint8_t reg);
    static void writeRegisterWord(uint8_t reg, uint16_t value);
};

// macros for debug (and error) calls to log/printf, and delay/yield/timing //
#ifdef UNPHONE_PRODUCTION_BUILD
# define D(args...) (void)0;
#else
# define D(args...) printf(args);
#endif
#define  E(args...) printf("ERROR: " args);
static const char *TAG = "MAIN";        // ESP logger debug tag
#define WAIT_A_SEC   vTaskDelay(    1000/portTICK_PERIOD_MS); // 1 second
#define WAIT_SECS(n) vTaskDelay((n*1000)/portTICK_PERIOD_MS); // n seconds
#define WAIT_MS(n)   vTaskDelay(       n/portTICK_PERIOD_MS); // n millis

#endif ///////////////////////////////////////////////////////////////////////
