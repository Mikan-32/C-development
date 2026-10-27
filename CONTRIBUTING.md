# 開発ルール（CONTRIBUTING）

チーム全員が同じやり方で開発するためのルールです。迷ったら、ここに戻ってきてください。

ルールを変えたいときは、まず Issue で提案して話し合い、決まったらこのファイルを PR で更新します。

## 1. 開発の流れ

1. **Issue を立てる**：やることを 1 つの Issue にします。テンプレート（機能・バグ）を使い、関係する PRD の項目番号を書きます。
2. **ブランチを切る**：最新の `main` から作ります。
   ```
   git switch main
   git pull
   git switch -c feat/12-session-start
   ```
3. **コミットする**：小さな単位でコミットします。コミットの前に、lefthook が clang-format を自動で実行します。
4. **PR を出す**：テンプレートに沿って書き、本文に `Closes #12` のように Issue 番号を書きます。
5. **マージする**：CI が通り、1 人以上が承認したら **Squash and merge** でマージし、ブランチを削除します。

`main` へ直接 push してはいけません。

### ブランチ名

`種類/Issue番号-短い説明`（英小文字とハイフンで書きます）

| 種類 | 使うとき |
|---|---|
| `feat/` | 機能の追加 |
| `fix/` | バグの修正 |
| `docs/` | ドキュメントだけの変更 |
| `test/` | テストだけの追加・修正 |
| `chore/` | 設定、ビルド、CI などの変更 |

### コミットメッセージ

[Conventional Commits](https://www.conventionalcommits.org/ja/v1.0.0/) の形にそろえます。要約は日本語で構いません。

```
種類(スコープ): 何をしたか

（必要なら）なぜそうしたか
```

- 種類：`feat` `fix` `docs` `test` `refactor` `build` `ci` `chore`
- スコープ：触ったコンポーネント（`session` `ui` `alarm` `platform` など）

例：`feat(session): 就寝セッションの開始と停止を追加`

### PR のルール

- 1 つの PR では 1 つのことだけをします（差分の目安は 400 行以内）。
- 自分でセルフレビューしてから、レビューを依頼します。
- レビューで指摘を受けたら、コメントで返事をしてから直します。
- 画面を変えたときは、スクリーンショットを貼ります。

## 2. C のコーディング規約

インデント、改行、空白などの整形は clang-format が自動で行います。ここでは、整形以外のルールだけを決めます。

### 名前

| 対象 | 形 | 例 |
|---|---|---|
| 公開する関数 | `nk_コンポーネント_動作` | `nk_session_start()` |
| 型 | `nk_名前`（構造体タグと同じ名前で typedef する） | `nk_session` |
| マクロ・列挙子 | `NK_` で始まる大文字 | `NK_OK`、`NK_SESSION_MAX_EVENTS` |
| ファイルの中だけで使う関数 | `static` を付け、接頭辞は付けない | `static int clamp_minutes(int m)` |
| 変数 | 小文字とアンダースコア | `wake_minutes` |

### コンポーネントとヘッダー

- コンポーネントは `src/コンポーネント名/` に置き、公開ヘッダー（例：`session.h`）と実装（`session.c`）を 1 対 1 で対応させます。
- 他のコンポーネントで使ってよいのは、公開ヘッダーだけです。中身（`.c` や非公開のヘッダー）を直接使ってはいけません。
- 依存は「画面 → 機能 → 基盤」の一方向だけにします。下の段から上の段へ結果を返すときは、コールバックを使います。
- インクルードガードは `NK_コンポーネント名_H`（例：`NK_SESSION_H`）にします。

### メモリ

- `nk_xxx_create()` で確保したものは、同じコンポーネントの `nk_xxx_destroy()` で解放します。
- 所有権を相手に渡す関数は、ヘッダーのコメントに「呼び出し側が解放する」と明記します。
- `malloc` の戻り値は必ず確認します。

### エラー処理

- 失敗する可能性のある関数は、共通のエラー型 `nk_result`（`src/common/` で定義します）を返します。
- エラーをあえて無視するときは、その理由をコメントに書きます。

### スレッド

- **LVGL を呼んでよいのは UI スレッドだけです。**
- 計測スレッドや JNI のコールバックから画面に何かを伝えるときは、`SDL_PushEvent` でイベントとして渡します。

### OS に触れる場所

- OS の機能を直接呼んでよいのは、`platform/` と `src/sensing/`（NDK の AAudio・ASensorManager）だけです。
- それ以外のコンポーネントは、PC 上でビルドしてテストできる状態を保ちます。

### LVGL

- v9.6 で非推奨（deprecated）になった API は使いません。v10 で削除されるためです。

## 3. Kotlin（プラットフォーム層）

- Kotlin のコードは `platform/android/` にだけ置きます。
- 役割は「OS への登録」と「OS からの呼び出しを C に渡すこと」だけです。判断やロジックは C 側に書きます。
- C から見える窓口は、`platform/include/nk_platform.h` の 1 枚にまとめます。

## 4. テスト

- ロジックを持つコンポーネント（`sensing` `analysis` `feedback` `session` など）には、Unity で単体テストを書きます。
- PR を出す前に、手元で `ctest --preset windows-clang` が通ることを確認します。
- アラームと一晩の計測は自動テストが難しいので、手順書を作って実機で確認します（手順書は `tests/manual/` に今後追加します）。

## 5. やってはいけないこと

- 署名鍵（`*.jks` `*.keystore`）、API キー、パスワードをコミットすること
- 同意を得ていない人の録音やセンサーデータをコミットすること
- `main` へ直接 push すること、CI を通さずにマージすること

## 6. リポジトリ管理者向け：最初の設定

GitHub の **Settings → Branches** で、`main` にブランチ保護ルールを設定してください。

- Require a pull request before merging（承認 1 件以上）
- Require status checks to pass before merging（CI の各ジョブを指定）
- Do not allow bypassing the above settings

あわせて **Settings → General → Pull Requests** で「Allow squash merging」だけを有効にし、「Automatically delete head branches」をオンにします。
