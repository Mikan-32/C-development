# tests/eval/ — 分析の評価（AI-7）

分析・スコア・フィードバックの仕様（`docs/spec/ai/`）を Python で実装した参照実装と、合成データでの評価です。

| ファイル | 中身 |
|---|---|
| `nk_ref.py` | 参照実装（AI-2 品質判定、AI-3 段階推定、AI-4 スコア、AI-5 規則の一部） |
| `evaluate.py` | 合成データを作って評価し、`out/results.json` に結果を書く。`--golden` で `tests/golden/analysis/` の期待値も作り直す |

```powershell
python tests/eval/evaluate.py --golden
```

Python 3.10 以降の標準ライブラリだけで動きます。

## ルール

- 仕様を変えたら、`nk_ref.py` を同じ PR で直し、期待値データを作り直します。
- C の実装（`src/analysis/`）と結果が食い違ったら、どちらが仕様どおりかを確かめてから直します。
- 合成データの結果は、実際の精度ではありません（AI-7 の 1 章）。
