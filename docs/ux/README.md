# docs/ux/ — 画面設計

| 資料 | タスク | 内容 |
|---|---|---|
| [UX-1_onboarding.md](UX-1_onboarding.md) | UX-1 | 初回利用フロー（同意、権限、置き方、アラーム、カーテン） |
| [UX-2_information-architecture.md](UX-2_information-architecture.md) | UX-2 | 情報設計（タブ、画面一覧、遷移、戻る操作） |
| [UX-3_dashboard.md](UX-3_dashboard.md) | UX-3 | ホーム（最新の睡眠、スコア、フィードバック） |
| [UX-4_history.md](UX-4_history.md) | UX-4 | 履歴（日付の選択、比較グラフ、日別の詳細） |
| [UX-5_curtain-automode.md](UX-5_curtain-automode.md) | UX-5 | カーテン操作と自動モード |
| [UX-6_sound-alarm.md](UX-6_sound-alarm.md) | UX-6 | 入眠サウンドとアラーム |
| [UX-7_states.md](UX-7_states.md) | UX-7 | 状態表示（データなし、保存中、分析中、接続切れ、操作失敗など） |
| [UX-8_expression-a11y.md](UX-8_expression-a11y.md) | UX-8 | デザイントークン、文字、表現、アクセシビリティ |
| [UX-9_prototype-review.md](UX-9_prototype-review.md) | UX-9 | PRD の利用の流れでのプロトタイプ確認と修正一覧 |
| [mock/nemunoki-mock-v2.html](mock/nemunoki-mock-v2.html) | UX-3〜7 | 画面モック v2（ブラウザで開く）。`screens/` に画面ごとの画像 |
| [mock/nemunoki-mock-v1.html](mock/nemunoki-mock-v1.html) | — | 画面モック v1（参考。手入力の記録画面は v2 で廃止） |

画面はすべて LVGL で作る（ADR-0004）。各資料の「LVGL での作り方」は、実装 Issue（CS-5〜CS-9）の出発点として書いている。
