# JOJIFrontier ローカライズ仕様
## 状態

日本語・英語の表示文、用語、検証規則の正本。ゲーム固有の表示名は本書とLocaleデータで管理し、
World Bibleが所有する固有名詞はBibleの定義を優先する。

## 基本規則

- 日本語を基準言語、英語を必須対応言語とする
- 内部IDと表示名を分離し、翻訳変更でSaveや戦闘データを壊さない
- C++、JSONのゲームデータ、Objective定義へ画面表示文を直書きしない
- C++は`TextKey`と型付き引数だけをUIへ渡す
- 診断ログ、テスト失敗文などプレイヤーに見せない文字列だけを直書きの例外とする
- 欠落時に英語、内部ID、`?`へ黙ってフォールバックしない
- 通常UIへSeed、Attempt ID、Battle IDなどの内部値を表示しない

## Localeデータ

正式な配置は次とする。

```text
data/locales/ja.json
data/locales/en.json
```

両ファイルは同じKey集合を持つ。Keyは`ui.battle_result.primary_objective`のような安定IDとし、
表示順や日本語をKeyへ含めない。

```json
{
  "ui.battle_result.primary_objective": "主目的",
  "ui.expedition.start": "遠征開始",
  "message.hp": "{unit}: {current} / {max}"
}
```

置換引数は`{unit}`の名前付き形式に統一する。日本語と英語で引数名、個数、型を一致させる。
複数形など言語固有処理が必要になった時は文字列連結を増やさずFormatterを追加する。

## 正式用語集

### 兵種

| ID | 日本語 | English |
|---|---|---|
| `march_captain` | 行軍隊長 | March Captain |
| `veteran_guard` | 古参守備兵 | Veteran Guard |
| `watch_archer` | 監視弓兵 | Watch Archer |
| `frontier_scout` | 辺境斥候 | Frontier Scout |
| `spearman` | 槍兵 | Spearman |
| `dawn_chirurgeon` | 暁の衛生兵 | Dawn Chirurgeon |
| `heavy_infantry` | 重装兵 | Heavy Infantry |
| `frontier_engineer` | 辺境工兵 | Frontier Engineer |
| `messenger_cavalry` | 伝令騎兵 | Messenger Cavalry |
| `frontier_ranger` | 辺境猟兵 | Frontier Ranger |
| `banner_bearer` | 旗手 | Banner Bearer |
| `battle_mage` | 戦闘魔導士 | Battle Mage |

### 施設

| ID | 日本語 | English |
|---|---|---|
| `command_post` | 司令所 | Command Post |
| `training_ground` | 訓練所 | Training Ground |
| `forge` | 鍛冶場 | Forge |
| `clinic` | 診療所 | Clinic |
| `workshop` | 工房 | Workshop |
| `quarters` | 宿舎 | Quarters |
| `gathering_hall` | 集会所 | Gathering Hall |
| `warehouse` | 倉庫 | Warehouse |

初期段階名の「作戦テント」「訓練場」「簡易鍛冶台」「救護テント」「工作台」「共同テント」は、
施設本体とは別の段階表示Keyとして管理する。

### 状態異常

| ID | 日本語 | English |
|---|---|---|
| `poison` | 毒 | Poison |
| `burning` | 炎上 | Burning |
| `move_down` | 移動低下 | Move Down |
| `defense_down` | 防御低下 | Defense Down |
| `stagger` | よろめき | Stagger |

### 地域

| ID | 日本語 | English |
|---|---|---|
| `ashbough_forest` | 灰枝の森 | Ashbough Forest |
| `cinderwatch_gate` | 沈黙した監視所群 | Cinderwatch Gate |
| `ashiron_quarry` | 灰鉄採石場 | Ashiron Quarry |
| `blackwater_lowlands` | 黒水低湿地 | Blackwater Lowlands |
| `windscar_plateau` | 風裂き高原 | Windscar Plateau |
| `old_frontier_settlement` | 旧辺境集落 | Old Frontier Settlement |
| `ember_ravine` | 燼火峡谷 | Ember Ravine |
| `buried_dawn_sanctum` | 埋没聖堂 | Buried Dawn Sanctum |
| `shattered_march_fort` | 破砕された前線砦 | Shattered March Fort |
| `mapped_edge` | 地図外縁 | Mapped Edge |

### 主な素材

| ID | 日本語 | English |
|---|---|---|
| `wood` | 木材 | Wood |
| `herb` | 薬草 | Herb |
| `high_quality_herb` | 高品質薬草 | High-quality Herb |
| `hide` | 獣皮 | Hide |
| `iron_ore` | 鉄鉱石 | Iron Ore |
| `iron` | 鉄材 | Iron |
| `high_quality_iron` | 高品質鉄材 | High-quality Iron |
| `stone` | 石材 | Stone |
| `building_materials` | 建築材 | Building Materials |
| `cloth` | 織物 | Cloth |
| `food` | 食料 | Food |
| `hardwood` | 硬木 | Hardwood |
| `tack_materials` | 騎具素材 | Tack Materials |
| `poison_material` | 毒素材 | Poison Material |
| `marsh_resin` | 湿地樹脂 | Marsh Resin |
| `heat_resistant_material` | 耐熱素材 | Heat-resistant Material |
| `sulfur` | 硫黄 | Sulfur |
| `ash_crystal` | 灰晶 | Ash Crystal |
| `ruin_fragment` | 遺跡片 | Ruin Fragment |
| `sanctum_equipment` | 聖堂器材 | Sanctum Equipment |
| `military_supplies` | 軍需品 | Military Supplies |
| `rare_material` | 希少素材 | Rare Material |

### 共通UI

| 日本語 | English |
|---|---|
| 主目的 | Primary Objective |
| 副目標 | Secondary Objective |
| 敗北条件 | Defeat Condition |
| 未確定戦利品 | Pending Loot |
| 重要発見 | Discoveries |
| 地域成果 | Region Progress |
| 加入候補 | Recruitment Candidates |
| 戦闘不能 | Incapacitated |
| 遠征を続ける | Continue Expedition |
| 拠点へ帰還 | Return to Base |

## 表示文の組み立て

- `"HP " + value`のような語順依存の連結をしない
- 数量、名前、残り回数はFormatter引数で渡す
- Unit、兵種、武器、施設、地域は表示名ではなくIDを保存する
- `HP / STR / MAG / DEF / RES / MOV`は両言語共通の略号とし、説明Tooltipを翻訳する
- ボタン幅に合わせて省略記号で意味を落とさず、折返しまたは画面側の可変寸法を使う

## 自動検証

`check_localization`をローカル検証とCIの必須項目にする。

1. JSONがUTF-8として読み込め、重複Keyがない
2. `ja`と`en`のKey集合が完全一致する
3. 空文字、未解決Key、内部IDの直接表示がない
4. 両言語の名前付き引数が一致する
5. U+FFFD、連続する`??`、代表的な文字化け断片を検出する
6. C++のプレイヤー表示経路に日本語・英語の直書きがない
7. 使用FontがLocale内の全Glyphを保持する
8. 主要画面を日本語・英語で描画し、欠字、見切れ、重なりを画像確認する

疑問文の単独`?`は許可するが、代替Glyphとして現れる`?`と連続`??`は許可しない。Debugでは欠落Keyを
目立つ専用表示にし、Releaseビルドは検証失敗時に作成しない。

## 新規表示文を追加するときのチェックリスト

Text Key移行が完了するまでの間、新しい兵種・素材・Discovery・地域・武器などを追加した際に
`？？`(Glyph欠落によるTofu)や英日混在を再発させないための暫定チェックリスト。

1. 新しい内部ID(兵種、素材、Discovery、武器など)を追加したら、対応する`pick(en, ja)`または
   Lookup Tableのエントリを**必ず同じコミットで**追加する。片方だけ追加して終わらせない。
2. 日本語表示文は`src/main.cpp`の`loadAppFont()`内`charsetSource`へ**必ず**追加する。追加を
   忘れるとFontのGlyph Atlasに含まれず、実行時に無言でTofu/`？？`になる。追加箇所は主に次の3つ:
   - 兵種enumのループ(`toString`/`classNameFor`/`classRoleFor`用、2箇所)
   - `materialNameFor`の対象ID列挙
   - 地域・地点固有の文字列(`missionNameJa`など)は文字列リテラルへ直接追記
3. 同じ日本語文字列を、意味の異なる別の内部ID(例: 地域の違う討伐対象の素材)へ使い回さない。
   IDごとに固有の表示名を用意する。`kAshveilFangMaterial`(沈黙した監視所群の素材)と
   `kAshenhornFangMaterial`(灰枝の森・灰角大猪の素材)を誤って同じ「灰角の大牙」に割り当てていた
   バグが実例(`materialNameFor`で修正済み)。
4. `unitDisplayNameFor`のような、データ層が渡す英語表示名(`enemyRoster`の名前欄など)を翻訳する
   Lookup Tableにも、新しい敵種を追加したら必ずエントリを足す。戦場ユニット名ラベルはここを経由する。
5. 追加後は日本語モードで実際に画面を確認する(ビルドが通ることは`？？`が出ないことの証明にならない)。

## 移行順

1. Locale loader、Formatter、言語設定を作る
2. 共通ボタン、見出し、エラー文を移す
3. 兵種、素材、施設、状態異常、地域名をID参照へ移す
4. 戦闘メッセージ、結果画面、遠征準備画面を移す
5. `kJa...`、`pick(en, ja)`、表示用直書きを削除する
6. `check_localization`をCIへ追加する
