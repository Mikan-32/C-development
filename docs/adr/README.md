# ADR（設計上の決定の記録）

ADR（Architecture Decision Record）は、「何を、なぜ決めたか」を 1 件ずつ残す短い文書です。あとから参加した人や、半年後の自分が「どうしてこうなっているのか」を確かめられるようにします。

## 一覧

| 番号 | タイトル | 状態 |
|---|---|---|
| [0001](0001-c-core-and-kotlin-platform-layer.md) | アプリ本体は C、OS 固有の処理は Kotlin のプラットフォーム層に置く | 承認 |
| [0002](0002-android-first.md) | 対象 OS を Android にする | 承認 |
| [0003](0003-windows-native-development.md) | 日常の開発は Windows ネイティブ（clang-cl）で行い、WSL2 は補助にする | 承認 |
| [0004](0004-sdl3-first-platform-boundary.md) | SDL3 と NDK で書けることは C で書き、Kotlin は「OS に登録するだけ」の処理に限る | 提案 |
| [0005](0005-sleep-data-source.md) | 睡眠データはスマホ本体のマイクと加速度センサー（＋照度センサー）から取る | 提案 |
| [0006](0006-smart-curtain-switchbot.md) | スマートカーテンは SwitchBot カーテン 3（＋ハブ）とし、クラウド API v1.1 で操作する | 提案 |
| [0007](0007-data-spec-and-retention.md) | 睡眠データの仕様と保存期間 | 提案 |
| [0008](0008-sleep-evaluation.md) | 睡眠状態の推定とスコアの方針 | 提案 |
| [0009](0009-automation-rules.md) | カーテンとアラームの自動動作の条件 | 提案 |
| [0010](0010-sound-recording-notification.md) | 入眠サウンドの音源、録音の扱い、通知とスヌーズ | 提案 |
| [0011](0011-test-scope.md) | 実機がない場合の代わりのテストと、実機で確認する範囲 | 提案 |

## 書き方

1. [0000-template.md](0000-template.md) をコピーし、次の番号を付けます（例：`0004-短い英語名.md`）。
2. PR を出して、チームで話し合います。
3. 決まったら「状態」を「承認」にしてマージし、上の一覧に追加します。

決定を変えるときは、元の ADR は消さずに「状態」を「置き換え（00XX へ）」にして、新しい ADR を書きます。
