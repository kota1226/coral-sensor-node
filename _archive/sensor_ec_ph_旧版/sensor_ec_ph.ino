/*
 * ATOM S3 マルチセンサー + 塩分EC測定（DFRobot_EC10使用）+ DS18B20水温 + pH
 *
 * sensor_ec.ino から派生（2026-07-15）。
 *
 * sensor_ec.ino からの変更点:
 *  - pH（SEN0161-V2）を ADS1115 の A1 に追加
 *  - DS18B20 の -127℃ ガードを実装（元コードはコメントだけで未実装だった）
 *
 * 主な機能（sensor_ec.ino から継承）:
 *  - 塩分を電圧でなく EC値(mS/cm) で表示（DFRobot_EC10使用）
 *  - ADS1115 で読んだ電圧(V)を mV に直して readEC() に渡す
 *  - ESP32 用に EEPROM.begin() を追加（校正値の保存に必須）
 *  - シリアルで校正コマンド（ENTEREC / CALEC / EXITEC）を受け付ける
 *  - DS18B20（G8）で水温を測り、EC計算・校正の温度補正に使う
 *
 * ★DS18B20を正しく動かすためのポイント（-127℃対策）:
 *  - pinMode(INPUT_PULLUP) を ds18b20.begin() の「前」に呼ぶ
 *  - setWaitForConversion(true) で変換完了を待ってから読む
 *  - ★それでも断線すれば -127 が返る。下の loop() でガードする
 *
 * EC校正手順（12.88mS/cm 標準液を使う1点校正）:
 *  1. プローブを蒸留水で洗い、紙で水気を拭く（白金黒層に触れない）
 *  2. 12.88mS/cm の標準液にプローブを入れ、軽く揺らして値が安定するまで待つ
 *  3. シリアルモニタに  ENTEREC  と入力（校正モードに入る）
 *  4. 続けて  CALEC  と入力（12.88mS/cm を自動認識して校正）
 *  5. 続けて  EXITEC  と入力（校正値をEEPROMに保存して終了）
 *     ※ EXITEC を入力しないと校正値は保存されない
 *     ※ ESP32はEEPROM保存に既知の問題あり。電源を切ると校正が消える場合があるので、
 *        本番前に「K値をコードに直接埋め込む」等の対策を検討する（今後の宿題）
 *
 * pH について:
 *  - この版では「pH電圧(V)」を出すだけ。pH値への換算はしていない。
 *  - 校正は 6.86 と 9.18 の2点で行う（海水 pH 7.5〜8.5 を挟むため）。
 *    4.00 は海水から遠く外挿になるので使わない。
 *  - 手順: 各標準液に浸けて安定した pH電圧 を記録 → 2点から直線を引く
 *      pH = a * V + b
 *      a = (9.18 - 6.86) / (V_918 - V_686)
 *      b = 6.86 - a * V_686
 *    求めた a, b を下の PH_SLOPE / PH_OFFSET に入れて PH_CALIBRATED を 1 にする。
 *  - ★プローブは初回使用前に 3N KCl 保存液に 8時間 浸けること（公式仕様）
 *
 * 配線:
 *  DS18B20 : 赤(VCC)→3V3 / 黒(GND)→GND / 黄(DATA)→G8   ※外付け4.7kΩプルアップ済み
 *  pH      : -(GND)→GND / +(VCC)→5V / A(信号)→ADS1115 A1 / BNC→pHプローブ
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
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

#define ADS_CH_EC 0   // A0: DFR0300-H（塩分）
#define ADS_CH_PH 1   // A1: SEN0161-V2（pH）

// pH の換算。校正するまでは 0 のままにして、電圧だけを見る。
#define PH_CALIBRATED 0
#define PH_SLOPE  0.0   // a
#define PH_OFFSET 0.0   // b

float voltage_mV = 0;   // ADS1115で読んだ電圧をmVに直したもの
float ecValue = 0;      // 換算したEC値 (mS/cm)
float waterTemp = 25.0; // DS18B20が読んだ水温（EC補正に使う）※初期値は常温
bool  tempValid = false;

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

  // DS18B20 水温（EC補正に使う）
  ds18b20.requestTemperatures();
  float t = ds18b20.getTempCByIndex(0);
  // 読み取り失敗（-127など）のときは前回値を使い、異常値をECに渡さない
  // ★ここが元コードでは未実装だった。-127 がそのまま ec.readEC() と
  //   ec.calibration() に渡り、EC値の破壊・EEPROMへの異常K値保存を招く。
  if (t != DEVICE_DISCONNECTED_C && t > -50.0 && t < 100.0) {
    waterTemp = t;      // 正常値なら更新
    tempValid = true;
  } else {
    tempValid = false;  // 異常なら waterTemp は前回値のまま据え置き
  }

  // 光量
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir = lum >> 16;
  uint16_t full = lum & 0xFFFF;
  float lux = tsl.calculateLux(full, ir);

  // 深度・（Bar02の）温度
  depth_sensor.read();
  float depth = depth_sensor.depth();
  float bar02Temp = depth_sensor.temperature(); // 参考表示用


  // 塩分（ADS1115のA0 → 電圧 → EC）。温度補正はDS18B20の実測を使う
  int16_t ads_raw = ads.readADC_SingleEnded(ADS_CH_EC);
  float salinity_voltage_V = ads.computeVolts(ads_raw); // ボルト(V)
  voltage_mV = salinity_voltage_V * 1000.0;             // mVに変換して渡す
  ecValue = ec.readEC(voltage_mV, waterTemp);           // 電圧(mV)+DS18B20水温 → EC(mS/cm)

  // pH（ADS1115のA1 → 電圧）
  int16_t ads_raw_ph = ads.readADC_SingleEnded(ADS_CH_PH);
  float ph_voltage_V = ads.computeVolts(ads_raw_ph);

  // 表示
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
  Serial.print(" V");
#if PH_CALIBRATED
  Serial.print("  pH: ");
  Serial.print(PH_SLOPE * ph_voltage_V + PH_OFFSET, 2);
#endif
  Serial.println();

  // 校正コマンドの処理（DS18B20の実測水温で校正する）
  ec.calibration(voltage_mV, waterTemp);

  delay(1000);
}
