# 技術スタック

版はすべてリポジトリで固定し、メンバー全員と CI が同じものを使います。版を上げるときは PR で変更し、このファイルも更新します。

## アプリの中身

| 役割 | 技術 | 版 | メモ |
|---|---|---|---|
| 言語（アプリ本体） | C | C11 | 全員が同じ規格、同じ警告設定でビルドする |
| 言語（プラットフォーム層） | Kotlin ＋ JNI | Android Studio に合わせる | 依存は最小限（AndroidX Core 程度）。画面系のライブラリは使わない |
| 画面・タッチ・音の再生 | SDL3 | release-3.4.18 | Android 用の Java 部分（SDLActivity）は、SDL 本体と同じ版のものを使う |
| GUI 部品 | LVGL | v9.6.0 | 非推奨 API は使わない（v10 で削除されるため） |
| データ保存 | SQLCipher | v4.19.0 | 暗号化キーは Android Keystore で守る（プラットフォーム層の担当） |
| 暗号・TLS | OpenSSL | 未定 | SQLCipher と libcurl で共用する。ビルド方法は third_party/README.md を参照 |
| 通信 | libcurl ＋ cJSON | curl-8_23_0 / v1.7.19 | スマートカーテンのクラウド API 用 |
| マイク・センサー | Android NDK（AAudio・ASensorManager） | NDK r28 以降 | 呼び出すのは `src/sensing/` だけ |
| 日本語フォント | Noto Sans JP | — | 使う文字だけにサブセットして同梱する（SIL OFL） |
| 単体テスト（C） | Unity | v2.7.0 | PC 上で実行する |
| 単体テスト（Kotlin） | JUnit | Android Studio に合わせる | |

## ビルドと開発ツール

| 項目 | 技術 | 版 | 管理方法 |
|---|---|---|---|
| C 部分のビルド | CMake ＋ Ninja | 3.31.12 / 1.13.2 | mise |
| Android 全体のビルド | Gradle（Android Gradle Plugin） | Android Studio に合わせる | Gradle Wrapper |
| PC 版のコンパイラ | clang-cl（Windows）、clang（Linux・CI） | Build Tools と Ubuntu 24.04 に付属の版 | 各自の環境と CI |
| 整形 | clang-format、ktlint | 23.1.3 / — | mise（ktlint は Gradle 側で導入予定） |
| 静的解析 | clang-tidy | 23.1.4 | mise |
| Git フック | lefthook | 2.2.2 | mise |
| エディタ | VS Code（C）、Android Studio（Kotlin・Gradle・実機デバッグ） | — | 各自 |

## Android の設定値

| 項目 | 値 | 理由 |
|---|---|---|
| targetSdk | 36 | 2026 年 8 月 31 日から、Google Play に出す新規アプリと更新は Android 16（API 36）以上が対象であることが必須になったため |
| minSdk | 26 | AAudio が使えるのが Android 8.0（API 26）からのため |
| NDK | r28 以降 | Google Play は、Android 15 以上を対象とするアプリに 16KB ページサイズ対応を求めている。NDK r28 以降は既定でこれに対応したビルドになる |

## 版を選んだ理由（補足）

- **CMake を 4 系ではなく 3.31 系にしている理由**：CMake 4 は、`cmake_minimum_required` に古い版を書いたプロジェクトを受け付けません。cJSON などがこれにあたるためです。
- **LVGL v9.6**：v9 系の最後の安定版です。v9.6 で非推奨になった API は v10 で削除されるので、最初から使わないことで v10 への移行を楽にします。
- **clang に統一している理由**：Android NDK のコンパイラが clang なので、PC 版も clang 系（Windows は clang-cl）にそろえると、「PC では通るのに Android では通らない」を減らせます。
