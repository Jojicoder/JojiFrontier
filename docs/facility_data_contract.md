# 施設データ契約

文書種別: **正本**
正本範囲: Facility、Research、RecipeのJSON/C++型、所有状態、Alias、検証

状態: 正式仕様。施設・研究・レシピのゲーム内容は`facility_research.md`と`item_system.md`、
この文書はJSON、C++、Save間の契約を定める。

## 定義型

```cpp
using FacilityId = std::string;
using ResearchNodeId = std::string;
using RecipeId = std::string;

enum class FacilityKind { Core, Buildable };
enum class ResearchEffectKind { UnlockRecipe, UnlockResearch, PermanentFeature };
enum class ProductKind { Consumable, Weapon, TraitPart, ExplorationTool };

struct CostEntry { ItemId itemId; int quantity; };

struct FacilityDefinition {
    FacilityId id;
    TextKey nameKey;
    TextKey summaryKey;
    FacilityKind kind;
    BaseStage unlockStage;
    std::vector<DiscoveryId> requiredDiscoveries;
    std::vector<CostEntry> buildCost;
    std::vector<ResearchNodeId> researchNodeIds;
    std::vector<RecipeId> recipeIds;
};

struct ResearchNodeDefinition {
    ResearchNodeId id;
    FacilityId facilityId;
    TextKey nameKey;
    TextKey descriptionKey;
    ResearchEffectKind effectKind;
    std::vector<DiscoveryId> requiredDiscoveries;
    std::vector<CostEntry> cost;
    std::vector<ResearchNodeId> prerequisiteIds;
    std::vector<FacilityId> requiredFacilityIds;
    std::vector<RecipeId> unlockedRecipeIds;
    std::vector<ResearchNodeId> unlockedResearchIds;
    RegionId earliestRegionId;
};

struct RecipeDefinition {
    RecipeId id;
    FacilityId facilityId;
    TextKey nameKey;
    ProductKind productKind;
    ItemId productId;
    int productQuantity = 1;
    std::vector<CostEntry> cost;
    std::vector<ResearchNodeId> requiredResearchIds;
    std::vector<DiscoveryId> requiredDiscoveries;
    std::optional<UnitClassId> classFilter;
};
```

表示文、素材名、兵種名を定義JSONへ直書きせずLocale Keyを使う。効果は自由文を実行せず、
列挙された`ResearchEffectKind`と参照IDから適用する。

## JSON配置

- `data/facilities.json`: `FacilityDefinition`
- `data/research_nodes.json`: `ResearchNodeDefinition`
- `data/recipes.json`: `RecipeDefinition`
- `data/id_aliases.json`: 旧IDから正式IDへの単方向Alias

JSONは定義だけを持ち、建設済み・研究済み・所持数を含めない。

## 所有状態

```cpp
struct BaseFacilityState {
    std::unordered_set<FacilityId> builtFacilityIds;
    std::unordered_set<ResearchNodeId> completedResearchIds;
};

struct BaseInventoryState {
    std::unordered_map<ItemId, int> quantities;
    std::unordered_set<ItemId> uniqueToolIds;
};
```

- Core施設は新規Save生成時に建設済みとして登録する
- Buildable施設は建設Transaction成功後に追加する
- 建設済み施設は常時利用可能。稼働枠、停止、解体、再建を持たない
- 研究完了状態は恒久で、素材を再消費しない
- レシピ所有集合は保存せず、Definitionと研究・Discoveryから毎回導出する

## Transaction

建設、研究、製作はすべて原子的に処理する。

1. IDと前提条件を検証
2. 素材・倉庫空きを検証
3. 変更後Stateを一時生成
4. Save成功
5. UIへ成功を通知

途中失敗時は素材、研究、完成品を一切変更しない。連打やSave再試行でも同じTransaction IDを
二重適用しない。

## 旧ID Alias

- Aliasは`oldId -> canonicalId`の一方向だけ
- Alias連鎖は禁止し、読込時に1回で正式IDへ解決する
- 正式IDと旧IDが同時に存在する場合は正式IDへ統合し、数量は上限まで合算する
- 研究・施設の集合は重複を除去する
- 未知IDは恒久状態ならSave上書きを停止して復旧画面へ送る
- Aliasは最低2 Schema世代保持し、削除前に公開Save Fixtureを移行試験する

## データ検証

起動時とCIで次を検証し、Releaseでは不正データによる新規遠征を開始しない。

1. 各種IDが種類内で一意
2. Facility、Research、Recipe、Item、Discovery、Region、Class参照が存在
3. 研究前提グラフに循環がない
4. Recipeの完成数とCost数量が1以上
5. Unique Toolの完成数が1
6. 製作解放研究が最低1 Recipeから参照される
7. Recipeの担当施設と研究の担当施設が矛盾しない
8. Core施設に建設費を設定しない
9. Locale Keyが日本語辞書に存在し、疑問符による代替表示やUnicode置換文字を含まない
10. Aliasの衝突、循環、正式ID上書きがない
