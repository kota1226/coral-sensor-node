#include <Wire.h>
#include "MS5837.h"

MS5837 depth_sensor;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(1000);
  
  Wire.begin(6, 5);  // ATOM S3: SDA=G6, SCL=G5
  
  depth_sensor.setModel(MS5837::MS5837_02BA);
  if (!depth_sensor.init()) {
    Serial.println("Bar02が見つかりません");
    while (1);
  }
  depth_sensor.setFluidDensity(997);
  
  Serial.println("Bar02 準備完了");
}

void loop() {
  depth_sensor.read();
  
  float pressure = depth_sensor.pressure();       // 圧力（mbar）
  float depth = depth_sensor.depth();             // 深度（m）
  float temperature = depth_sensor.temperature(); // 温度（℃）
  
  Serial.print("圧力: ");
  Serial.print(pressure, 2);   // 小数点2桁で表示
  Serial.print(" mbar  深度: ");
  Serial.print(depth, 3);      // 小数点3桁
  Serial.print(" m  温度: ");
  Serial.print(temperature, 2);
  Serial.println(" C");
  
  delay(1000);
}