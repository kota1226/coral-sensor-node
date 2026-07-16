#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"
#include "MS5837.h"
#include <Adafruit_ADS1X15.h>

Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
MS5837 depth_sensor;
Adafruit_ADS1115 ads;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(1000);
  
  Wire.begin(6, 5);
  
  if (!tsl.begin()) {
    Serial.println("TSL2591が見つかりません");
    while (1);
  }
  // tsl.setGain(TSL2591_GAIN_MED);
  tsl.setGain(TSL2591_GAIN_LOW);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
  
  depth_sensor.setModel(MS5837::MS5837_02BA);
  if (!depth_sensor.init()) {
    Serial.println("Bar02が見つかりません");
    while (1);
  }
  depth_sensor.setFluidDensity(997);
  
  if (!ads.begin()) {
    Serial.println("ADS1115が見つかりません");
    while (1);
  }
  
  Serial.println("全センサー準備完了");
}

void loop() {
  // 光量
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir = lum >> 16;
  uint16_t full = lum & 0xFFFF;
  float lux = tsl.calculateLux(full, ir);
  
  // 深度・水温
  depth_sensor.read();
  float depth = depth_sensor.depth();
  float temperature = depth_sensor.temperature();
  
  // 塩分電圧（ADS1115のA0）
  int16_t ads_raw = ads.readADC_SingleEnded(0);
  float salinity_voltage = ads.computeVolts(ads_raw);
  
  // 表示
  Serial.print("光量: ");
  Serial.print(lux);
  Serial.print(" Lux  水温: ");
  Serial.print(temperature);
  Serial.print(" C  深度: ");
  Serial.print(depth);
  Serial.print(" m  塩分電圧: ");
  Serial.print(salinity_voltage);
  Serial.println(" V");
  
  delay(1000);
}