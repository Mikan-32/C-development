# プラットフォーム境界の設計（C4 設計書 v1 の追補）

- 対応タスク：CS-1「アプリの構成を決める」（C4 設計書 v1 を補う）
- 関係 ADR：[0001](../adr/0001-c-core-and-kotlin-platform-layer.md)、[0004](../adr/0004-sdl3-first-platform-boundary.md)
- 状態：提案（レビュー後に C4 設計書の 5 章・6 章へ反映する）

C4 設計書 v1 は「どのコンポーネントがあるか」を決めた。この資料は、PRD の機能に必要な OS の機能を 1 つずつ洗い出し、**SDL3／NDK（C）で書くか、Kotlin で書くか**を決める。あわせて、画面がない状態で動く入口（ヘッドレス入口）とスレッドの約束を定める。

## 1. 機能ごとの担当

判定の順番は ADR-0004 のとおり「SDL3 → NDK → Kotlin」。

| # | 必要な OS の機能 | PRD | 担当 | 使うもの | 補足 |
|---|---|---|---|---|---|
| 1 | 画面の描画・タッチ | 全体 | C（SDL3＋LVGL） | `SDL_Renderer`、LVGL の SDL ドライバー | |
| 2 | 縦画面の固定 | 5.1 | C（SDL3） | `SDL_HINT_ORIENTATIONS="Portrait"` | |
| 3 | ノッチ・ナビゲーションバーを避ける | 5.1 | C（SDL3） | `SDL_GetWindowSafeArea` | |
| 4 | 戻るボタン | 5.1 | C（SDL3） | `SDL_HINT_ANDROID_TRAP_BACK_BUTTON`、`SDL_SCANCODE_AC_BACK` | 画面ごとの戻り先は `ui/` が決める |
| 5 | 日本語フォント・音源・画像の読み込み | 4.6 | C（SDL3） | `SDL_IOFromFile`（APK の assets を読める） | フォントは LVGL の Tiny TTF で読む |
| 6 | マイク・通知の実行時権限 | 4.1, 5.2 | C（SDL3） | `SDL_RequestAndroidPermission` | `RECORD_AUDIO`、`POST_NOTIFICATIONS` |
| 7 | 正確なアラーム・全画面通知の許可の確認と設定画面 | 4.7 | **Kotlin** | `AlarmManager.canScheduleExactAlarms`、`NotificationManager.canUseFullScreenIntent`、設定画面の Intent | 実行時権限ではないため SDL3 では扱えない |
| 8 | トースト | 5.3 | C（SDL3） | `SDL_ShowAndroidToast` | 重要な結果はトーストではなく画面に残す（UX-7） |
| 9 | URL を開く（プライバシーポリシー、トークン取得手順） | 5.2 | C（SDL3） | `SDL_OpenURL` | |
| 10 | トークンの貼り付け | 4.4 | C（SDL3） | `SDL_GetClipboardText` | 長い英数字を手で打たせない |
| 11 | 文字入力 | 4.4 | C（SDL3＋LVGL） | `SDL_StartTextInput`、LVGL の `lv_textarea` | 入力は英数字だけに限定する（UX-8） |
| 12 | 計測中に画面を消さない（任意の「時計表示」） | 4.1 | C（SDL3） | `SDL_DisableScreenSaver` | 既定は画面オフ。表示する場合も暗い配色 |
| 13 | 電池残量・充電中か | 4.1, 5.3 | C（SDL3） | `SDL_GetPowerInfo` | 計測前の注意表示に使う |
| 14 | 保存先のフォルダ | 5.2 | C（SDL3） | `SDL_GetPrefPath` | アプリ専用の内部領域 |
| 15 | 現在時刻・タイムゾーン・地域設定 | 4.3 | C（SDL3） | `SDL_GetCurrentTime`、`SDL_TimeToDateTime`、`SDL_GetPreferredLocales` | |
| 16 | 入眠サウンドの再生（画面オフ中も） | 4.6 | C（SDL3） | `SDL_AudioStream`、ヒント 2 つ（ADR-0004） | プロセスを生かすため夜間サービスを併用 |
| 17 | マイク入力（夜間） | 4.1 | C（NDK） | AAudio | `src/sensing/` だけ |
| 18 | 加速度・照度（夜間） | 4.1 | C（NDK） | ASensorManager | 同上。照度は任意（ADR-0005） |
| 19 | 夜間の常駐（画面オフでも計測を続ける） | 4.1 | **Kotlin** | フォアグラウンドサービス（種類 `microphone` と `mediaPlayback`）、部分 WakeLock | 計測そのものは C のスレッドで行う |
| 20 | アラームの登録・取り消し | 4.7 | **Kotlin** | `AlarmManager.setAlarmClock` | 時刻と ID は C が決めて渡す |
| 21 | アラーム時刻の到来の受信 | 4.7 | **Kotlin** → C | BroadcastReceiver → ヘッドレス入口 | |
| 22 | 鳴動の通知（ロック中は全画面） | 4.7 | **Kotlin** | 通知チャネル（アラーム用の音を設定）、全画面 Intent | 鳴動画面そのものは LVGL（`alarm/`） |
| 23 | アラーム音 | 4.7 | **Kotlin**（通知の音として） | アラーム用チャネルの音＋繰り返しフラグ | アプリが落ちていても鳴る。理由は 4 章 |
| 24 | ロック画面の上に鳴動画面を出す | 4.7 | **Kotlin** | `setShowWhenLocked`、`setTurnScreenOn` | SDLActivity の継承クラスで 2 行 |
| 25 | 再起動後のアラーム再登録 | 4.7 | **Kotlin** | `BOOT_COMPLETED` の受信 | Kotlin が保存した「登録済みの時刻と ID」をそのまま登録し直す。判断はしない |
| 26 | カーテンの時刻実行（アプリが裏・終了中） | 4.4, 4.5 | **Kotlin** → C | AlarmManager → ヘッドレス入口 → libcurl | クラウド API に予約機能がないため（ADR-0006） |
| 27 | カーテン API との HTTPS 通信 | 4.4 | C（libcurl＋cJSON） | | |
| 28 | データベース暗号化キーの保護 | 5.2 | **Kotlin** | Android Keystore で鍵をラップして保存 | C には生の鍵を渡すだけ |
| 29 | 結果の通知（カーテン失敗など） | 5.3 | **Kotlin** | NotificationManager | 文面は C から渡す |
| 30 | 端末バックアップから DB を外す | 5.2 | **Kotlin**（マニフェスト） | `android:allowBackup="false"` など | 鍵は Keystore にあり、復元しても読めないため |

**集計**：30 項目のうち C（SDL3・NDK・ライブラリ）が 18、Kotlin が 12。Kotlin の 12 項目は、下の 8 クラスにまとまる。

## 2. `nk_platform.h` の窓口（案）

関数名は CONTRIBUTING の命名規則に合わせる。実際のヘッダーは CS-2・CS-6・CS-8 の実装 Issue で作る。

### C → プラットフォーム層

| 関数（案） | Android の実装 | PC 版の代役 |
|---|---|---|
| `nk_platform_request_permission(perm, cb)` | **C**：`SDL_RequestAndroidPermission` | すぐ「許可」でコールバック |
| `nk_platform_capability(cap)`（正確なアラーム、全画面通知、通知） | Kotlin | すべて「可」 |
| `nk_platform_open_settings(page)` | Kotlin（設定画面の Intent） | ログに出すだけ |
| `nk_platform_toast(text)` | **C**：`SDL_ShowAndroidToast` | 標準出力 |
| `nk_platform_open_url(url)` | **C**：`SDL_OpenURL` | 同じ |
| `nk_platform_night_service_start(flags)` / `_stop()` | Kotlin（サービス起動） | 何もしない |
| `nk_platform_schedule(id, at_ms, kind)` / `_cancel(id)` | Kotlin（AlarmManager） | SDL のタイマーで呼び返す。早送り可 |
| `nk_platform_alarm_ringing_stop(id)` | Kotlin（通知を消して音を止める） | ログ |
| `nk_platform_notify(kind, title, body)` | Kotlin | 標準出力 |
| `nk_platform_db_key(buf, len)` | Kotlin（Keystore） | 開発用の固定キー |

`kind` は `NK_SCHED_ALARM`（アラーム）、`NK_SCHED_CURTAIN_OPEN`、`NK_SCHED_CURTAIN_CLOSE`、`NK_SCHED_REMINDER`（おやすみ通知）の 4 種類。

### プラットフォーム層 → C（コールバック）

| 関数（案） | 呼ばれる場面 | 画面の有無 |
|---|---|---|
| `nk_on_scheduled(id, kind)` | 予約した時刻の到来 | **なくてもよい**（ヘッドレス入口） |
| `nk_on_launch(reason, id)` | 全画面通知などでアプリが開いた | あり |
| `nk_on_notification_action(action, id)` | 通知の「スヌーズ」「止める」「自動モードを止める」 | なくてもよい |
| `nk_on_night_service(state)` | サービスの開始・停止・OS による停止 | なくてもよい |

## 3. Kotlin に残すもの

| クラス（案） | 役割 | 目安 |
|---|---|---|
| `MainActivity`（SDLActivity を継承） | 読み込むライブラリの指定、ロック画面の上に表示する設定、起動理由を C に渡す | 40 行 |
| `NightService` | フォアグラウンドサービス、部分 WakeLock、C の計測開始・停止を呼ぶ | 80 行 |
| `Scheduler` | AlarmManager への登録・取り消し、登録内容の保存（再起動用） | 80 行 |
| `ScheduleReceiver` | 時刻の到来を受け、アラームなら鳴動通知を出し、C のヘッドレス入口を呼ぶ | 80 行 |
| `BootReceiver` | 保存してある登録内容で再登録する | 30 行 |
| `KeyVault` | Keystore で DB キーを作成・ラップ・取り出し | 60 行 |
| `SystemSettings` | 能力の確認と設定画面を開く Intent | 50 行 |
| `NativeBridge` | JNI の `external fun` 宣言と、C から呼ばれる静的メソッド | 50 行 |

合計 470 行前後。Kotlin 側に **判断（いつ鳴らすか、スヌーズできるか、カーテンを開けるか）を書かない** ことをレビューで確認する（CODEOWNERS）。

## 4. 判断の理由（迷いやすい 3 点）

**アラーム音を SDL3 で鳴らさない理由**：アラームは、アプリのプロセスが OS に終了させられていても鳴らなければならない。そのときは SDL がまだ起動していない。さらに端末を操作中だと、全画面通知はアプリを開かずに上部の通知として出るだけなので、SDL の画面が出ないまま音を鳴らす必要がある。通知チャネルにアラーム用の音を設定すれば、Kotlin 側は通知を出すだけで済み、音を鳴らすコードを書かずに済む。鳴動画面（止める・スヌーズ）は LVGL で作る。

**入眠サウンドは SDL3 で鳴らす理由**：ユーザーが画面の前で再生を始めるので、SDL は必ず起動している。ADR-0004 のヒント 2 つで裏に回っても止まらない。プロセスが終了させられないよう、再生中は夜間サービス（種類 `mediaPlayback`）を動かす。

**カーテンの時刻実行に Kotlin が要る理由**：SwitchBot のクラウド API には、端末側に予約を登録する機能が見当たらない（ADR-0006）。予約はアプリが持ち、時刻が来たらアプリが命令を送るしかない。アプリが終了していても時刻に起こしてもらうには AlarmManager が必要になる。

## 5. ヘッドレス入口

画面（SDL のウィンドウと LVGL）がない状態でも動く C の入口。夜間サービスと ScheduleReceiver から呼ばれる。

```
nk_core_init(NK_CORE_HEADLESS)   … データアクセス、カーテン API、自動モード、アラーム判断だけを初期化
   ├─ nk_on_scheduled(id, ALARM)          → alarm：鳴動を記録／automode：カーテンを開ける
   ├─ nk_on_scheduled(id, CURTAIN_OPEN)   → curtain：命令→状態確認→結果を保存・通知
   └─ nk_on_night_service(STARTED)        → session：計測スレッドを開始
```

- `nk_core_init` は 2 回呼ばれても安全にする（画面ありの起動と、ヘッドレスの起動が重なることがある）。
- ヘッドレス入口では `ui/` と `sound/` に触れない。CMake では、ヘッドレスで使うコンポーネントが `ui` に依存していないことをリンクで確かめる。
- 受信から処理完了までの時間の目安は 20 秒以内（カーテンの命令 1 回＋状態確認）。超える場合は結果を「未確認」として保存し、次にアプリを開いたときに表示する。

## 6. スレッドの約束

| スレッド | 持ち主 | してよいこと |
|---|---|---|
| UI スレッド（SDL のメイン） | SDL | LVGL の呼び出しはここだけ。描画は前面にいるときだけ行う |
| 計測スレッド | `sensing/`（C） | NDK のセンサー・マイク。結果はコールバックで `session/` に返す |
| 作業スレッド | `common/`（C） | HTTPS 通信、DB への書き込み、分析。結果は `SDL_PushEvent`（画面があるとき）か DB への保存で返す |
| Android のメインスレッド | Kotlin | JNI から C を呼ぶときは、すぐ戻る関数だけ呼ぶ。重い処理は C の作業スレッドに渡す |

- SQLCipher はシリアライズモードで開き、接続は作業スレッドが持つ。UI からの読み取りも作業スレッド経由にする。
- 裏に回ったら（`SDL_EVENT_DID_ENTER_BACKGROUND`）LVGL の描画を止め、イベント処理だけを続ける（ADR-0004 のヒントでループが止まらなくなるため、電池の無駄を防ぐ）。

## 7. C4 設計書 v1 への反映点

1. 図 1・図 4 のスマートカーテンの説明「開閉時刻の予約」を「開閉の命令と状態の取得」に直す（予約はアプリ側が持つ）。
2. 5.3 のプラットフォーム層の行に「技術：C（SDL3 の Android 用 API）＋Kotlin＋JNI」と書き、C 部分があることを明記する。
3. 6 章に「ヘッドレス入口」と「スレッドの約束」を追加する。
4. 7 章のアクセシビリティのリスクに、UX-8 の対策（アプリ内の文字サイズ設定など）を書き足す。
