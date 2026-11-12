# C-development（ねむのき）

一年次の共同開発です。

睡眠改善アプリ「ねむのき」の Android 版を開発します。アプリ本体は C 言語で書きます。

> **ステータス**：画面の試作（PC 版）が動きます。表示している記録はデモ用の固定データで、計測・分析・カーテン操作などの機能はまだありません。

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
├─ android/        Android アプリとして組み立てる Gradle プロジェクト（これから作る）
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
4. PC 版をビルドしてテストします（**Developer PowerShell for VS** で実行します）。
   ```powershell
   cmake --preset windows-clang
   cmake --build --preset windows-clang
   ctest --preset windows-clang
   ```

## 画面の試作（PC 版）を動かす

UI 案（`docs/ux/` の UX-6〜8 と画面モック v2、UX 班の改訂版 UX-1〜5）をもとに、全画面を LVGL で作っています。ビルドすると `build/windows-clang/nemunoki.exe` ができます。

```powershell
.\build\windows-clang\nemunoki.exe                                  # ホームから起動
.\build\windows-clang\nemunoki.exe --screen BR-01                   # 画面を指定して起動
.\build\windows-clang\nemunoki.exe --screen HM-01 --state ST-ANALYZING
.\build\windows-clang\nemunoki.exe --list                           # 画面と状態の一覧
.\build\windows-clang\nemunoki.exe --shots build\shots              # 全画面・全状態を PNG で保存（ウィンドウなし）
```

| 操作 | 内容 |
|---|---|
| クリック・ドラッグ・ホイール | タップ・スクロール |
| Esc・Backspace | 戻る（Android の戻るボタン） |
| F2 | いまの画面を PNG で保存 |
| F12 | 画面と状態の一覧（押すとその画面を開く） |

- 日本語フォントは、`assets/fonts/NotoSansJP-*.ttf`（同梱する予定）か、Windows の游ゴシック・メイリオを使います。`--font` と `--font-bold` で指定もできます。
- `--font-scale 1.2`（大）・`1.4`（特大）で文字を大きくし、`--reduce-motion` で動きを減らします（UX-8）。

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
