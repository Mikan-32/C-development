# 0002. 対象 OS を Android にする

- 状態：承認
- 日付：2026-10-27

## 背景

- PRD 8 章で、対応 OS は「今後決める事項」になっていた。
- アプリ本体を C で書く（ADR-0001）場合、iOS は新しい API ほど Swift を前提にしていて（アラームの AlarmKit など）、C から使える範囲が狭い。
- Android は、JNI を使えば C から OS の機能を呼べる範囲が広い。NDK には AAudio（音声）や ASensorManager（センサー）のように C の API もある。

## 決定

対象 OS を Android にする。iOS は対象外とする。

## 結果

- C で書ける範囲が広くなる。
- iPhone の利用者は対象外になる。
- Google Play の要件（targetSdk、16KB ページサイズ、全画面通知やフォアグラウンドサービスの申告など）に従う必要がある。詳細は `docs/tech-stack.md` と設計書の 7 章。
