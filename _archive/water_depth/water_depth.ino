#include <Wire.h>
#include "MS5837.h"

MS5837 sensor;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Wire.begin(6, 5);  // ATOM S3: SDA=G6, SCL=G5
  
  Serial.println("Starting Bar02");
  
  // ★重要: Bar02なので必ず02BAを指定
  sensor.setModel(MS5837::MS5837_02BA);
  
  while (!sensor.init()) {
    Serial.println("Init failed. Check wiring.");
    delay(2000);
  }
  
  sensor.setFluidDensity(997);  // 淡水: 997, 海水: 1029
  Serial.println("Ready!");
}

void loop() {
  sensor.read();
  
  Serial.print("圧力: ");
  Serial.print(sensor.pressure());
  Serial.print(" mbar  温度: ");
  Serial.print(sensor.temperature());
  Serial.print(" C  深度: ");
  Serial.print(sensor.depth());
  Serial.println(" m");
  
  delay(1000);
}