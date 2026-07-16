// unPhoneFactoryTest.cpp
// factory test mode, mostly by Gareth and Gee

#if UNPHONE_FACTORY_MODE == 1
#include "unPhone.h"            // unPhone specifics

static unPhone* up = NULL;      // up = unphone ptr, static ref to singleton

static bool slideState;
static int loopCounter = 0;
static bool doFlash = true;
static sensors_event_t a, m, g, temp;
static bool doneSetup = false;
static void screenTouched(void);
static void screenError(const char* message);
static bool inFactoryTestMode = false;

bool unPhone::factoryTestMode() {           // read factory test mode
  return inFactoryTestMode;
}
void unPhone::factoryTestMode(bool mode) {  // toggle factory test mode
  inFactoryTestMode = mode;
}

void screenDraw() {
  up->tftp->fillScreen(HX8357_BLACK);
  up->tftp->drawRect(0, 0, 320, 480, HX8357_WHITE);
  up->tftp->setTextSize(3);
  up->tftp->setCursor(0,0);
  up->tftp->setTextColor(HX8357_RED);
  up->tftp->print("Red");
  up->tftp->setCursor(230,0);
  up->tftp->setTextColor(HX8357_GREEN);
  up->tftp->print("Green");
  up->tftp->setCursor(0,460);
  up->tftp->setTextColor(HX8357_BLUE);
  up->tftp->print("Blue");
  up->tftp->setCursor(212,460);
  up->tftp->setTextColor(HX8357_YELLOW);
  up->tftp->print("Yellow");
  up->rgb(1,0,1);
  delay(2000);

  up->tftp->fillScreen(HX8357_BLACK);
  up->tftp->setTextColor(HX8357_RED);
  up->tftp->setCursor(30,50);
  up->tftp->print("But3");

  up->tftp->setCursor(135,50);
  up->tftp->print("But2");

  up->tftp->setCursor(240,50);
  up->tftp->print("But1");

  up->tftp->setCursor(100,100);
  up->tftp->print("Slide ");
  if (slideState) up->tftp->print("<-"); else up->tftp->print("->");
  up->tftp->drawRect(40, 200, 250, 70, HX8357_MAGENTA);
  up->tftp->setTextSize(4);
  up->tftp->setCursor(70,220);
  up->tftp->setTextColor(HX8357_CYAN);
  up->tftp->print("Touch me");

  up->tftp->setTextColor(HX8357_MAGENTA);
  up->tftp->setTextSize(2);
  up->tftp->setCursor(30,160);
  up->tftp->print("(note power switch");
  up->tftp->setCursor(30,175);
  up->tftp->print("now switches power :)");
}

void screenTouched(void) {
  up->tftp->fillRect(40, 200, 250, 70, HX8357_BLACK);
  up->tftp->drawRect(15, 200, 290, 70, HX8357_CYAN);
  up->tftp->setTextSize(4);
  up->tftp->setCursor(25,220);
  up->tftp->setTextColor(HX8357_GREEN);
  up->tftp->print("I'm touched");
  doFlash=false;
  up->vibe(false);
  up->ir(false);
  up->rgb(0,1,0);
}

void screenError(const char* message) {
  up->rgb(1,0,0);
  up->tftp->fillScreen(HX8357_BLACK);
  up->tftp->setCursor(0,10);
  up->tftp->setTextSize(2);
  up->tftp->setTextColor(HX8357_WHITE);
  up->tftp->print(message);
  delay(5000);
  up->backlight(false);
  up->rgb(0,0,0);
  while(true);
}

void unPhone::factoryTestSetup() {
  up = unPhone::me;
  up->checkPowerSwitch();
  slideState = unPhoneTCA::digitalRead(POWER_SWITCH);
  Serial.print("Spin 7/9 test rig, spin ");
  Serial.println(String(UNPHONE_SPIN));
  Serial.print("version() says spin ");
  Serial.println(up->version());
  up->backlight(true);

  screenDraw();

  Serial.println("screen displayed");
  up->tftp->setTextColor(HX8357_GREEN);
  up->rgb(1,1,1);
  if(!up->tsp->begin()) {
    Serial.println("failed to start touchscreen controller");
    screenError("failed to start touchscreen controller");
  } else {
    up->tsp->setRotation(1); // should actually be 3 I think
    Serial.println("Touchscreen started OK");
    up->tftp->setCursor(30,380);
    up->tftp->setTextSize(2);
    up->tftp->print("Touchscreen started");
  }

#if UNPHONE_SPIN == 7
  if(!up->accelp->begin()) {
#elif UNPHONE_SPIN >= 9
  if(!up->accelp->begin_I2C()) {
#endif
    Serial.println("failed to start accelerometer");
    screenError("failed to start accelerometer");
  } else {
    Serial.println("Accelerometer started OK");
    up->tftp->setCursor(30,350);
    up->tftp->setTextSize(2);
    up->tftp->print("Accelerometer started");
  }

  // init the SD card
  // see Adafruit_ImageReader/examples/FeatherWingHX8357/FeatherWingHX8357.ino
  unPhoneTCA::digitalWrite(SD_CS, LOW);
  if(!up->sdp->begin(SD_CS, SD_SCK_MHZ(25))) { // ESP32 25 MHz limit
    Serial.println("failed to start SD card");
    screenError("failed to start SD card");
  } else {
    Serial.println("SD Card started OK");
    up->tftp->setCursor(30,410);
    up->tftp->setTextSize(2);
    up->tftp->setTextColor(HX8357_GREEN);
    up->tftp->print("SD Card started");
  }
  unPhoneTCA::digitalWrite(SD_CS, HIGH);

  if(up->getRegister(BM_I2CADD, BM_VERSION)!=192) {
    Serial.println("failed to start Battery management");
    screenError("failed to start Battery management");
  } else {
    Serial.println("Battery management started OK");
    up->tftp->setCursor(30,440);
    up->tftp->setTextSize(2);
    up->tftp->print("Batt management started");
  }
  up->tftp->setCursor(30,320);
  up->tftp->setTextSize(2);
  up->tftp->println("LoRa failed");
  Serial.printf("Calling up->loraSetup()\n");
  up->loraSetup(); // init the board
  up->tftp->fillRect(30, 320, 250, 32, HX8357_BLACK);
  up->tftp->setCursor(30,320);
  up->tftp->println("LoRa started");

  up->tftp->setTextSize(2);
  up->tftp->setCursor(120,140);
  up->tftp->print("spin: ");
  up->tftp->println(UNPHONE_SPIN);

  up->rgb(1,0,0);
  delay(300);
  up->rgb(0,1,0);
  delay(300);
  up->rgb(0,0,1);
  delay(300);
}

static bool sentLora = false;
void unPhone::factoryTestLoop() {
  if (doFlash) {
    loopCounter++;
    if (loopCounter<50) {
      up->vibe(true);
      up->expanderPower(true);            // enable expander power supply
      up->ir(true);
    } else if (loopCounter>=50) {
      up->vibe(false);
      up->expanderPower(false);           // disable expander power supply
      up->ir(false);
    } else if (loopCounter>=100000 && !sentLora) {
      up->loraSend("unPhone factory test mode message");
      sentLora = true;
    }
    if (loopCounter>=99) loopCounter=0;
  }

  if (up->tsp->touched()) {
    TS_Point p = up->tsp->getPoint();
    if (p.z>40 && p.x>1000 && p.x<3500 && p.y>1300 && p.y<3900) {
      screenTouched();
    }
  }

  if (digitalRead(up->BUTTON1)==LOW) {
    up->tftp->setTextSize(3);
    up->tftp->setCursor(240,50);
    up->tftp->print("But1");
  }

  if (digitalRead(up->BUTTON2)==LOW) {
    up->tftp->setTextSize(3);
    up->tftp->setCursor(135,50);
    up->tftp->print("But2");
  }

  if (digitalRead(up->BUTTON3)==LOW) {
    up->tftp->setTextSize(3);
    up->tftp->setCursor(30,50);
    up->tftp->print("But3");
  }

  if (unPhoneTCA::digitalRead(POWER_SWITCH)!=slideState) {
    slideState=!slideState;
    up->tftp->fillRect(100, 100, 150, 32, HX8357_BLACK);
    up->tftp->setTextSize(3);
    up->tftp->setCursor(130,100);
    up->tftp->print("Slid");
  }

  up->tftp->fillRect(50, 280, 250, 32, HX8357_BLACK);
  up->tftp->setTextSize(2);
  up->tftp->setCursor(50,280);
#if UNPHONE_SPIN == 7
  up->accelp->getEvent(&a, &m, &g, &temp);
#elif UNPHONE_SPIN >= 9
  up->accelp->getEvent(&a, &g, &temp);
#endif
  up->tftp->print("X: "); up->tftp->println(a.acceleration.x);
  up->tftp->setCursor(150,280);
  up->tftp->print("Y: "); up->tftp->println(a.acceleration.y);

  // require 3 button presses to check power switch
  if(up->button1() && up->button2() && up->button3())
    up->checkPowerSwitch();
}
#endif
