# JOJIFrontier 兵種仕様

## Canon status

JOJIFrontier is set around Embermarch, so its roster should first express
Embermarch's canonical military doctrine. The World Bible defines archetypes,
while this repository defines exact stats and battlefield mechanics.

Primary sources:

- `../../JojiWorldBible/docs/world/embermarch.md`
- `../../JojiWorldBible/military.md`
- `../../JojiWorldBible/docs/story/war_of_rivermark/class_tree_ja.md`
- `../../JojiWorldBible/docs/world/magic.md`

The Rivermark class tree is authoritative for that named campaign, not a universal
class list for every Joji game. Frontier may reuse its mechanical roles without
claiming that every Rivermark title exists in Embermarch.

## Implemented core roster

| Frontier class | Bible basis | Battlefield rule | Exploration contribution |
|---|---|---|---|
| March Captain | Embermarch command culture | MOV 4; adjacent ally receives defense +1; no stacking | military orders and negotiation |
| Veteran Guard | Embermarch typical unit | entering an adjacent tile ends enemy movement | force and endurance choices |
| Watch Archer | Embermarch typical unit | MOV 4; range 2-3; cannot attack adjacent | scouting and deployment preview |
| Frontier Scout | Embermarch typical unit | MOV 5; Ash costs 1 | hidden routes and encounter warning |
| Spearman | Embermarch typical unit | defense +2 against attackers that moved 2+ tiles | hold crossings and escort actions |
| Dawn Chirurgeon | Embermarch Church of Dawn healers | heals self/adjacent ally for 8 HP; action ends | medicine and casualty choices |

The playable starting party is March Captain, Veteran Guard, Watch Archer, and
Dawn Chirurgeon. Nessa (Frontier Scout) and Rowan (Spearman) are loaded as reserve roster data;
party selection UI is the next requirement before they can be deployed normally.

Bandit remains as the generic enemy-only raider class. Enemy archers and spear
users now use the same Watch Archer and Spearman rules as player units.

## 12兵種の統合基準表

この表をプレイアブル兵種の基礎能力、内部ID、基本武器の正本とする。数値は装備・一時効果適用前で、
通常レベルでは上昇しない。

| 兵種ID | 日本語名 | HP | STR | MAG | SPD | DEF | RES | MOV | 基本武器 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| `MarchCaptain` | 行軍隊長 | 22 | 7 | 1 | 7 | 5 | 4 | 4 | 鉄の剣、威力5、射程1、物理 |
| `VeteranGuard` | 古参守備兵 | 28 | 8 | 0 | 3 | 10 | 3 | 3 | 鉄の長槍、威力6、射程1-2、物理 |
| `WatchArcher` | 監視弓兵 | 18 | 6 | 0 | 7 | 3 | 2 | 4 | 監視弓、威力5、射程2-3、物理 |
| `FrontierScout` | 辺境斥候 | 18 | 5 | 0 | 9 | 3 | 3 | 5 | 斥候刀、威力4、射程1、物理 |
| `Spearman` | 槍兵 | 23 | 7 | 0 | 5 | 7 | 3 | 4 | 鉄の槍、威力6、射程1-2、物理 |
| `DawnChirurgeon` | 暁の衛生兵 | 17 | 2 | 6 | 6 | 2 | 8 | 4 | 暁の杖、威力3、射程1-2、魔法 |
| `HeavyInfantry` | 重装兵 | 32 | 8 | 0 | 2 | 12 | 2 | 3 | 鉄の大槌、威力7、射程1、物理 |
| `FrontierEngineer` | 辺境工兵 | 21 | 6 | 1 | 5 | 5 | 4 | 4 | 工作槌、威力5、射程1、物理 |
| `MessengerCavalry` | 伝令騎兵 | 22 | 7 | 0 | 9 | 4 | 3 | 6 | 伝令剣、威力5、射程1、物理 |
| `FrontierRanger` | 辺境猟兵 | 20 | 6 | 0 | 7 | 4 | 4 | 4 | 狩猟弓、威力4、射程2、物理 |
| `BannerBearer` | 旗手 | 22 | 5 | 2 | 5 | 5 | 6 | 4 | 戦旗槍、威力4、射程1-2、物理 |
| `BattleMage` | 戦闘魔導士 | 16 | 1 | 9 | 5 | 2 | 7 | 4 | 魔導焦点具、威力6、射程1-2、魔法 |

- `Bandit`など敵専用兵種は12兵種へ含めず、別の敵データとして管理する
- SPDは現行のPhase内行動回数や命中率を増やさない
- 基本必中を維持し、茂みなど明示された地形だけが命中率を変更する
- 敵が同じ兵種を使う場合も基礎能力と固有能力を共有する
- 戦闘魔導士の魔法は短射程で、希少性・疲労・非攻城兵器という共有設定を維持する

## Planned expansion roster

Heavy Infantry, Frontier Engineer, Messenger Cavalry, Banner Bearer, Frontier
Ranger, and Battle Mage remain planned. Ranger is a tracker/trapper rather than
a dedicated monster-hunter; wolves and large territorial wildlife may still be
combat threats in Frontier scenarios. Battle Mage should remain tied to a rare
named specialist.

## 後半6兵種の基礎仕様

後半6兵種は未実装だが、実装時の基準値と固有能力を次で固定する。数値は初期6兵種と同じ
装備補正前の基礎値であり、通常レベルによって上昇しない。

| 兵種 | HP | STR | MAG | SPD | DEF | RES | MOV | 兵種固有能力 |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| 重装兵 | 32 | 8 | 0 | 2 | 12 | 2 | 3 | **重量装甲**: ノックバック距離を1減らし、0以下なら移動しない |
| 辺境工兵 | 21 | 6 | 1 | 5 | 5 | 4 | 4 | **野戦工作**: 戦闘中1回、隣接する空きマスへ耐久10の防護板を設置 |
| 伝令騎兵 | 22 | 7 | 0 | 9 | 4 | 3 | 6 | **再移動**: 攻撃・スキル・アイテム後、生存していれば最大2マス移動して行動終了 |
| 辺境猟兵 | 20 | 6 | 0 | 7 | 4 | 4 | 4 | **簡易罠**: 戦闘中1回、隣接する空きマスへ踏むと移動終了する罠を設置 |
| 旗手 | 22 | 5 | 2 | 5 | 5 | 6 | 4 | **戦旗**: マンハッタン距離2以内の味方のSTRとMAGを+1。同効果は重複しない |
| 戦闘魔導士 | 16 | 1 | 9 | 5 | 2 | 7 | 4 | **魔力波及**: 戦闘中1回、通常魔法攻撃後に対象と上下隣接する敵へ固定3ダメージ |

### 固有能力の制限

- 重量装甲は押し出しを完全無効にする能力ではない。距離2以上の強制移動は短縮後に適用する
- 防護板はユニット初期配置を妨げず、既存ユニット、目的マス、通行不能地形には置けない
- 再移動は敵占有、通行不能地形、Zone of Controlを無視しない。通常攻撃前の未使用MOVは加算しない
- 簡易罠は敵味方とも通過可能だが、最初に踏んだ敵だけを停止させて消滅する。ダメージは与えない
- 戦旗は旗手自身へ適用せず、複数の旗手や一時強化と重複しない
- 魔力波及の固定ダメージはDEF、RES、標的指定の補正を受けず、反応攻撃を発生させない
- 戦闘魔導士は希少な名前付き加入人物とし、初期編成や一般雇用候補には含めない

後半6兵種の装備スキルは[`skill_system.md`](skill_system.md#後半6兵種の装備スキル)を正本とする。

## 兵種の役割境界

ここでいう「やらないこと」は単なる低ステータスではなく、今後その兵種へ武器、スキル、
調整特性を追加するときも越えてはいけない設計境界とする。装備分岐によって隣の兵種の
中心能力を獲得させない。

| 兵種 | 専有する中心役割 | 明確にやらないこと |
|---|---|---|
| 行軍隊長 | 少人数の隊形支援。近くの味方1〜2人を位置関係によって守る | 広範囲の常時強化、再行動付与、主回復、最高火力、重装級の耐久を持たない。旗手・衛生兵・重装兵の役割を奪わない |
| 古参守備兵 | Zone of Controlで敵の移動継続を止め、通路を封鎖する | 味方の被害を肩代わりしない、ノックバック完全無効を標準装備にしない、突撃敵だけへの特効を持たない。重装兵・槍兵と分ける |
| 監視弓兵 | 射程2以上から直接ダメージを与え、接近を強制する | 隣接攻撃、罠設置、悪路無視、再移動、範囲攻撃を持たない。猟兵・斥候・魔導士の仕事をしない |
| 辺境斥候 | 悪路を低コストで通過し、先行・側面取り・踏査を行う | 騎兵級の攻撃後長距離再移動、敵移動の強制停止、遠距離主火力、罠による継続封鎖を持たない |
| 槍兵 | 2マス以上移動して攻撃してきた敵を迎撃姿勢で受け止める | 静止した敵への常時防御ボーナス、周囲全体の移動停止、最高基礎防御、味方の被害肩代わりを持たない |
| 暁の衛生兵 | 単体HP回復、状態異常治療、戦闘不能予防を行う | 広範囲の恒常バフ、強力な範囲攻撃、蘇生、死亡の撤回、無制限の完全回復を持たない。魔法は回復・防護の短射程に限定する |
| 重装兵 | 高防御とノックバック耐性で攻撃を受け、障害物を突破する | Zone of Controlで移動を止めない、突撃限定の迎撃補正を持たない、高MOV・再移動・遠距離攻撃を持たない |
| 辺境工兵 | 障害物・防護板・橋・罠解除で盤面構造を変える | 前衛最高耐久、主回復、長射程直接火力、敵を自動停止させるオーラを持たない。設置物なしで戦闘役割を完結させない |
| 伝令騎兵 | 高MOVと行動後再移動で救援・伝令・離脱を行う | 悪路ペナルティを無条件で無視しない、敵陣に居座る耐久を持たない、Zone of Control、罠、長射程主火力を持たない |
| 辺境猟兵 | 罠、追跡、獣の行動予測で敵の進路と選択を変える | 監視弓兵以上の通常射程・直接火力を持たない、広範囲バフ、重装耐久、自由な攻撃後再移動を持たない。獣への単純な恒常大ダメージ特効だけで役割を作らない |
| 旗手 | 複数の味方へ短時間の範囲支援を与える | 行軍隊長の位置関係による個別防御、回復、再行動の連発、単体高火力、前衛耐久を持たない。効果は広い代わりに数値を小さくする |
| 戦闘魔導士 | 低RES攻撃、限定的な範囲攻撃、短時間の地形干渉を行う | 回復役を兼ねない、弓を超える恒常射程、攻城兵器級の攻撃、長時間の地形封鎖、量産可能な一般兵扱いをしない |

### 境界を判定する短文

- 敵を止める: 古参守備兵
- 攻撃を受ける: 重装兵
- 突撃を迎撃する: 槍兵
- 悪路を越える: 辺境斥候
- 行動後に離脱する: 伝令騎兵
- 遠くから直接倒す: 監視弓兵
- 罠で進路を変える: 辺境猟兵
- 設置物で盤面を変える: 辺境工兵
- 近くの少人数を支える: 行軍隊長
- 広い範囲を薄く支える: 旗手
- HPと状態を治療する: 暁の衛生兵
- 複数マスと低RESを攻める: 戦闘魔導士

## データ・UI契約

- 兵種ID、Unit ID、Weapon IDを表示文字列から逆引きしない
- 日本語UIでは本書の日本語兵種名と武器名を使用し、英語名や`?`へ暗黙フォールバックしない
- Unitデータは人物名と兵種IDだけを持ち、基礎能力と基本武器を兵種データから取得する
- 兵種データ読込時に基礎能力、基本武器、固有能力、探索能力、Tier 1スキルの存在を検証する
- 許可されていないWeapon ID、Skill ID、重複Unit IDを含むセーブは装備だけ安全な基本値へ戻し、人物を削除しない
- 加入時に基本武器へ戻す処理で、既存の製作武器や別人物の装備を上書きしない
- 兵種固有能力は装備から外せず、武器分岐や装備スキルで完全代替しない

## 確定テスト

1. 12兵種すべてのID、基礎能力、基本武器、固有能力を読める
2. 全基本武器が対応する兵種へ装備でき、非対応兵種へ装備できない
3. 物理・魔法ダメージがそれぞれSTR/DEF、MAG/RESを使う
4. 監視弓と狩猟弓が隣接攻撃できない
5. 分岐武器を1回製作すると恒久登録され、再製作を要求しない
6. 複数の同兵種人物が同じ製作済み武器を装備できる
7. 遠征開始後は装備を変更できない
8. 敗北、戦闘不能、施設の稼働停止で製作済み武器を失わない
9. 12兵種の「やらないこと」に反する固有能力、武器、スキルをデータ検証で警告できる
10. 日本語名が欠落した兵種・武器をリリースデータ検証で失敗させる
11. 同じUnit IDを二重登録できない
12. Schema移行後もUnit IDを基準に人物、武器、スキルが一致する

## Reusable Rivermark patterns

Useful mechanical patterns from `class_tree_ja.md` include Scout, Field Healer,
Skilled Spearman, Archer, Heavy Infantry, Engineer, Messenger Cavalry, and support
banner roles. Names tied to Linus's campaign, its promotions, or its unique cast
should not be copied into Frontier without a deliberate cross-project decision.

## Rules that remain Frontier-specific

- No character levels or level-gated promotions.
- Growth comes from facilities, equipment traits, class techniques, and roster
  options.
- Basic attacks remain deterministic and always hit.
- Classes should differ through movement, range, terrain interaction, protection,
  and action options rather than stat inflation alone.

## Weapon branch design

The initial six classes' proposed horizontal weapon branches, exact numeric
tradeoffs, unlock cadence, and implementation guardrails are defined in
[`base_development.md`](base_development.md#初期6兵種の武器分岐仕様). These are
Frontier gameplay rules rather than new World Bible military canon. Spearman is
the only class whose branches are currently implemented.

### 武器の所有と装備

- 基本武器は人物加入時にその兵種の恒久装備候補として自動登録する
- 分岐武器は鍛冶場で一度製作すると恒久装備候補として登録し、同じレシピを再製作しない
- 武器は数量を持つLoot Stackではなく、製作済みWeapon IDの集合で管理する
- 同じ兵種の複数人物が将来加入した場合、製作済み武器を同時装備できる
- 武器耐久、紛失、修理、遠征バッグ枠、装備重量Inventoryは採用しない
- 装備変更は拠点のユニットページでのみ行い、遠征開始後は帰還まで固定する
- 製作した武器と調整特性は施設の稼働停止、敗北、人物の戦闘不能で失わない
- 装備互換は兵種ごとの許可Weapon IDで検証し、別兵種の武器を自由装備させない

## Tactical implementation reference

Cross-class formations, movement control, objective play, terrain use, enemy
matchups, and implementation review criteria are collected in
[`tactical_design_reference.md`](tactical_design_reference.md). New class,
weapon, skill, enemy AI, and map work should identify which tactical pattern in
that document it supports.

The shared rules for innate abilities, two equipped skill slots, cooldowns,
stacking, unlocks, UI, enemy use, and future save data are defined in
[`skill_system.md`](skill_system.md).

The twelve-person roster size, class-by-class recruitment order, retry-safe
join conditions, and current character-name canon status are defined in
[`roster_design.md`](roster_design.md).

All twelve battlefield roles become available by the safe return after the
Ashiron Quarry, the third region. Later progression unlocks weapon branches,
additional equipped skills, traits, and facility effects rather than withholding
core classes.
