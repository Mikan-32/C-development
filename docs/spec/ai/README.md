# docs/spec/ai/ — 分析・スコア・フィードバックの仕様

| 資料 | タスク | 実装するコンポーネント |
|---|---|---|
| [AI-1_data-spec.md](AI-1_data-spec.md) | AI-1 睡眠データの入力仕様 | `sensing/`、`session/`、`data/` |
| [AI-2_data-quality.md](AI-2_data-quality.md) | AI-2 データ品質の判定 | `analysis/`、`ui/` |
| [AI-3_stage-estimation.md](AI-3_stage-estimation.md) | AI-3 浅い睡眠・深い睡眠・覚醒の推定 | `analysis/` |
| [AI-4_score.md](AI-4_score.md) | AI-4 日ごとの睡眠スコア | `analysis/` |
| [AI-5_feedback.md](AI-5_feedback.md) | AI-5 フィードバック | `feedback/` |
| [AI-6_uncertainty.md](AI-6_uncertainty.md) | AI-6 不確かな結果の抑制 | `analysis/`、`feedback/`、`ui/` |
| [AI-7_evaluation.md](AI-7_evaluation.md) | AI-7 評価・検証 | `tests/eval/`、`tests/golden/` |

仕様を変えるときは、Python の参照実装（`tests/eval/nk_ref.py`）と期待値データ（`tests/golden/analysis/`）を同じ PR で更新します。
