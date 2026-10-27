# tests/ — テスト

| フォルダ | 中身 |
|---|---|
| `unit/` | Unity の単体テスト。`unit/<コンポーネント名>/test_<対象>.c` の形で置く |
| `data/` | 計測アルゴリズムの回帰テスト用データ（録音・センサーログ）。Git LFS で管理する |
| `manual/` | 実機で確認する手順書（アラーム、一晩の計測など）。今後追加する |

## 実行のしかた

```powershell
ctest --preset windows-clang
```

CI では、Linux 上で AddressSanitizer と UndefinedBehaviorSanitizer を有効にして実行します。

## テストを追加するとき

1. `unit/<コンポーネント名>/` にテストのファイルを作ります。
2. `tests/CMakeLists.txt`（最初のテストを書く人が作ります）に、テストの実行ファイルと `add_test` を追加します。
3. 最初のテストが入ったら、`CMakePresets.json` のテストプリセットの `noTestsAction` を `ignore` から `error` に変えます（テストが 1 つも見つからないときに失敗させるため）。
