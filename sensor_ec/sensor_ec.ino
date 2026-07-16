/*
 * ATOM S3 マルチセンサー + 塩分EC + DS18B20水温 + pH
 *
 * 主な変更点:
 *  ...
 *  - pH（SEN0161-V2）を ADS1115 A1 で読み、pH電圧(V) を表示
 *  - DS18B20 の -127℃ ガードを実装（読む→チェック→使う の順）
 *
 * pH について:
 *  - この版は pH電圧(V) を出すだけ。pH値への換算は校正後。
 *  - 校正は 6.86 と 9.18 の2点（海水 pH 7.5〜8.5 を挟むため）。4.00 は使わない。
 *  - プローブは初回使用前に 3N KCl 保存液に 8時間浸ける（公式仕様）
 *
 * 配線:
 *  DS18B20 : 赤(VCC)→3V3 / 黒(GND)→GND / 黄(DATA)→G8  ※外付け4.7kΩ済み
 *  pH      : -(GND)→GND / +(VCC)→5V / A(信号)→ADS1115 A1 / BNC→pHプローブ
 
  * 校正手順（12.88mS/cm 標準液を使う1点校正）:
 *  1. プローブを蒸留水で洗い、紙で水気を拭く（白金黒層に触れない）
 *  2. 12.88mS/cm の標準液にプローブを入れ、軽く揺らして値が安定するまで待つ
 *  3. シリアルモニタに  ENTEREC  と入力（校正モードに入る）
 *  4. 続けて  CALEC  と入力（12.88mS/cm を自動認識して校正）
 *  5. 続けて  EXITEC  と入力（校正値をEEPROMに保存して終了）
 *     ※ EXITEC を入力しないと校正値は保存されない
 *     ※ ESP32はEEPROM保存に既知の問題あり...
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"
#include "MS5837.h"
#include <Adafruit_ADS1X15.h>
#include "DFRobot_EC10.h"
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>

Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
MS5837 depth_sensor;
Adafruit_ADS1115 ads;
DFRobot_EC10 ec;

#define ONE_WIRE_BUS 8
#define ADS_CH_EC 0   // A0: DFR0300-H（塩分）
#define ADS_CH_PH 1   // A1: SEN0161-V2（pH）
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
bool tempValid = false;
float voltage_mV = 0;   // ADS1115で読んだ電圧をmVに直したもの
float ecValue = 0;      // 換算したEC値 (mS/cm)
float waterTemp = 25.0;   // DS18B20が読んだ水温（EC補正に使う）

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(1000);

  Wire.begin(6, 5);

  // ESP32系ではEEPROMを使う前にbegin()が必須（校正値の保存領域を確保）
  EEPROM.begin(32);

  if (!tsl.begin()) {
    Serial.println("TSL2591が見つかりません");
    while (1);
  }
  tsl.setGain(TSL2591_GAIN_LOW);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);

  depth_sensor.setModel(MS5837::MS5837_02BA);
  if (!depth_sensor.init()) {
    Serial.println("Bar02が見つかりません");
    while (1);
  }
  depth_sensor.setFluidDensity(997); // ※本番（海水）では 1029 に変更

  if (!ads.begin()) {
    Serial.println("ADS1115が見つかりません");
    while (1);
  }

  ec.begin(); // EEPROMから校正済みK値を読み込む（未校正ならデフォルト値）

  // ★DS18B20の初期化（この順序が重要）
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);  // begin()の前にプルアップ有効化
  ds18b20.begin();
  ds18b20.setWaitForConversion(true);   // 変換完了を待ってから読む（-127℃対策）
  ds18b20.setResolution(12);            // 12ビット（0.0625℃刻み）
  Serial.print("検出したDS18B20の数: ");
  Serial.println(ds18b20.getDeviceCount());
}

void loop() {

  // ---- 1. DS18B20 を読む ----
  ds18b20.requestTemperatures();
  float t = ds18b20.getTempCByIndex(0);

  // ---- 2. チェックする（使う前に！）----
  if (t != DEVICE_DISCONNECTED_C && t > -50.0 && t < 100.0) {
    waterTemp = t;      // 正常なときだけ更新
    tempValid = true;
  } else {
    tempValid = false;  // 異常なら waterTemp は前回値のまま
  }

  // ---- 3. 光量 ----
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir = lum >> 16;
  uint16_t full = lum & 0xFFFF;
  float lux = tsl.calculateLux(full, ir);

  // ---- 4. 深度・Bar02温度 ----
  depth_sensor.read();
  float depth = depth_sensor.depth();
  float bar02Temp = depth_sensor.temperature();

  // ---- 5. 塩分（A0）----
  int16_t ads_raw_ec = ads.readADC_SingleEnded(ADS_CH_EC);
  float salinity_voltage_V = ads.computeVolts(ads_raw_ec);

  // ---- 6. pH（A1）----
  int16_t ads_raw_ph = ads.readADC_SingleEnded(ADS_CH_PH);
  float ph_voltage_V = ads.computeVolts(ads_raw_ph);

  // ---- 7. EC に換算（チェック済みの waterTemp を使う）----
  voltage_mV = salinity_voltage_V * 1000.0;
  ecValue = ec.readEC(voltage_mV, waterTemp);

  // ---- 8. 表示 ----
  Serial.print("光量: ");
  Serial.print(lux);
  Serial.print(" Lux  DS18B20水温: ");
  Serial.print(waterTemp);
  if (!tempValid) Serial.print("(古い値)");
  Serial.print(" C  Bar02温度: ");
  Serial.print(bar02Temp);
  Serial.print(" C  深度: ");
  Serial.print(depth);
  Serial.print(" m  塩分電圧: ");
  Serial.print(salinity_voltage_V);
  Serial.print(" V  EC: ");
  Serial.print(ecValue);
  Serial.print(" mS/cm  pH電圧: ");
  Serial.print(ph_voltage_V, 4);
  Serial.println(" V");

  // ---- 9. 校正コマンドの処理 ----
  ec.calibration(voltage_mV, waterTemp);

  delay(1000);
}
