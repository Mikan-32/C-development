# Android アプリのビルドと実機確認

Android Studioで開くのは、この `android/` フォルダーです。

- applicationId / namespace: `io.github.mikan32.nemunoki`
- compileSdk / targetSdk: 36、minSdk: 26
- NDK: 28.2.13676358、SDK CMake: 3.31.6
- 実機ABI: arm64-v8a
- Cの入口: `platform/android/c/android_main.c`
- Kotlin・マニフェスト・リソース: `platform/android/`

ルートに重複していた未接続の `app/` とGradle雛形は取り除きました。既存の実機アプリと同じapplicationIdを使います。計測中のAPKを更新せずに作業する場合は、別フォルダーのクローンでビルドまで行い、Runによる実機への反映は計測終了後に行ってください。

## 依存の準備（Windows）

リポジトリ直下で実行します。新しいクローンでは、最初にsubmoduleを取得します。

```bat
git submodule update --init --recursive --depth 1 third_party/SDL third_party/Unity third_party/lvgl third_party/sqlcipher
scripts\generate-sqlcipher.cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-openssl-android.ps1 -NdkRoot "%LOCALAPPDATA%\Android\Sdk\ndk\28.2.13676358" -VcpkgRoot "C:\Tools\vcpkg"
```

SQLCipher生成にはx64 Visual Studio開発者環境が必要です。OpenSSLのWindows用・Android用ライブラリが同じ固定版で準備済みなら、その `third_party/prebuilt/openssl/` は別クローンへコピーして再利用できます。CMakeがメタデータとライブラリのハッシュを確認します。SQLCipherのamalgamationは4.19.0のsubmoduleから生成し直します。

LinuxのNDKビルドでは `bash scripts/generate-sqlcipher.sh` と `bash scripts/build-openssl-android.sh "$ANDROID_NDK" arm64-v8a` を使います。

## ビルド

`CMakePresets.json` の `android-arm64` はC部分の確認用で、APKのインストールは行いません。Android StudioのGradleビルドはプリセットを使わず、`android/app/build.gradle.kts` からルートのCMakeへ設定を渡します。

実機へ反映するときは、Android Studioでこのフォルダーを開き、同期後にRunします。アプリデータの削除・アンインストールは、保存した試験記録を消すため実施しません。既存の4.20.0で作ったDBについては、4.19.0での読み出しを合成データで確認しています。実機の保存済み記録は更新後にも確認してください。

## 現在の実機確認画面

- 初回・同意版変更時は計測への同意を確認します。マイクの利用同意は含みません。
- STARTで加速度取得を開始し、STOPでは保存・DB読み直し・接続の終了を待ってからサービスを停止します。
- PRIVACYから同意の取り消し・再同意・停止済み記録の全削除を確認できます。削除は取り消せないため、必要な試験結果を先に保存してください。
- 同意を別の接続で取り消した場合も、取得側は原則1秒ごとに確認して終了します。DBの読み取り失敗時も取得を継続しません。
- Q04の復旧表示とTM02の集計ログは維持しています。

マイク・照度・分析・スコアは未接続です。PCのLVGL画面をAndroidの正式画面へ接続する作業と、TM-02の全項目の実機確認は残っています。
