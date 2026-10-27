# android/ — Android アプリとして組み立てる Gradle プロジェクト

ここに Android Studio のプロジェクトを作ります。**まだ作っていません。** 最初に 1 人が作って PR を出してください。

## 作り方

1. Android Studio で **New Project → No Activity** を選びます。
   - Name：`Nemunoki`
   - Package name：チームで決めて ADR に残す（例：`jp.example.nemunoki`）
   - Save location：このフォルダ（`android/`）
   - Language：Kotlin
   - Minimum SDK：API 26
   - Build configuration language：Kotlin DSL
2. SDL の `third_party/SDL/android-project/app/src/main/java/org/libsdl/app/` を、作ったプロジェクトの同じ場所にコピーします（**SDL 本体と同じ版のもの**を使います）。
3. SDL の `android-project` を参考に、`SDLActivity` を継承したアクティビティを作ります。
4. `app/build.gradle.kts` に次の設定を入れます。
   - `compileSdk` と `targetSdk`：36
   - `minSdk`：26
   - `ndkVersion`：r28 以降（SDK Manager で入れた版）
   - `externalNativeBuild.cmake.path`：リポジトリ直下の `CMakeLists.txt`
   - `ndk.abiFilters`：`arm64-v8a`（実機）と `x86_64`（エミュレーター）
5. Kotlin のプラットフォーム層（`platform/android/`）をソースとして読み込む設定をします。

## 注意

- **Gradle は `CMakePresets.json` を使いません。** Gradle が NDK の設定を CMake に直接渡します。プリセットの `android-arm64` は、C のコードが Android 向けにビルドできるかを CI で確かめるためのものです。
- Google Play の 16KB ページサイズ要件のため、NDK は r28 以降を使います。APK を作ったら、Android Studio の APK Analyzer で .so ファイルの警告が出ていないことを確認します。
- 作ったら、`lefthook.yml` の ktlint を有効にし、CI に Android のビルドのジョブを追加します。
