# C-development（ねむのき）

一年次の共同開発です。

睡眠改善アプリ「ねむのき」の Android 版を開発します。アプリ本体は C 言語で書きます。

> **ステータス**：PCの画面試作・同意管理と、Androidの加速度計測・暗号化保存を実装中です。Androidの画面は実機確認用で、マイク取得・分析・スコア表示は未接続です。

## ねむのきとは

睡眠の記録・分析・フィードバック、スマートカーテン連携による寝室環境づくり、入眠と起床のサポートを行うアプリです。

- 構造：[アーキテクチャ設計書（C4 モデル レベル1〜3）](docs/architecture/nemunoki-architecture-c4.pdf)
- 要件：PRD（`docs/prd/` に置く予定）

## 開発方針

| 項目 | 方針 |
|---|---|
| 対象 OS | Android |
| 言語 | アプリ本体はすべて C。OS 固有の処理だけを、自作のプラットフォーム層（Kotlin＋JNI）に置く |
| 開発環境 | Windows ネイティブが基本。WSL2 は補助として使う |

決めた理由は [docs/adr/](docs/adr/) に記録しています。

## 技術スタック

| 役割 | 技術 |
|---|---|
| 言語 | C11（アプリ本体）、Kotlin（プラットフォーム層のみ） |
| 画面・タッチ・音 | SDL3 |
| GUI 部品 | LVGL v9.6 |
| データ保存 | SQLCipher（暗号化した SQLite） |
| 通信 | libcurl ＋ cJSON |
| マイク・センサー | Android NDK（AAudio・ASensorManager） |
| ビルド | CMake ＋ Ninja（C）、Gradle（Android） |
| テスト | Unity（C）、JUnit（Kotlin） |
| 整形・静的解析 | clang-format、clang-tidy、ktlint |
| ツールの版管理 | mise |
| Git フック | lefthook |

バージョンと、それを選んだ理由は [docs/tech-stack.md](docs/tech-stack.md) にまとめています。

## ディレクトリ構成

```
C-development/
├─ src/            アプリ本体（C）。コンポーネントごとにフォルダを分ける
├─ platform/       プラットフォーム層（C から見える窓口、Android 版の実装、PC 版の代役）
├─ android/        Android アプリとして組み立てる Gradle プロジェクト
├─ tests/          単体テストとテストデータ
├─ third_party/    外部ライブラリ（git submodule）
├─ scripts/        セットアップ用のスクリプト
├─ docs/           設計書、ADR、環境構築の手順
└─ .github/        CI、PR と Issue のテンプレート
```

各フォルダの役割は、そのフォルダの README.md に書いています。

## はじめかた

1. [docs/setup-windows.md](docs/setup-windows.md) に沿って開発環境を入れます。
2. リポジトリを取得します。
   ```powershell
   git clone --recurse-submodules https://github.com/Mikan-32/C-development.git
   cd C-development
   ```
3. ツールと Git フックを入れます。
   ```powershell
   mise trust
   mise install
   lefthook install
   ```
4. 暗号化DBの依存を準備します（**x64 Developer Command Prompt for VS** で実行します）。OpenSSLが準備済みなら再ビルドは不要です。ネイティブWindows版Perlのパスは各自の環境に合わせます。
   ```bat
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-openssl-windows.ps1 -PerlPath C:\Strawberry\perl\bin\perl.exe
   scripts\generate-sqlcipher.cmd
   ```
   固定した版とソースのハッシュは `cmake/dependencies.json` にあります。SQLCipherはmainに合わせた4.19.0、Windows/AndroidのOpenSSLは3.5.9です。
5. PC 版をビルドしてテストします（**Developer PowerShell for VS** または開発者コマンドプロンプトで実行します）。
   ```powershell
   cmake --preset windows-clang
   cmake --build --preset windows-clang
   ctest --preset windows-clang
   ```
6. 画面の試作を起動します。値はすべて固定で、データや機器にはまだつながっていません。クリックで押す、ホイールでスクロール、Esc で戻る、の操作ができます。
   ```powershell
   .\build\windows-clang\nemunoki.exe
   ```

同意・権限・プライバシーのPC確認画面は、リポジトリ直下を作業場所にして `build/windows-clang/platform/pc/nemunoki_pc.exe` を実行します。確認用DBは `nemunoki_pc_encrypted.db`、固定キーはPCのデモ用です。AndroidではKeyVaultの鍵を使います。以前の平文の確認用DBは自動変換せず、新しいファイルを作ります。

Linuxでは `bash scripts/generate-sqlcipher.sh` の後にCMakeで構成します（Tcl・make・OpenSSL 3.xの開発パッケージが必要）。UIを作らず取得・保存だけを検証するときは `-DNK_BUILD_PC_UI=OFF` を指定できます。

## #27と#28のデータ層の接続

- 計測・履歴・削除は同じSQLCipherの `consent`・`session`・`epoch`・`event` を使います。
- `nk_db_*` はPC画面向けの互換窓口です。計測スレッドは専用の `nk_data` 接続を持ち、UIの接続を共有しません。
- 同意の版が変わったら再同意が必要です。以前の `dev-accel-1` の同意も、正式な版へ自動で引き継がず確認し直します。
- 取り消しを取得側が検知すると計測を止め、保存を完了してからサービスを停止します。記録の削除は関連するエポック・イベントと同じトランザクションで行い、未終了の計測は削除を拒否します。
- `sleep_sessions` と仮スコアを作る旧挿入APIは、実際の `data.h` / `session.h` の保存APIへ置き換えました。スコアの保存は分析側で接続する範囲です。
- 私有のWAVを使う再生試験は `-DNK_ENABLE_LOCAL_WAV_TESTS=ON` で有効にします。録音ファイルはこの変更に含めません。

Android Studioで開く場所と実機確認は [android/README.md](android/README.md) を参照してください。夜間試験は、実施したビルドのコミットと条件を併記して記録します。

## 開発の流れ

Issue を立てる → `main` からブランチを切る → PR を出す → CI が通り、1 人以上が承認したらマージ、という流れです。

ブランチ名、コミットメッセージ、C のコーディング規約は [CONTRIBUTING.md](CONTRIBUTING.md) にまとめています。**開発を始める前に必ず読んでください。**

## ドキュメント

| 資料 | 内容 |
|---|---|
| [CONTRIBUTING.md](CONTRIBUTING.md) | 開発の流れとコーディング規約 |
| [docs/setup-windows.md](docs/setup-windows.md) | Windows での環境構築 |
| [docs/tech-stack.md](docs/tech-stack.md) | 技術スタックとバージョン |
| [docs/architecture/](docs/architecture/) | アーキテクチャ設計書 |
| [docs/adr/](docs/adr/) | 設計上の決定の記録（ADR） |
| [third_party/README.md](third_party/README.md) | 外部ライブラリの一覧とライセンス |

## ライセンス

未定です。決まったら `LICENSE` を追加します。利用している外部ライブラリのライセンスは [third_party/README.md](third_party/README.md) を見てください。
