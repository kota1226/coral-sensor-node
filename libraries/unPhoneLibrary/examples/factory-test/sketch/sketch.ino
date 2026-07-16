// sketch.ino ////////////////////////////////////////////////////////////////
// quick test firmware to install on new boards //////////////////////////////

#include <Arduino.h>
#include <freertos/task.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiManager.h> // force inclusion to fix pio LDF WebServer.h issue
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include "unPhone.h"
#include <Adafruit_EPD.h>

unPhone u = unPhone("factory-test");

void setup() { ///////////////////////////////////////////////////////////////
  Serial.begin(115200);
  Serial.println("in setup()...");
  u.begin();
  u.factoryTestMode(true);
  u.factoryTestSetup();
  Serial.println("done with setup()");
}

void loop() { ////////////////////////////////////////////////////////////////
  u.factoryTestLoop();
}
