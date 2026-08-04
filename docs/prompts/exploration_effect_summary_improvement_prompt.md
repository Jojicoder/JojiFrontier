# プロンプト: 探索効果の自動要約システムをさらに改善させる

このファイルはプロンプトそのもの。以下を丸ごとコピーして使う。

---

JOJIFrontier(C++/raylib製タクティクスRPG)の探索画面(戦闘前に出る3択)で、
各選択肢の効果説明テキストを`ExplorationOutcome`構造体から自動生成する仕組みを
今回新しく実装しました。この仕組みをさらに改善する具体案を提案してください。

## 現状の実装(2026-08-02実装)

- `ExplorationOutcome`(`include/jf/core/Exploration.hpp`)は7フィールドのみ:
  `partyDamage`、`enemiesRemoved`、`enableFreeDeployment`、`deploymentMaxColumn`、
  `restrictedAutoSpawnMaxColumn`(optional)、`extraBarrierCount`、`startingHeatLevel`、
  `enableReinforcementWave`。
- `buildExplorationEffectSummary(const ExplorationOutcome&)`(`src/ui_shared.cpp`)が
  非デフォルトのフィールドだけを「・」区切りで文字列化する(例: "HP-2・敵-1")。
  全フィールドがデフォルトなら固定文言「戦闘条件に変化なし」を返す。
- `ui_exploration.cpp`が3つの選択肢(正面/脇道/斥候ルート)それぞれについて
  `app.explorationOutcomeForChoice(choice)` → `buildExplorationEffectSummary()`で
  文言を生成し、灰枝の森だけ既存の物語文言に括弧書きで併記、他地域はそのまま表示する。
- **既知の未解決課題**: 空の`ExplorationOutcome{}`は「意図的に効果なしとして設計した
  選択肢」と「まだ個別設定していない(残り47地点がフォールバックしているだけの)選択肢」
  を区別できない。どちらも同じ「戦闘条件に変化なし」と表示されてしまう。
- `buildExplorationEffectSummary()`は`ui_shared.cpp`(raylib依存のUI層)にあり、
  ヘッドレスな`jf_battle_tests`ターゲット(`jf_lib`のみリンク)からは直接ユニット
  テストできない。現状は手動/目視確認に頼っている。
- 偵察網(`scout_network`施設)解放時は選択前に敵編成が見えるが(`explorationEnemyPreview()`)、
  ルート選択の判断材料としては使われておらず、効果要約とも連動していない。

## 依頼内容

以下の観点それぞれについて、既存の「Region.cppに直書き」「7フィールドの数値調整のみ」
という設計を大きく崩さない範囲で、具体的な改善案を提案してください。

1. **「意図的な無効果」と「未実装」の区別**: `ExplorationOutcome`側に何らかの
   マーカー(例: `bool intentionallyNoEffect`のようなフラグ、あるいは全く別の手段)を
   足さずに区別する方法はあるか。足す場合、既存62地点への影響(デフォルト値の扱い)は
   どうなるか。
2. **要約文言の質の向上**: 現状は「HP-2・敵-1」のような機械的な列挙のみ。危険度に応じた
   色分けや強調(例: `partyDamage`が大きい場合は警告色にする)、あるいは複数フィールドが
   絡む場合のまとめ方(例: 「配置自由+障害物+2」のような組み合わせ表現)を改善する案。
3. **偵察網との連動**: 偵察網解放時、見えている敵編成の危険度(数・強さ)と選択肢の
   `partyDamage`/`enemiesRemoved`を組み合わせて、選択肢ごとの「おすすめ度」や
   警告文言を追加で出せないか。既存の`explorationEnemyPreview()`を活用する前提で。
4. **テスト可能性の向上**: `buildExplorationEffectSummary()`が`ui_shared.cpp`(raylib層)
   にあるためヘッドレステストから検証できない現状について、UI層への依存を増やさず
   ロジック部分だけ`jf_lib`側に切り出してテスト可能にする現実的な方法はあるか
   (文字列生成ロジックと実際の描画・配色を分離する等)。
5. **今後のフィールド追加への追従性**: `ExplorationOutcome`に新フィールドが増えた際、
   `buildExplorationEffectSummary()`の手作業追記を忘れるリスクをどう減らせるか
   (静的チェック、リフレクション的な仕組み、テストでの网羅性検証など)。

各観点について「効果の大きさ」「実装コスト」「既存設計との整合性」を明記したうえで、
優先順位をつけて提案してください。
