# 0004. SDL3 と NDK で書けることは C で書き、Kotlin は「OS に登録するだけ」の処理に限る

- 状態：提案
- 日付：2026-10-31
- 関係：ADR-0001 の運用ルールを具体化する。詳細は [platform-boundary.md](../architecture/platform-boundary.md)

## 背景

- ADR-0001 で「OS 固有の処理だけを Kotlin に置く」と決めたが、どこまでを OS 固有とみなすかの線引きがなかった。線引きがないと、権限の要求や設定画面、通知の文面なども Kotlin 側に書かれ、Kotlin のコードが増えていく。
- SDL3 には、Android の実行時権限の要求（`SDL_RequestAndroidPermission`）、トースト、URL を開く、クリップボード、画面の点灯維持、電池残量、セーフエリア、戻るボタン、APK 内アセットの読み込みなどの C の API がある。
- SDL3 の音は、ヒント（`SDL_HINT_ANDROID_BLOCK_ON_PAUSE` と `SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO` を `"0"`）を設定すれば、アプリが裏に回っても止まらない。
- 一方、AlarmManager、フォアグラウンドサービス、通知、BroadcastReceiver、Android Keystore は SDL3 にも NDK にも C の API がない。

## 決定

1. **判定の順番**を次のとおりにする。上で書けるなら下には書かない。
   1. SDL3 の API（UI スレッドから呼ぶもの）
   2. NDK の C API（AAudio、ASensorManager など。`src/sensing/` と `platform/android/` の C 部分だけ）
   3. Kotlin（SDL3 と NDK のどちらにもない機能だけ）
2. `platform/android/` を **C 部分（`c/`）** と **Kotlin 部分（`kotlin/`）** に分ける。`nk_platform.h` の関数のうち、SDL3 で実現できるものは C 部分で実装し、Kotlin を経由しない。
3. Kotlin に置くのは次の 8 つだけとする（[一覧](../architecture/platform-boundary.md#3-kotlin-に残すもの)）。
   SDLActivity の継承クラス、夜間サービス、アラーム登録、アラーム受信、再起動時の再登録、Keystore、設定画面を開く処理、JNI の宣言。
4. **画面はすべて LVGL** で作る。同意画面、アラーム鳴動画面、設定画面、エラー表示も含め、Kotlin で Activity やダイアログを作らない。
5. **文面は C が持つ**。通知のタイトルや本文も C から引数で渡し、Kotlin に日本語の文字列を書かない（表現のルールを `feedback/` と UI の文字列表に集約するため）。
6. アプリが裏にいる・画面が消えている・プロセスが起動し直された、の 3 つの状況でも動く必要がある処理（計測、アラーム時刻の到来、カーテンの時刻実行）は、**SDL の画面がなくても動く C の入口（ヘッドレス入口）** を用意し、Kotlin はそこを呼ぶだけにする。

## 結果

- Kotlin のコードは小さなクラス 8 個程度に収まる見込み。Kotlin に不慣れなメンバーが多くても、C だけで大半の機能を作れる。
- 権限の要求や設定画面も LVGL で作るので、PC 版でも初回設定の流れを確認できる（PC 版の代役は「すぐ許可された」として動く）。
- ヘッドレス入口では LVGL と SDL の画面に触れられない。データベースとカーテン API を、画面がなくても初期化できるように作る必要がある。
- SDL3 の音を裏で鳴らし続ける設定は、実機で必ず確認する（試作 Issue「SP-2」）。うまくいかなかった場合は、入眠サウンドの出力だけを NDK の AAudio に切り替える案を、この ADR を置き換える形で検討する。
