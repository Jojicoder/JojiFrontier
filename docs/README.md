# JOJIFrontier 文書索引

文書種別: **正本**
正本範囲: `docs/`内の文書分類、責務、競合時の優先順位

設計変更前に、対象分野の正本を確認する。説明文書や進捗記録だけを編集して仕様変更を
完了扱いにしない。

## 文書種別

### 正本

安定ID、数値、条件、処理順、例外、保存項目、受入条件を定義する。コードとデータは正本へ従う。
同じ情報を複数の正本へ重複させず、別分野はリンクで参照する。

### 説明文書

企画意図、全体像、遊び方、設計判断を読みやすく説明する。新しい安定ID、数値、例外を定義しない。
正本と食い違う場合は正本を優先し、説明文書を修正する。

### 進捗記録

実装状況、ロードマップ、試験結果、移行計画を記録する。現在のコードを説明するが、ゲーム仕様を
決定せず、正本を上書きしない。

## 優先順位

1. `../JojiWorldBible`の共有世界正本
2. 本索引で指定したJOJIFrontier正本
3. 実行データと公開Schema。ただし正本との差は不具合として扱う
4. 説明文書
5. 進捗記録、試験結果、旧実装コメント

同じ階層の正本が競合した場合は黙って片方を選ばず、下表の担当正本へ集約し、他方を参照へ変える。

## 正本一覧

### 世界・人物

| 範囲 | 正本 |
|---|---|
| 共有世界、国、歴史、魔法、人物共有設定 | `../JojiWorldBible` |
| Frontierキャンペーン物語 | `story_synopsis.md` |
| Frontier人物表示名・人物像・関係 | `cast_reference.md` |
| 編成・加入時期 | `roster_design.md` |

### 戦闘

| 範囲 | 正本 |
|---|---|
| 3x8基本規則 | `battle_system.md` |
| Root Actionと完全解決順 | `battle_resolution_contract.md` |
| Objectiveと勝敗 | `mission_objectives.md` |
| Battle Object | `battle_objects.md` |
| 反撃 | `counterattack_rules.md` |
| 増援 | `reinforcement_rules.md` |
| Boss共通規則 | `boss_common_rules.md` |
| 状態異常 | `status_effects.md` |
| 攻撃予測 | `combat_forecast.md` |
| 敵AI | `enemy_ai_rules.md` |
| 敗北・撤退・戦闘不能 | `defeat_and_retreat.md` |
| 戦闘結果画面 | `battle_results_screen.md` |
| 戦闘Message | `battle_message_box.md` |

### 兵種・成長

| 範囲 | 正本 |
|---|---|
| 兵種ID・基礎能力・基本武器 | `class_reference.md` |
| 横方向成長・武器レシピ・特性 | `character_progression.md` |
| Skill構成・解放 | `skill_system.md` |
| 初期6兵種Skill実効果 | `initial_skill_effects.md` |

### 遠征・地域

| 範囲 | 正本 |
|---|---|
| 共通探索・周回短縮 | `exploration_system.md` |
| 状態遷移・Transaction・Checkpoint | `expedition_flow.md` |
| 中断Save復旧・Node変更 | `expedition_recovery.md` |
| Pending・報酬所有権 | `expedition_rewards.md` |
| 地域順序・地点数 | `campaign_regions.md` |
| 62地点接続 | `campaign_route_graph.md` |
| Route Graph型 | `route_graph_data.md` |
| Site・Choice・Mission横断型 | `region_mission_data_contract.md` |
| 地域完了・解放 | `region_unlocks.md` |
| 地点固有設定 | `regions/*.md` |
| 本編バランス目標 | `campaign_balance.md` |

### 拠点・所持品

| 範囲 | 正本 |
|---|---|
| 拠点段階・施設建設 | `base_development.md` |
| 施設研究ノード | `facility_research.md` |
| Facility・Research・Recipe型 | `facility_data_contract.md` |
| 地点成果と施設解放 | `stage_facility_progression.md` |
| 施設画面 | `facility_ui.md` |
| 消耗品・探索道具 | `item_system.md` |
| 商店 | `shop_system.md` |
| 倉庫上限・受取保留 | `inventory_overflow.md` |
| 集会所・会話条件 | `gathering_place.md` |

### UI・保存

| 範囲 | 正本 |
|---|---|
| 日本語用語・Locale Key | `localization.md` |
| 遠征準備画面 | `expedition_preparation_screen.md` |
| セーブ媒体・Schema・Export | `save_system.md` |

## 説明文書

| 文書 | 役割 | 参照正本 |
|---|---|---|
| `frontier_setting.md` | 世界とゲームの接続説明 | World Bible、`story_synopsis.md` |
| `tactical_design_reference.md` | 戦術設計の参考書 | 兵種、Skill、AI、Objective各正本 |
| `combat_resolution_order.md` | 行動解決順の解説 | `battle_resolution_contract.md` |
| `item_catalog.md` | 所持品・素材分類の参照一覧 | `item_system.md`、`class_reference.md`、`character_progression.md`、`stage_facility_progression.md` |

説明文書へ数値やIDを追加する必要が生じた場合は、先に担当正本へ追加し、説明文書からリンクする。

## 進捗記録

| 文書 | 役割 |
|---|---|
| `implementation_roadmap.md` | 実装順と完了条件 |
| `implementation_status.md` | 現行コードとの差分 |
| `balance_test_results.md` | 実戦計測結果 |
| `reuse_plan.md` | 既存コード流用計画 |
| `regression_test_plan.md` | 回帰試験計画。ゲーム規則は各正本を参照 |

## 編集規則

1. 正本の先頭に`文書種別: 正本`と`正本範囲:`を書く。
2. 説明文書の先頭に`文書種別: 説明文書`と`参照正本:`を書く。
3. 進捗記録の先頭に`文書種別: 進捗記録`を書く。
4. 数値と安定IDは担当正本に1回だけ記載する。
5. 説明文書は値を再定義せず、正本の節へリンクする。
6. 実装との差は`implementation_status.md`へ記録し、実装を理由に正本を暗黙変更しない。
7. 共有世界の変更前にWorld Bibleの責務を確認する。
