// sketch.ino ////////////////////////////////////////////////////////////////
// minimal unPhone example, just does LEDs ///////////////////////////////////

#include "unPhone.h"
unPhone u = unPhone("minimal");

void cycleLeds();
void printLEDPins();

void setup() { ///////////////////////////////////////////////////////////////
  Serial.begin(115200);
  D("\nin setup(), doing unPhone begin()...\n\n")
  printLEDPins();

  u.begin();                                    // initialise unPhone hardware
  u.backlight(false);                           // no UI, turn backlight off

  D("\ndone with setup()\n\n")
}

void loop() { ////////////////////////////////////////////////////////////////
  D("IR OFF\n") u.ir(false); cycleLeds();       // cycle through RGB, no IR
  D("IR ON\n")  u.ir(true);  cycleLeds();       // cycle through RGB, IR on
  D("IR OFF, LEDs OFF\n")    u.ir(false);       // all off

  D("\nrepeat\n")                               // once more from the top...
}

void printLEDPins() { ////////////////////////////////////////////////////////
  D("\nLED pins:\n") // the pins & 0x40 (bit 7) gives their non-TCA9555 value
  D("u.IR_LEDS = %#02X,   %3u\n",  u.IR_LEDS,   u.IR_LEDS   & 0b10111111)
  D("LED_BUILTIN=%#02X,   %3u\n",  LED_BUILTIN, LED_BUILTIN & 0b10111111)
  D("u.LED_RED = %#02X,   %2u\n",  u.LED_RED,   u.LED_RED   & 0b10111111)
  D("u.LED_GREEN = %#02X, %2u\n",  u.LED_GREEN, u.LED_GREEN & 0b10111111)
  D("u.LED_BLUE = %#02X,  %2u\n",  u.LED_BLUE,  u.LED_BLUE  & 0b10111111)
}

void cycleLeds() { ///////////////////////////////////////////////////////////
  D("cycling through R, G, B...\n")
  u.rgb(0,0,0); delay(2000);                    // LEDs off
  u.rgb(1,0,0); delay(2000);                    // red
  u.rgb(0,1,0); delay(2000);                    // green
  u.rgb(0,0,1); delay(2000);                    // blue
  u.rgb(0,0,0);
} ////////////////////////////////////////////////////////////////////////////
