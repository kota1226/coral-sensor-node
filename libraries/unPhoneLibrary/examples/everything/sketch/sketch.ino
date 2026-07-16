// sketch.ino ////////////////////////////////////////////////////////////////
// an unPhone example with a bit of everything... ////////////////////////////

#include <WiFiMulti.h>          // manage WiFi connections
#include <Adafruit_EPD.h>       // for pio LDF

#if __has_include("private.h")  // for WiFi SSIDs/PSKs and LoRa config; see:
// https://gitlab.com/hamishcunningham/the-internet-of-things/-/blob/master/support/private-template.h
#  include "private.h"
#endif
#include "unPhone.h"

unPhone u = unPhone("everything");

static uint32_t loopIter = 0;   // time slicing iterator
bool useWifi = true;            // toggle wifi connection
WiFiMulti wifiMulti;            // manage...
void wifiSetup();               // ...the wifi...
void wifiConnectTask(void *);   // ...connection

void setup() { ///////////////////////////////////////////////////////////////
  // say hi, init, blink etc.
  Serial.begin(115200);
  Serial.printf("Starting build from: %s\n", u.buildTime);
  u.begin();
  u.store(u.buildTime);

  // if all three buttons pressed, go into factory test mode
#if UNPHONE_FACTORY_MODE == 1
  if(u.button1() && u.button2() && u.button3()) {
    u.factoryTestMode(true);
    u.factoryTestSetup();
    return;
  }
#endif

  // power management
  u.printWakeupReason();        // what woke us up?
  u.checkPowerSwitch();         // if power switch is off, shutdown
  Serial.printf("battery voltage = %3.3f\n", u.batteryVoltage());
  Serial.printf("enabling expander power\n");
  u.expanderPower(true);        // turn expander power on

  // flash the internal RGB LED and then the IR_LEDs
  u.ir(true); u.rgb(0, 0, 0);
  u.rgb(1, 0, 0); delay(300); u.rgb(0, 1, 0); delay(300);
  u.expanderPower(false);       // expander power off
  u.rgb(0, 0, 1); delay(300); u.rgb(1, 0, 0); delay(300);
  u.expanderPower(true);        // expander power on
  u.ir(false);
  u.rgb(0, 1, 0); delay(300); u.rgb(0, 0, 1); delay(300);
  for(uint8_t i = 0; i<4; i++) {
    u.ir(true);  delay(300);
    u.expanderPower(false);     // expander power on
    u.ir(false); delay(300);
    u.expanderPower(true);      // expander power on
  }
  u.rgb(0, 0, 0);

  // buzz a bit
  for(int i = 0; i < 3; i++) {
    u.vibe(true);  delay(150);
    u.vibe(false); delay(150);
  }
  u.printStore();               // print out stored messages

  // get a connection and run the wifi connection task
  if(useWifi) {
    Serial.println("trying to connect to wifi...");
    wifiSetup();
    xTaskCreate(wifiConnectTask, "wifi connect task", 4096, NULL, 1, NULL);
  }

  u.provisioned();              // redisplay the UI for example
  // ledTest(); // TODO on spin 7 LEDs don't work when UI0 is enabled
  Serial.println("done with setup()");
}

void loop() { ////////////////////////////////////////////////////////////////
#if UNPHONE_FACTORY_MODE == 1
  if(u.factoryTestMode()) { u.factoryTestLoop(); return; }
#endif

  // send a couple of TTN messages for testing purposes
  if(loopIter++ == 0)
    u.loraSend("first time: UNPHONE_SPIN=%d MAC=%s", UNPHONE_SPIN, u.getMAC());
  else if(loopIter == 20000000)
    u.loraSend("20000000: UNPHONE_SPIN=%d MAC=%s", UNPHONE_SPIN, u.getMAC());

  if(loopIter % 25000 == 0) // allow IDLE; 100 is min to allow it to fire:
    delay(100);  // https://github.com/espressif/arduino-esp32/issues/6946
}

void wifiSetup() { ///////////////////////////////////////////////////////////
#ifdef _MULTI_SSID1
  Serial.printf("wifiMulti.addAP %s\n", _MULTI_SSID1);
  wifiMulti.addAP(_MULTI_SSID1, _MULTI_KEY1);
#endif
#ifdef _MULTI_SSID2
  Serial.printf("wifiMulti.addAP %s\n", _MULTI_SSID2);
  wifiMulti.addAP(_MULTI_SSID2, _MULTI_KEY2);
#endif
#ifdef _MULTI_SSID3
  Serial.printf("wifiMulti.addAP %s\n", _MULTI_SSID3);
  wifiMulti.addAP(_MULTI_SSID3, _MULTI_KEY3);
#endif
#ifdef _MULTI_SSID4
  Serial.printf("wifiMulti.addAP %s\n", _MULTI_SSID4);
  wifiMulti.addAP(_MULTI_SSID4, _MULTI_KEY4);
#endif
#ifdef _MULTI_SSID5
  Serial.printf("wifiMulti.addAP %s\n", _MULTI_SSID5);
  wifiMulti.addAP(_MULTI_SSID5, _MULTI_KEY5);
#endif
#ifdef _MULTI_SSID6
  Serial.printf("wifiMulti.addAP %s\n", _MULTI_SSID6);
  wifiMulti.addAP(_MULTI_SSID6, _MULTI_KEY6);
#endif
#ifdef _MULTI_SSID7
  Serial.printf("wifiMulti.addAP %s\n", _MULTI_SSID7);
  wifiMulti.addAP(_MULTI_SSID7, _MULTI_KEY7);
#endif
#ifdef _MULTI_SSID8
  Serial.printf("wifiMulti.addAP 8\n");
  wifiMulti.addAP(_MULTI_SSID8, _MULTI_KEY8);
#endif
}

static bool wifiConnected = false; ///////////////////////////////////////////
void wifiConnectTask(void *param) {
  while(true) {
    bool previousWifiState = wifiConnected;
    if(wifiMulti.run() == WL_CONNECTED)
      wifiConnected = true;
    else
      wifiConnected = false;

    // call back to UI controller if state has changed
    if(previousWifiState != wifiConnected) {
      previousWifiState = wifiConnected;
      u.provisioned();
    }

    delay(1000);
  }
}

void ledTest() { // cycle through LEDs
  /*
  D("IR OFF\n") u.ir(false);
  #if UNPHONE_SPIN == 9
  D("red on 13 LOW\n")       unPhoneTCA::digitalWrite(13, LOW);     delay(4000);
  D("red on 13 HIGH\n")      unPhoneTCA::digitalWrite(13, HIGH);    delay(4000);
  #else
  D("red on 8|0x40 HIGH\n")  unPhoneTCA::digitalWrite(8|0x40,HIGH);delay(4000);
  D("red on 8|0x40 LOW\n")   unPhoneTCA::digitalWrite(8|0x40, LOW);  delay(4000);
  #endif
  D("green 9|0x40 LOW\n")    unPhoneTCA::digitalWrite(9|0x40, LOW); delay(4000);
  D("green on 9|0x40 HIGH\n")unPhoneTCA::digitalWrite(9|0x40, HIGH);delay(4000);
  D("IR on\n") u.ir(true);
  D("red on 13 LOW\n")       unPhoneTCA::digitalWrite(13, LOW);     delay(4000);
  D("red on 13 HIGH\n")      unPhoneTCA::digitalWrite(13, HIGH);    delay(4000);
  D("blue on 13|0x40 LOW\n") unPhoneTCA::digitalWrite(13|0x40, LOW);delay(4000);
  D("blue on 13|0x40 HIGH\n")unPhoneTCA::digitalWrite(13|0x40,HIGH);delay(4000);
  */
  // note that LoRa uses the RED LED if USE_LED is true
  D("\nLED pins:\n") // the pins & 0x40 (bit 7) gives their non-TCA9555 value
  D("u.IR_LEDS = %#02X,   %3u\n",  u.IR_LEDS,   u.IR_LEDS   & 0b10111111)
  D("LED_BUILTIN=%#02X,   %3u\n",  LED_BUILTIN, LED_BUILTIN & 0b10111111)
  D("u.LED_RED = %#02X,   %2u\n",  u.LED_RED,   u.LED_RED   & 0b10111111)
  D("u.LED_GREEN = %#02X, %2u\n",  u.LED_GREEN, u.LED_GREEN & 0b10111111)
  D("u.LED_BLUE = %#02X,  %2u\n",  u.LED_BLUE,  u.LED_BLUE  & 0b10111111)
  delay(4000);
  D("IR...\n")
  D("IR ON\n")    u.ir(true);     delay(4000);
  D("IR OFF\n")   u.ir(false);    delay(4000);
  D("RGB...\n")
  delay(4000);
  D("000 off\n")  u.rgb(0, 0, 0); delay(4000); // off
  D("111 all\n")  u.rgb(1, 1, 1); delay(4000); // all
  D("110 r+g\n")  u.rgb(1, 1, 0); delay(4000); // red + green
  D("101 r+b\n")  u.rgb(1, 0, 1); delay(4000); // red + blue
  D("011 g+b\n")  u.rgb(0, 1, 1); delay(4000); // green + blue
  D("100 red\n")  u.rgb(1, 0, 0); delay(4000); // red
  D("010 grn\n")  u.rgb(0, 1, 0); delay(4000); // green
  D("001 blue\n") u.rgb(0, 0, 1); delay(4000); // blue
  D("000 off\n")  u.rgb(0, 0, 0); delay(4000); // off

  D("\nRGB+IR\n") u.ir(true);     delay(4000);
  D("111 all\n")  u.rgb(1, 1, 1); delay(4000); // all
  D("110 r+g\n")  u.rgb(1, 1, 0); delay(4000); // red + green
  D("101 r+b\n")  u.rgb(1, 0, 1); delay(4000); // red + blue
  D("011 g+b\n")  u.rgb(0, 1, 1); delay(4000); // green + blue
  D("100 red\n")  u.rgb(1, 0, 0); delay(4000); // red
  D("010 grn\n")  u.rgb(0, 1, 0); delay(4000); // green
  D("001 blue\n") u.rgb(0, 0, 1); delay(4000); // blue
  D("000 off\n")  u.rgb(0, 0, 0); delay(4000); // off
  D("IR OFF\n")   u.ir(false);    delay(4000);
}
