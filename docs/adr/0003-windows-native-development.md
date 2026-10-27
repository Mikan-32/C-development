# 0003. 日常の開発は Windows ネイティブ（clang-cl）で行い、WSL2 は補助にする

- 状態：承認
- 日付：2026-10-27

## 背景

- メンバーの PC は Windows。
- Android のビルドと実機デバッグは、Windows 上の Android Studio で行うのが基本。
- PC 版を WSL2 で開発すると、1 つの作業フォルダを Windows 側と WSL2 側の両方から触ることになる。境界をまたいだファイルアクセスは遅く、どちらに置いてもどちらかが遅くなる。

## 決定

- 日常の開発は Windows ネイティブで行う。コンパイラは MSVC ではなく clang-cl にする。
- ビルドは CMake ＋ Ninja。構成は `CMakePresets.json` で共有する。
- UBSan や Valgrind を使いたいとき、CI の失敗を再現したいときだけ、WSL2 に別のクローンを作って使う。

## 結果

- 導入が簡単で、Android Studio と同じ作業フォルダを使える。
- コンパイラが Android NDK と同じ clang 系になり、PC と Android の差が出にくい。
- Windows では UBSan と Valgrind が使えないので、CI（Linux）で必ず回す。
