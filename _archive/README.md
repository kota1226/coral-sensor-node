# 退避したスケッチ

2026-07-15 に整理。以下は重複のため退避したもの。**削除していないので、必要なら `../` に戻せる。**

| フォルダ | 中身 | 退避した理由 |
| --- | --- | --- |
| `sketch_jul8a` | I²C スキャナ | `I2C_scanner` とほぼ同一 |
| `sketch_jul8b` | Bar02 単体 | `water_depth` と完全に同一 |
| `water_depth` | Bar02 単体 | `depth_sensor` と機能が重複 |

`sketch_jul8a` / `sketch_jul8b` は Arduino IDE の自動命名（7月8日作成）。
後から名前を付けて作り直したものが本体。

## 現役のスケッチ（`../` にある）

| フォルダ | 用途 |
| --- | --- |
| `sensor_node` | **統合コード（本番用）** 光量/水温/深度/塩分/pH |
| `test_sensor` | 旧統合コード（pH・DS18B20 なし）。動作実績があるので保持 |
| `I2C_scanner` | I²C アドレス確認 |
| `depth_sensor` | Bar02 単体 |
| `tsl2591_light_sensor` | TSL2591 単体（Adafruit サンプルほぼそのまま） |
