/*
 * センサーノード統合コード
 *
 * test_sensor.ino をベースに、DS18B20（水温）と pH（ADS1115 A1）を追加したもの。
 *
 * 変更履歴:
 *   2026-07-15  test_sensor.ino から派生。DS18B20 と pH を追加。
 *
 * ---- 構成 ----
 *   ATOM S3 (ESP32-S3)
 *     I2C バス (SDA=G6, SCL=G5)
 *       ├─ TSL2591  (0x29)  光量
 *       ├─ Bar02    (0x76)  深度・圧力・基板温度
 *       └─ ADS1115  (0x48)  外部ADC
 *            ├─ A0: DFR0300-H  塩分（EC）
 *            └─ A1: SEN0161-V2 pH
 *     1-Wire (G8)
 *       └─ DS18B20  水温（外付け 4.7kΩ プルアップ済み）
 *
 * ---- 水温について ----
 *   旧 test_sensor.ino は Bar02 内蔵の温度を「水温」として出力していたが、
 *   Bar02 の温度は本来 圧力補正用。水温の共変量には DS18B20 を使う。
 *   （DS18B20 は温度計比較済み。系統オフセット 約 +0.9℃ / 2026-07-13）
 *   Bar02 の温度も参考として残す（基板温度）。
 *
 * ---- 未校正の値について ----
 *   塩分・pH は「電圧」のまま出力する。換算式は校正後に決める。
 *     塩分の暫定式（2026-07-13 の校正）: EC(mS/cm) = (V - 0.01) * 34.81
 *     pH は校正液 6.86 と 9.18 の2点で校正する（海水 pH 7.5〜8.5 を挟むため）
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"
#include "MS5837.h"
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---- 設定 ----
#define I2C_SDA 6
#define I2C_SCL 5
#define ONE_WIRE_BUS 8      // DS18B20 の DATA（黄）

#define ADS_CH_SALINITY 0   // A0: DFR0300-H（塩分）
#define ADS_CH_PH 1         // A1: SEN0161-V2（pH）

Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
MS5837 depth_sensor;
Adafruit_ADS1115 ads;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

bool ds18b20_ok = false;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(1000);

  Wire.begin(I2C_SDA, I2C_SCL);

  // --- TSL2591（光量） ---
  if (!tsl.begin()) {
    Serial.println("TSL2591が見つかりません");
    while (1);
  }
  // tsl.setGain(TSL2591_GAIN_MED);   // 室内用
  tsl.setGain(TSL2591_GAIN_LOW);      // 屋外用（晴天 65,000 Lux でも飽和せず / 2026-07-15）
  tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);

  // --- Bar02（深度） ---
  depth_sensor.setModel(MS5837::MS5837_02BA);
  if (!depth_sensor.init()) {
    Serial.println("Bar02が見つかりません");
    while (1);
  }
  depth_sensor.setFluidDensity(997);  // 淡水: 997, 海水: 1029

  // --- ADS1115（外部ADC） ---
  if (!ads.begin()) {
    Serial.println("ADS1115が見つかりません");
    while (1);
  }

  // --- DS18B20（水温） ---
  // 外付け 4.7kΩ プルアップ済み。内蔵プルアップは保険として併用。
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  ds18b20.begin();
  int ds_count = ds18b20.getDeviceCount();
  Serial.print("検出したDS18B20の数: ");
  Serial.println(ds_count);
  if (ds_count > 0) {
    ds18b20_ok = true;
    ds18b20.setWaitForConversion(true);
    ds18b20.setResolution(12);
  } else {
    // DS18B20 が無くても他のセンサーは動かす（切り分けしやすくするため）
    Serial.println("DS18B20が見つかりません（水温は NaN になります）");
  }

  Serial.println("全センサー準備完了");
  Serial.println("光量[Lux] / 水温[C] / 深度[m] / 基板温度[C] / 塩分電圧[V] / pH電圧[V]");
}

void loop() {
  // --- 光量 ---
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir = lum >> 16;
  uint16_t full = lum & 0xFFFF;
  float lux = tsl.calculateLux(full, ir);

  // --- 深度・基板温度 ---
  depth_sensor.read();
  float depth = depth_sensor.depth();
  float board_temp = depth_sensor.temperature();  // Bar02 内蔵（圧力補正用）

  // --- 水温（DS18B20） ---
  float water_temp = NAN;
  if (ds18b20_ok) {
    ds18b20.requestTemperatures();
    float t = ds18b20.getTempCByIndex(0);
    // 断線・読み取り失敗時は -127 が返る。そのままログに入れると解析時に事故るので弾く。
    if (t != DEVICE_DISCONNECTED_C && t > -50.0 && t < 100.0) {
      water_temp = t;
    }
  }

  // --- 塩分電圧（ADS1115 A0） ---
  int16_t raw_sal = ads.readADC_SingleEnded(ADS_CH_SALINITY);
  float salinity_voltage = ads.computeVolts(raw_sal);

  // --- pH電圧（ADS1115 A1） ---
  int16_t raw_ph = ads.readADC_SingleEnded(ADS_CH_PH);
  float ph_voltage = ads.computeVolts(raw_ph);

  // --- 表示 ---
  Serial.print("光量: ");
  Serial.print(lux);
  Serial.print(" Lux  水温: ");
  if (isnan(water_temp)) {
    Serial.print("NaN");
  } else {
    Serial.print(water_temp);
  }
  Serial.print(" C  深度: ");
  Serial.print(depth);
  Serial.print(" m  基板温度: ");
  Serial.print(board_temp);
  Serial.print(" C  塩分電圧: ");
  Serial.print(salinity_voltage, 4);
  Serial.print(" V  pH電圧: ");
  Serial.print(ph_voltage, 4);
  Serial.println(" V");

  delay(1000);
}
