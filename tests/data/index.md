# テストデータ目録

以下は、人工的に生成した1秒特徴量の合成ログです。

| ファイル名 | 説明 | 期待される検出数 |
| :--- | :--- | :---: |
| `quiet_night.bin` | SNORE候補が現れない合成入力 | 0 |
| `snore_3events.bin` | SNORE候補が3区間現れる合成入力 | 3 |

期待される検出数は、生成されるSNOREイベントの件数です。

## 異常系テスト用データ

- `truncated.bin`：途中で切れたログ。`NK_ERROR_FORMAT` を期待する。
- `trailing.bin`：末尾に余分なデータがあるログ。`NK_ERROR_FORMAT` を期待する。
- `not-present.bin`：作成しない。存在しないファイルに対して `NK_ERROR_IO` を期待する。
