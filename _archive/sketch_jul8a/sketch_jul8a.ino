#include <Wire.h>

void setup() {
  Wire.begin(6, 5);  // ATOM S3裏面ピン: SDA=G6, SCL=G5
  Serial.begin(115200);
  while (!Serial);
  delay(1000);
  Serial.println("\nI2C Scanner (ATOM S3)");
}

void loop() {
  byte error, address;
  int nDevices = 0;
  Serial.println("Scanning...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) Serial.println("No I2C devices found\n");
  else Serial.println("done\n");
  delay(2000);
}