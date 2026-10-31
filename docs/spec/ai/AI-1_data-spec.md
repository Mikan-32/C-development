# AI-1 睡眠データの入力仕様

- 状態：提案（ADR-0005・0007 と同時にレビュー）
- 使う人：`sensing/`（作る側）、`session/`・`data/`（保存する側）、`analysis/`（読む側）
- PRD：4.1、5.2

## 1. データの流れ

```
加速度 50 Hz ─┐
マイク 16 kHz ─┼─ sensing/（メモリ上で 1 秒ごとの値 → 30 秒ごとの特徴量とイベント）
照度（変化時）─┘            │ コールバック
                          session/ ── data/（SQLCipher） ── analysis/ ── feedback/
```

生の値は `sensing/` の外に出さない。`session/` 以降が扱うのは、下の「エポック」と「イベント」だけ。

## 2. 入力（センサー）

| 入力 | 取り方 | 単位 | 必須 | 取れないとき |
|---|---|---|---|---|
| 加速度 | `ASENSOR_TYPE_ACCELEROMETER`、間隔 20,000 µs、まとめ受け取り最大 10 秒 | m/s² | 必須 | 計測を始めない（Q02） |
| マイク | AAudio 入力、16,000 Hz、モノラル、16bit、入力の種類は音声認識用（加工が少ない） | dBFS に変換 | 任意 | いびき・呼吸音なしで計測を続ける（Q01、Q08） |
| 照度 | `ASENSOR_TYPE_LIGHT`、変化時 | lx | 任意 | 照度なしで続ける |
| 電池 | 開始時と終了時に `SDL_GetPowerInfo` | %、充電中か | 任意 | 空欄 |

## 3. 1 秒ごとの値（保存しない）

| 名前 | 内容 |
|---|---|
| `acc_hp` | 重力成分（0.5 秒の移動平均）を引いた加速度の大きさの平均（mg） |
| `gravity` | 重力の向き（単位ベクトル）。姿勢の変化の判定に使う |
| `moving` | `acc_hp` が体動しきい値（既定 15 mg）を超えたら真 |
| `snd_db` | 音の大きさ（dBFS） |
| `snore_like` | いびきらしさ 0〜1（100〜1,000 Hz の帯域の比率と、2〜6 秒周期の繰り返しから求める） |
| `breath` | 呼吸の周期の候補（加速度の細かな揺れ、または呼吸音の包絡線） |

## 4. エポック（30 秒ごと・保存する）

テーブル `epoch`。1 夜で最大 1,680 行（14 時間）。

| 列 | 型 | 単位・範囲 | 欠損 | 説明 |
|---|---|---|---|---|
| `session_id` | INTEGER | | 不可 | |
| `idx` | INTEGER | 0〜 | 不可 | 計測開始からのエポック番号 |
| `acc_cov` | REAL | 0〜1 | 不可 | 加速度の受け取れた割合（受け取った数 ÷ 1,500） |
| `mic_cov` | REAL | 0〜1 | 不可 | マイクの受け取れた割合。マイクを使わない夜は 0 |
| `move_n` | INTEGER | 0〜30 | NULL | `moving` が真だった秒数 |
| `move_mg` | REAL | mg | NULL | `acc_hp` の平均 |
| `micro_mg` | REAL | mg | NULL | 体動がない秒の `acc_hp` の中央値（呼吸や鼓動の細かな揺れ。ベッドの上にあるかの判定に使う） |
| `posture` | INTEGER | 0/1 | NULL | 前のエポックから重力の向きが 30° 以上変わったら 1（寝返りの候補） |
| `snd_db_mean` | REAL | dBFS | NULL | |
| `snd_db_p95` | REAL | dBFS | NULL | |
| `snore_s` | INTEGER | 0〜30 | NULL | `snore_like` が 0.6 以上だった秒数 |
| `breath_bpm` | REAL | 回/分 | NULL | 呼吸数の推定値 |
| `breath_q` | REAL | 0〜1 | NULL | 呼吸数の確からしさ |
| `lux` | REAL | lx | NULL | 照度の中央値 |
| `flags` | INTEGER | ビット | 不可 | 下の表 |

`flags` のビット：

| ビット | 名前 | 意味 |
|---|---|---|
| 0 | `NK_EPF_GAP_ACC` | `acc_cov` < 0.8 |
| 1 | `NK_EPF_GAP_MIC` | `mic_cov` < 0.8（マイクを使う夜だけ） |
| 2 | `NK_EPF_OFFBED` | `micro_mg` が 0.5 mg 未満（ベッドの上にない疑い） |
| 3 | `NK_EPF_MIC_BUSY` | 通話など、ほかのアプリがマイクを使っていた |
| 4 | `NK_EPF_CHARGING` | 充電中 |
| 5 | `NK_EPF_SOUND_ON` | 入眠サウンドを再生中（自分の音をいびきと誤認しないため、いびき判定から除く） |

## 5. イベント（保存する）

テーブル `event`。

| `type` | 内容 | `value` | `reason` |
|---|---|---|---|
| `TURN` | 寝返り（姿勢の変化） | 変化した角度 | — |
| `SNORE` | いびきの続いた区間（10 秒以上） | 平均の大きさ dBFS | — |
| `NOISE` | 大きな音（周囲より 15 dB 以上大きい） | 大きさ dBFS | — |
| `GAP_START` / `GAP_END` | 取得できなかった区間 | — | 理由コード（AI-2 の表） |
| `SOUND_START` / `SOUND_STOP` | 入眠サウンド | 音源の番号 | — |
| `CURTAIN` | カーテンの命令と結果 | 目標位置 | 結果（成功・失敗・未確認） |
| `ALARM` | 鳴動・スヌーズ・停止 | スヌーズ回数 | — |
| `USER` | 「起きた」「計測を止めた」 | — | — |

列：`session_id`、`t_ms`（UTC ミリ秒）、`dur_ms`、`type`、`value`、`reason`。

## 6. セッション（1 夜・保存する）

テーブル `session`。

| 列 | 説明 |
|---|---|
| `id`、`start_ms`、`end_ms`、`tz_min` | 開始・終了（UTC ミリ秒）と、開始時のタイムゾーンのずれ（分） |
| `night_date` | 起床日（`end_ms` の現地日付、`YYYY-MM-DD`） |
| `end_reason` | `USER_STOP`、`ALARM_STOP`、`TIMEOUT_14H`、`SERVICE_KILLED`、`BATTERY_LOW`、`ERROR` |
| `target_sleep_min` | その夜の目標睡眠時間（既定 420 分） |
| `alarm_ms` | その夜のアラーム時刻（なければ NULL） |
| `mic_used`、`light_used` | 使ったセンサー |
| `placement` | 試し計測の結果 `OK`／`WARN`／`SKIP` |
| `battery_start`、`battery_end` | % |
| `app_ver`、`algo_ver`、`device_model` | 版と端末 |

## 7. 夜ごとの分析結果（保存する）

テーブル `night_result`。中身は AI-2〜AI-4 で定める。

| 列 | 説明 |
|---|---|
| `session_id`、`algo_ver`、`computed_ms` | |
| `quality` | `OK`／`INSUFFICIENT`／`FAILED`（AI-2） |
| `reasons` | 理由コードのビット列（AI-2） |
| `confidence` | `HIGH`／`MED`／`LOW`（AI-3） |
| `tib_min`、`tst_min`、`sol_min`、`waso_min`、`awakenings`、`se` | 寝床にいた時間、睡眠時間、寝つき、中途覚醒の合計、中途覚醒の回数、睡眠効率 |
| `onset_ms`、`final_wake_ms` | 寝ついた時刻、最後に目覚めた時刻 |
| `deep_min`、`light_min`、`wake_min`、`unknown_min` | 段階ごとの時間 |
| `turns`、`snore_min`、`breath_bpm` | 寝返りの回数、いびきの時間、呼吸数の中央値（取れない夜は NULL） |
| `score`、`score_parts` | 点数（出さない夜は NULL）と、項目ごとの点数（JSON） |
| `hypnogram` | 段階の並び（1 エポック 1 バイト：0 不明、1 覚醒、2 浅い、3 深い） |
| `mood` | 任意の目覚めの気分（1〜3、未入力は NULL）。スコアには使わない |

## 8. 設定・同意など（保存する）

| テーブル | 主な列 |
|---|---|
| `consent` | 同意文の版、同意した項目（計測、マイク、録音の保存）、日時、取り消し日時 |
| `settings` | 目標睡眠時間、起きる時刻（平日・休日）、文字サイズ、サウンドの既定 |
| `curtain` | デバイス ID、名前、トークンとシークレット（DB ごと暗号化）、開閉の予約 |
| `automode` | 有効か、開ける何分前か、スキップの予定、一時停止の理由 |
| `alarm` | 時刻、スヌーズの長さと回数、音 |
| `feedback` | 日付、ルール ID、差し込み値（JSON）、表示した日時、「反映した」「閉じた」 |
| `snore_clip` | 任意の録音（10 秒、Ogg）、作成日時。7 日で削除 |

## 9. 欠損値のルール

1. 欠けた値は **NULL** にし、0 や前の値で埋めない。
2. `acc_cov` < 0.8 のエポックは、段階を「不明」とする（AI-3）。
3. 欠損が 2 エポック（1 分）以上続いたら、`GAP_START`／`GAP_END` のイベントを理由付きで残す。
4. 合計の時間（睡眠時間など）は、取れたエポックだけで数え、「取得できた時間」を必ず一緒に表示する。

## 10. PC で使うときの形式

再生テスト（ADR-0011 の段 3）と評価（AI-7）では、上のエポックを 1 行ずつ並べた JSON を使う（`tests/golden/analysis/*.input.json`）。列名はこの表と同じにする。
