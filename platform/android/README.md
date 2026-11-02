# platform/android/ — Android のプラットフォーム層（Kotlin）

Android の OS に登録する処理だけを Kotlin で書いたものです（[ADR-0001](../../docs/adr/0001-c-core-and-kotlin-platform-layer.md)、[ADR-0004](../../docs/adr/0004-sdl3-first-platform-boundary.md)）。判断（いつ鳴らすか、スヌーズできるか、カーテンを開けるか）は書かず、すべて C に任せます。C 側の受け口（`platform/android/c/`）はまだありません。

パッケージ名は ADR-0012 の案（`io.github.mikan32.nemunoki`）で書いています。ADR で別の名前に決まったら、フォルダとパッケージ名と、下の JNI の関数名を直します。

## 中身

| ファイル | 役割 | 主な Issue |
|---|---|---|
| `kotlin/…/NativeBridge.kt` | C との窓口（JNI）。定数の定義もここ | #37 |
| `kotlin/…/MainActivity.kt` | SDLActivity を継承。起動理由を C に伝え、アラームのときはロック画面の上に表示 | #37、#33 |
| `kotlin/…/Scheduler.kt` | AlarmManager への登録・取り消し。再起動後のために予約を保存 | #37、#33、#31 |
| `kotlin/…/ScheduleReceiver.kt` | 予約時刻の到来と通知のボタンを受け、C のヘッドレス入口を呼ぶ | #37、#33 |
| `kotlin/…/BootReceiver.kt` | 再起動・更新・許可の回復のあとに予約を登録し直す | #37 |
| `kotlin/…/Notifications.kt` | 通知チャネル、鳴動の通知、お知らせ、常駐通知 | #37、#33 |
| `kotlin/…/NightService.kt` | 夜間の常駐（フォアグラウンドサービス）と WakeLock | #38、#27 |
| `kotlin/…/KeyVault.kt` | DB の暗号化キーを Android Keystore で守る | #28 |
| `kotlin/…/SystemSettings.kt` | 正確なアラーム・全画面通知などの許可の確認と、設定画面を開く処理 | #37 |
| `AndroidManifest.xml` | 権限、サービス、受信クラスの宣言 | — |
| `res/` | 通知のアイコン、チャネル名とボタンの文字、バックアップから外す設定 | — |

## Gradle への組み込み

`android/` に Android Studio のプロジェクトを作ったあと（[android/README.md](../../android/README.md)）、`android/app/build.gradle.kts` の `android { … }` に次を入れます。

```kotlin
android {
    namespace = "io.github.mikan32.nemunoki"
    compileSdk = 36

    defaultConfig {
        applicationId = "io.github.mikan32.nemunoki"
        minSdk = 26
        targetSdk = 36
        resValue("string", "app_name", "ねむのき")
        ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
    }
    buildTypes {
        debug {
            applicationIdSuffix = ".debug"
            resValue("string", "app_name", "ねむのき（開発）")
        }
    }
    buildFeatures { resValues = true }

    // C（リポジトリ直下の CMakeLists.txt）。共有ライブラリ「SDL3」と「main」を作ること
    externalNativeBuild { cmake { path = file("../../CMakeLists.txt") } }

    // このフォルダをアプリのソースとして読み込む
    sourceSets["main"].apply {
        java.srcDirs("../../platform/android/kotlin")
        res.srcDirs("../../platform/android/res")
        manifest.srcFile("../../platform/android/AndroidManifest.xml")
    }
}
```

- SDL の Java 部分（`org/libsdl/app/`）は、android/README.md の手順 2 のとおり、SDL 本体と同じ版（release-3.4.18）のものを `android/app/src/main/java/` にコピーします。
- AndroidX は使っていません（Android の標準の API だけ）。依存を増やさないためです。

## JNI の約束

関数名と型は、コンパイルしたクラスファイルから取り出して確かめたものです。

### Kotlin → C（C 側で実装する関数）

| C の関数 | 呼ばれるスレッド | 守ること |
|---|---|---|
| `void Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnScheduled(JNIEnv *env, jclass cls, jint id, jint kind, jint timeout_ms)` | 作業スレッド | 予約時刻の到来（ヘッドレス入口）。`timeout_ms`（20 秒）以内に戻る |
| `void Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnLaunch(JNIEnv *env, jclass cls, jint reason, jint id)` | UI スレッド | 起動理由。値を保存してすぐ戻る（SDL の画面がまだ準備中のことがある） |
| `void Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnNotificationAction(JNIEnv *env, jclass cls, jint action, jint id, jint timeout_ms)` | 作業スレッド | 通知のボタン。`timeout_ms` 以内に戻る |
| `void Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnNightService(JNIEnv *env, jclass cls, jint state)` | UI スレッド | 夜間サービスの状態。すぐ戻る（計測は C の自分のスレッドで始める） |

### C → Kotlin（C から呼べるメソッド）

クラスは `io/github/mikan32/nemunoki/platform/NativeBridge`、すべて `static` です（`GetStaticMethodID` と `CallStatic…Method` で呼ぶ）。

| メソッド | JNI の型 | 中身 | nk_platform.h（platform-boundary.md の案） |
|---|---|---|---|
| `capability(cap)` | `(I)Z` | 許可や設定の状態 | `nk_platform_capability` |
| `openSettings(page)` | `(I)Z` | 設定画面を開く | `nk_platform_open_settings` |
| `nightServiceStart(flags, title, body)` | `(ILjava/lang/String;Ljava/lang/String;)Z` | 夜間サービスを始める（画面の操作の直後に呼ぶ） | `nk_platform_night_service_start` |
| `nightServiceStop()` | `()V` | 夜間サービスを止める | `nk_platform_night_service_stop` |
| `schedule(id, atMs, kind, flags, timeoutMs, title, body)` | `(IJIIILjava/lang/String;Ljava/lang/String;)Z` | 予約（同じ id は上書き）。正確な時刻で登録できたら true | `nk_platform_schedule` |
| `cancel(id)` | `(I)V` | 予約の取り消し | `nk_platform_cancel` |
| `alarmRingingStop(id)` | `(I)V` | 鳴動を止める（通知を消して音を止める） | `nk_platform_alarm_ringing_stop` |
| `postNotice(kind, id, title, body)` | `(IILjava/lang/String;Ljava/lang/String;)V` | お知らせの通知 | `nk_platform_notify` |
| `dbKey()` | `()[B` | DB のキー（32 バイト）。取り出せなければ null | `nk_platform_db_key` |
| `dbKeyReset()` | `()V` | キーを破棄する（DB を作り直すとき） | （追加） |

`nk_platform_request_permission`、`nk_platform_toast`、`nk_platform_open_url` は、C から SDL3 の API を直接呼ぶので Kotlin にはありません（ADR-0004）。

### 定数（C 側の列挙を同じ値にする）

| 種類 | 名前と値 |
|---|---|
| 予約の種類 `kind` | `SCHED_ALARM`=0、`SCHED_CURTAIN_OPEN`=1、`SCHED_CURTAIN_CLOSE`=2、`SCHED_REMINDER`=3 |
| 予約のフラグ `flags` | `SCHED_FLAG_SNOOZE`=1（鳴動の通知に「スヌーズ」を出す） |
| 能力 `cap` | `CAP_EXACT_ALARM`=0、`CAP_FULL_SCREEN_INTENT`=1、`CAP_NOTIFICATIONS`=2、`CAP_IGNORING_BATTERY_OPTIMIZATIONS`=3 |
| 設定画面 `page` | `PAGE_APP_DETAILS`=0、`PAGE_EXACT_ALARM`=1、`PAGE_FULL_SCREEN_INTENT`=2、`PAGE_NOTIFICATIONS`=3、`PAGE_BATTERY_OPTIMIZATION`=4 |
| 夜間サービス `flags` | `NIGHT_FLAG_MIC`=1、`NIGHT_FLAG_PLAYBACK`=2 |
| 夜間サービス `state` | `NIGHT_STARTED`=0、`NIGHT_STOPPED`=1、`NIGHT_FAILED`=2 |
| 起動理由 `reason` | `LAUNCH_NORMAL`=0、`LAUNCH_ALARM`=1、`LAUNCH_NOTICE`=2、`LAUNCH_TRACKING`=3 |
| 通知のボタン `action` | `ACT_SNOOZE`=0、`ACT_STOP_ALARM`=1、`ACT_STOP_TRACKING`=2、`ACT_AUTOMODE_SKIP`=3、`ACT_AUTOMODE_OFF`=4 |
| お知らせ `kind` | `NOTICE_INFO`=0、`NOTICE_AUTOMODE`=1（スキップ・停止のボタン付き）、`NOTICE_REMINDER`=2 |

## C 側で守ること

1. **クラスはロード時に覚えておく**：C のスレッドから `FindClass` を呼ぶとアプリのクラスが見つからない。`libmain` の `JNI_OnLoad` で `JavaVM`、`NativeBridge` クラスのグローバル参照、各メソッドの ID を保存しておく。
2. **C のスレッドから呼ぶときは JVM につなぐ**：`AttachCurrentThread`（SDL のスレッドなら `SDL_GetAndroidJNIEnv()`）。呼んだあとは `ExceptionCheck` で確かめる（Kotlin 側でも例外は止めている）。
3. **文字列**：`NewStringUTF` は「修正 UTF-8」なので、絵文字（サロゲートペア）を含めない。日本語は問題ない。
4. **ヘッドレス入口では画面に触れない**：`nativeOnScheduled` と `nativeOnNotificationAction` は、SDL の画面が動いていないプロセスから呼ばれることがある（platform-boundary.md の 5 章）。
5. **端末のロックを一度も解除していない間**：再起動後に解除する前にアラームの時刻が来た場合も、鳴動の通知（音とボタン）は出る。このとき C が呼ばれても DB は開けない（暗号化された領域がまだ使えない）ので、待たずにすぐ戻る。鳴動画面（MainActivity）はロック解除前には開かない。
6. **キーは使い終わったら消す**：`dbKey()` で受け取ったバイト列は、SQLCipher に渡したら `memset` で消す。

## 設計（platform-boundary.md）から変えたところ

- 通知チャネルの名前と、通知のボタンの文字（「スヌーズ」「止める」など）は `res/values/strings.xml` に置いた。アプリが終了している状態で鳴動の通知を出すときに、C を待たずに使えるようにするため。通知のタイトルと本文は、これまでどおり C から渡す。ADR-0004 の 5 番（文面は C が持つ）の例外として、レビューで確認してほしい。
- `nk_platform_notify` に当たるメソッドの名前は `postNotice` にした（Java の `Object.notify` と紛らわしいため）。
- `schedule` に `flags`（スヌーズを出すか）、`timeoutMs`（鳴動を止めるまでの時間）、通知の文面を加えた。アプリが終了していても、時刻が来たら Kotlin だけで鳴動の通知を出せるようにするため。
- `dbKeyReset` を加えた。

## 確かめたこと・まだ確かめていないこと

- 確かめたこと：Kotlin 2.1.20 のコンパイラで、API 36 の `android.jar`（AOSP から作られたもの）と SDL release-3.4.18 の Java のソースに対してコンパイルし、エラーと警告が 0 件であることを確認した（`R` クラスは代用品を使用）。JNI の関数名と型は、できたクラスファイルから確認した。
- まだ確かめていないこと：Gradle（Android Gradle Plugin）でのビルド（リソースとマニフェストの統合、lint、ktlint）と、実機・エミュレーターでの動作。C 側がまだないため、アプリとしては動かない。
- 実機での確認は、手順書 [TM-01](../../tests/manual/TM-01_alarm.md)（アラーム）、[TM-02](../../tests/manual/TM-02_overnight.md)（一晩の計測）、[TM-03](../../tests/manual/TM-03_background-sound.md)（画面オフのサウンド）で行う。

## レビューで確認してほしいこと

- アラーム音は、今は端末の「アラームの既定の音」を使っている。ADR-0010 の「同梱の 3 種類から選ぶ」にするには、音ごとに通知チャネルを分ける必要がある（チャネルの音は、作ったあとに変えられないため）。
- 再起動で時刻を過ぎたアラームは、10 分以内ならすぐ鳴らし、それより古いものは捨てる。この扱いでよいか。
- 正確なアラームの許可は、Android 14 以降は初期状態でオフになる。初回設定（UX-1 の ON-06）で許可してもらう前提。
- Google Play に出す場合は、全画面通知、正確なアラーム、フォアグラウンドサービスの種類（health、microphone、mediaPlayback）の申告が必要。
