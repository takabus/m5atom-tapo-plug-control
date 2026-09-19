# m5atom-tapo-plug-control

M5Stack Atom Lite / ESP8266MOD から TP-Link Tapo スマートプラグ（P105 等）を
KLAP プロトコルで直接制御するファームウェアです。

電源を入れるとプラグを ON にし、一定時間後に自動 OFF するカウントダウンタイマーを
定期的に更新し続けます。ボードの電源が落ちるとタイマーが更新されなくなるため、
プラグは自動的に OFF になります（デッドマンスイッチ動作）。

## 動作の流れ

1. WiFi に接続
2. Tapo デバイスと KLAP ハンドシェイク（認証）
3. プラグを ON
4. カウントダウン OFF タイマー（既定 180 秒）をセット
5. 以降 30 秒ごとにタイマーを再設定し続ける

いずれかの手順が失敗した場合は LED でエラーを表示し、自動的に再起動します。

## 必要なもの

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/) または PlatformIO IDE
- 対応ボード
  - M5Stack Atom Lite（ESP32） … env 名 `m5stack-atom`
  - ESP8266MOD / ESP-12E … env 名 `esp8266mod`
- TP-Link Tapo スマートプラグと Tapo アカウント

## セットアップ

### 1. 認証情報を `.env` に書く

WiFi と Tapo の認証情報はソースには含めず、ビルド時に `.env` から注入します。
`.env.example` をコピーして値を埋めてください。

```bash
cp .env.example .env
```

```ini
WIFI_SSID=your-ssid
WIFI_PASSWORD=your-password
TAPO_IP=192.168.1.170
TAPO_EMAIL=you@example.com
TAPO_PASSWORD=your-tapo-password
```

| 変数 | 内容 |
| --- | --- |
| `WIFI_SSID` | 接続先 WiFi の SSID（2.4GHz） |
| `WIFI_PASSWORD` | WiFi のパスワード |
| `TAPO_IP` | Tapo プラグの IP アドレス（固定 IP 推奨） |
| `TAPO_EMAIL` | Tapo アカウントのメールアドレス |
| `TAPO_PASSWORD` | Tapo アカウントのパスワード |

`.env` は `.gitignore` 済みでコミットされません。

ビルド時には `scripts/load_env.py`（PlatformIO の pre-build スクリプト）が `.env` を読み、
`-DWIFI_SSID="..."` のようなマクロとしてコンパイラに渡します。
**実際の環境変数が `.env` より優先される**ため、CI では `.env` を置かずに
環境変数だけで同じビルドができます。値が足りない場合はビルドログに警告が出ます。

### 2. ビルド

```bash
# M5Stack Atom Lite (ESP32)
pio run -e m5stack-atom

# ESP8266MOD
pio run -e esp8266mod
```

### 3. 書き込み

```bash
pio run -e m5stack-atom -t upload
```

ポートを明示したい場合:

```bash
pio run -e m5stack-atom -t upload --upload-port COM3
```

### 4. シリアルモニタ

```bash
pio device monitor -e m5stack-atom
```

ボーレートは 115200 です。

### その他のコマンド

```bash
pio run -t clean          # ビルド成果物を削除
pio run                   # 全 env をビルド
```

## LED ステータス（M5Stack Atom Lite のみ）

ESP8266MOD には NeoPixel がないため、LED 表示は no-op になります。

| 表示 | 状態 |
| --- | --- |
| 青点滅 | WiFi 接続中 |
| 黄点灯 | Tapo ハンドシェイク中 |
| 緑点灯 | 正常動作中 |
| 白点灯 | カウントダウン更新中 |
| 赤高速点滅 | エラー（3 秒後に再起動） |

## 設定値の変更

タイミングや GPIO は `src/config.h` で定義しています。

| 定数 | 既定値 | 内容 |
| --- | --- | --- |
| `COUNTDOWN_SECONDS` | 180 | 自動 OFF までの秒数 |
| `COUNTDOWN_REFRESH_MS` | 30000 | タイマー再設定の間隔（ms） |
| `WIFI_CONNECT_TIMEOUT` | 3000 | WiFi 接続のタイムアウト（ms） |
| `HANDSHAKE_RETRY_DELAY` | 1000 | ハンドシェイク再試行の待ち時間（ms） |
| `MAX_RETRIES` | 2 | ハンドシェイクの最大試行回数 |

## ファイル構成

```
platformio.ini        ビルド設定（2 ボード分の env）
scripts/load_env.py   .env を読み込む pre-build スクリプト
src/
  main.cpp            起動シーケンスとメインループ
  config.h            タイミング定数・GPIO 定義
  led_status.*        NeoPixel によるステータス表示（ESP8266 ではスタブ）
  tapo_device.*       プラグ操作の高レベル API（ON/OFF/カウントダウン）
  tapo_klap.*         KLAP ハンドシェイクと暗号化リクエスト送信
  tapo_cipher.*       AES-CBC + シーケンス番号による暗号化処理
```

## トラブルシューティング

- **ビルド時に `[load_env] Missing values for: ...` と出る**
  `.env` が無いか、キーの値が空です。`.env.example` を参考に埋めてください。
- **WiFi に接続できない**
  ESP32 / ESP8266 は 2.4GHz のみ対応です。5GHz の SSID には接続できません。
  `WIFI_CONNECT_TIMEOUT` が既定 3000ms と短いので、環境によっては延長してください。
- **ハンドシェイクが失敗する**
  `TAPO_IP` が現在のプラグの IP と一致しているか、Tapo アプリのアカウント情報が
  正しいかを確認してください。プラグの IP はルータ側で固定するのが確実です。
