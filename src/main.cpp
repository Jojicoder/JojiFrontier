#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/core/GameApp.hpp"
#include "jf/core/Grid.hpp"
#include "jf/data/GameData.hpp"

namespace {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 800;

// Fixed side-view mapping. Each logical cell is a low floor surface; the
// larger vertical space between rows belongs to the units standing on it.
constexpr float kGridOriginX = 32.0f;
constexpr float kTileW = 152.0f;
constexpr float kTileH = 58.0f;
constexpr float kGridOriginY = 220.0f;
constexpr float kRowStep = 198.0f;
constexpr float kUnitRadius = 40.0f;
constexpr float kHudY = 704.0f;

Rectangle tileRect(jf::GridPos pos) {
    return Rectangle{
        kGridOriginX + pos.col * kTileW,
        kGridOriginY + pos.row * kRowStep,
        kTileW - 4.0f,
        kTileH,
    };
}

Color scaleColor(Color color, float factor) {
    return Color{static_cast<unsigned char>(std::clamp(color.r * factor, 0.0f, 255.0f)),
                 static_cast<unsigned char>(std::clamp(color.g * factor, 0.0f, 255.0f)),
                 static_cast<unsigned char>(std::clamp(color.b * factor, 0.0f, 255.0f)), color.a};
}

Color withAlpha(Color color, unsigned char alpha) { return Color{color.r, color.g, color.b, alpha}; }

// --- Shared palette ---------------------------------------------------
// One place to keep the game's chrome (panels, borders, buttons, text)
// visually consistent. Gameplay-meaning colors (team colors, tile
// highlight colors, terrain colors) are intentionally left where they
// are so their meaning stays unambiguous.
const Color kColorCard{23, 27, 38, 250};
const Color kColorCardAlt{28, 33, 46, 250};
const Color kColorBorder{108, 122, 145, 210};
const Color kColorBorderSoft{72, 82, 100, 150};
const Color kColorShadow{0, 0, 0, 90};
const Color kColorAccentGold{224, 190, 120, 255};
const Color kColorTextPrimary{238, 240, 245, 255};
const Color kColorTextMuted{176, 186, 200, 255};
const Color kColorTextFaint{140, 150, 165, 255};

constexpr Color kFloorPanelColor{88, 98, 112, 255};

// Rounded card with a soft drop shadow and a hairline border - the base
// look reused by popups, tooltips, and HUD chrome throughout.
void drawCard(Rectangle rect, Color fill, Color border, float roundness = 0.12f) {
    Rectangle shadow{rect.x + 3.0f, rect.y + 4.0f, rect.width, rect.height};
    DrawRectangleRounded(shadow, roundness, 10, kColorShadow);
    DrawRectangleRounded(rect, roundness, 10, fill);
    DrawRectangleRoundedLinesEx(rect, roundness, 10, 2.0f, border);
}

Color terrainColor(jf::TerrainType terrain) {
    switch (terrain) {
        case jf::TerrainType::Ash: return Color{104, 91, 86, 255};
        case jf::TerrainType::Rubble: return Color{103, 94, 78, 255};
        case jf::TerrainType::Barrier: return Color{45, 49, 57, 255};
        case jf::TerrainType::WatchPost: return Color{83, 108, 98, 255};
        case jf::TerrainType::Floor: return kFloorPanelColor;
    }
    return kFloorPanelColor;
}

void drawBattlefieldBackdrop() {
    DrawRectangle(0, 40, kScreenWidth, static_cast<int>(kHudY - 40.0f), Color{25, 31, 42, 255});
    for (int row = 0; row < jf::kGridRows; ++row) {
        float y = kGridOriginY + row * kRowStep;
        DrawRectangleGradientV(0, static_cast<int>(y - 132.0f), kScreenWidth, 132,
                               Color{31, 39, 52, 255}, Color{42, 50, 61, 255});
        DrawLine(0, static_cast<int>(y + kTileH + 8.0f), kScreenWidth,
                 static_cast<int>(y + kTileH + 8.0f), Color{12, 16, 23, 170});
    }
}

void drawTilePanel(Rectangle rect, Color frameColor) {
    constexpr float gap = 3.0f;
    Rectangle surface{rect.x + gap, rect.y, rect.width - gap * 2.0f, rect.height - 12.0f};
    DrawRectangleGradientV(static_cast<int>(surface.x), static_cast<int>(surface.y),
                           static_cast<int>(surface.width), static_cast<int>(surface.height),
                           scaleColor(frameColor, 1.18f), frameColor);
    DrawRectangleRec(Rectangle{surface.x, surface.y + surface.height, surface.width, 12.0f},
                     scaleColor(frameColor, 0.58f));
    DrawLineEx(Vector2{surface.x, surface.y}, Vector2{surface.x + surface.width, surface.y}, 2.0f,
               Color{210, 226, 224, 135});
    DrawRectangleLinesEx(Rectangle{surface.x, surface.y, surface.width, rect.height}, 1.0f,
                         withAlpha(kColorBorder, 130));
}

// --- Bilingual UI strings (English / Japanese) ---------------------------
const std::string kJaPlayerPhase = "プレイヤーフェイズ";
const std::string kJaEnemyPhase = "敵フェイズ";
const std::string kJaSelectUnit = "行動するユニットを選択してください。";
const std::string kJaAttack = "こうげき";
const std::string kJaBack = "戻る";
const std::string kJaItems = "アイテム";
const std::string kJaPotion = "ポーション";
const std::string kJaHeal = "治療";
const std::string kJaWait = "待機";
const std::string kJaChooseMove = "移動先を選択";
const std::string kJaChooseAction = "行動を選択";
const std::string kJaChooseTarget = "攻撃対象を選択";
const std::string kJaConfirm = "決定";
const std::string kJaCancel = "キャンセル";
const std::string kJaCombatPreview = "戦闘予測";
const std::string kJaDamage = "ダメージ";
const std::string kJaVictory = "勝利";
const std::string kJaProceedToCamp = "キャンプへ進む";
const std::string kJaDefeatTitle = "遠征失敗";
const std::string kJaLootLostLine = "遠征で得た戦利品はすべて失われます。";
const std::string kJaReturnToBase = "拠点へ帰還";
const std::string kJaCamp = "キャンプ";
const std::string kJaLootSecured = "戦利品を確保した";
const std::string kJaNoLootThisExpedition = "今回の遠征では戦利品なし";
const std::string kJaContinue = "つづける";
const std::string kJaPartyHp = "パーティHP";
const std::string kJaPendingLoot = "保留中の戦利品";
const std::string kJaNoneYet = "まだありません";
const std::string kJaBattlesWon = "勝利数";
const std::string kJaContinueExpedition = "遠征を続ける";
const std::string kJaDown = "戦闘不能";
const std::string kJaHp = "体力";
const std::string kJaWeapon = "武器";
const std::string kJaStr = "力";
const std::string kJaMag = "魔";
const std::string kJaDef = "防";
const std::string kJaRes = "魔防";
const std::string kJaDataLoadFailed = "データの読み込みに失敗しました";
const std::string kJaSettings = "設定";
const std::string kJaLanguage = "言語";
const std::string kJaClose = "閉じる";
const std::string kJaJapaneseNative = "日本語";
const std::string kJaExpeditionPrep = "遠征準備";
const std::string kJaPartyChoose4 = "パーティ(4人選択)";
const std::string kJaSupplies = "物資";
const std::string kJaBagSlots = "荷物(6枠)";
const std::string kJaEmptySlot = "空き枠";
const std::string kJaBeginExpedition = "遠征開始";
const std::string kJaSelectExactly4 = "ちょうど4人選んでください";
const std::string kJaAdd = "追加";
const std::string kJaRemove = "削除";
const std::string kJaPlaceBarrier = "障害物を設置";
const std::string kJaBag = "荷物";
const std::string kJaSeed = "シード";
const std::string kJaFirstAidShort = "救急";
const std::string kJaFieldShort = "野戦";
const std::string kJaBoardShort = "防御板";
const std::string kJaFieldTreatmentFull = "野戦治療";
const std::string kJaRescuePack = "救助パック";
const std::string kJaCampRations = "野営食料";
const std::string kJaReturnFlare = "帰還信号弾";
const std::string kJaExploration = "探索";
const std::string kJaCinderwatchSituation = "崩れた門の先に放棄された監視所が見える。正面の通路には新しい足跡が残っている。";
const std::string kJaFrontalAdvance = "正面突破";
const std::string kJaFrontalEffect = "消耗なし / 通常戦闘";
const std::string kJaSidePath = "崩れた側道を進む";
const std::string kJaSidePathEffect = "生存中の味方全員 HP -2 / 敵1体減少";
const std::string kJaScoutRoute = "【斥候】高所から偵察";
const std::string kJaScoutRouteEffect = "消耗なし / 左3列に自由配置";
const std::string kJaScoutRouteLocked = "編成に国境斥候が必要";
const std::string kJaPreBattleDeployment = "出撃配置";
const std::string kJaDeploymentInstructions = "左3列の通行可能マスに味方4人を配置してください。";
const std::string kJaEnemyForces = "敵勢力(予告)";
const std::string kJaDeployAllToStart = "全員配置すると戦闘開始できます";
const std::string kJaBeginBattle = "戦闘開始";
const std::string kJaPlaced = "配置済み";
const std::string kJaUnplaced = "未配置";
const std::string kJaOutpost = "拠点";
const std::string kJaDiscoveries = "発見物";
const std::string kJaNoDiscoveriesYet = "まだ発見物はありません";
const std::string kJaEncampment = "野営地";
const std::string kJaPioneerOutpost = "開拓拠点";
const std::string kJaFrontierSettlement = "辺境集落";
const std::string kJaPioneerCity = "開拓都市";
const std::string kJaAdvanceOutpost = "拠点を発展させる";
const std::string kJaAdvanceOutpostLocked = "灰角の大牙を持ち帰る必要があります";
const std::string kJaScoutNetworkDiscovery = "偵察網の発見記録";
const std::string kJaFieldMedicineDiscovery = "野戦医療の記録";
const std::string kJaReturnSignalDiscovery = "帰還信号技術の記録";

// --- Language setting -------------------------------------------------
// Purely a display concern (like the font/colors above), not part of
// battle rules, so it lives here rather than in GameApp/BattleController.
enum class Language { English, Japanese };
Language gLanguage = Language::English;
bool gSettingsOpen = false;

// Picks the string for whichever language is currently selected. Falls
// back to `en` if no Japanese variant was given (e.g. a proper noun).
std::string pick(const std::string& en, const std::string& ja) {
    return (gLanguage == Language::Japanese && !ja.empty()) ? ja : en;
}

// --- Japanese-capable font handling ---------------------------------------
Font gFont{};
bool gFontReady = false;

void drawText(const std::string& text, int x, int y, int fontSize, Color color) {
    if (gFontReady) {
        DrawTextEx(gFont, text.c_str(), Vector2{static_cast<float>(x), static_cast<float>(y)},
                   static_cast<float>(fontSize), 1.0f, color);
    } else {
        DrawText(text.c_str(), x, y, fontSize, color);
    }
}

int textWidth(const std::string& text, int fontSize) {
    if (gFontReady) {
        return static_cast<int>(MeasureTextEx(gFont, text.c_str(), static_cast<float>(fontSize), 1.0f).x);
    }
    return MeasureText(text.c_str(), fontSize);
}

// Loads a system font that covers both ASCII and the Japanese glyphs this
// UI needs (raylib's built-in default font only covers Latin-1). Falls
// back to the default font - and thus tofu/missing glyphs for Japanese -
// if no such font is found, so the game still runs everywhere.
void loadAppFont() {
    std::string charsetSource;
    for (int c = 32; c <= 126; ++c) charsetSource += static_cast<char>(c);
    charsetSource += kJaPlayerPhase + kJaEnemyPhase + kJaSelectUnit + kJaAttack + kJaBack + kJaItems + kJaHeal + kJaWait +
                     kJaChooseMove + kJaChooseAction + kJaChooseTarget + kJaConfirm +
                      kJaCancel + kJaCombatPreview + kJaDamage + kJaVictory + kJaProceedToCamp +
                      kJaDefeatTitle + kJaLootLostLine + kJaReturnToBase + kJaCamp + kJaLootSecured +
                      kJaNoLootThisExpedition + kJaContinue + kJaPartyHp + kJaPendingLoot + kJaNoneYet +
                      kJaBattlesWon + kJaContinueExpedition + kJaDown + kJaHp + kJaWeapon + kJaStr + kJaMag +
                      kJaDef + kJaRes + kJaDataLoadFailed + kJaSettings + kJaLanguage + kJaClose + kJaJapaneseNative +
                      kJaPotion + kJaExpeditionPrep + kJaPartyChoose4 + kJaSupplies + kJaBagSlots + kJaEmptySlot +
                      kJaBeginExpedition + kJaSelectExactly4 + kJaAdd + kJaRemove + kJaPlaceBarrier + kJaBag +
                      kJaSeed + kJaFirstAidShort + kJaFieldShort + kJaBoardShort + kJaFieldTreatmentFull +
                      kJaRescuePack + kJaCampRations + kJaReturnFlare + kJaExploration + kJaCinderwatchSituation +
                      kJaFrontalAdvance + kJaFrontalEffect + kJaSidePath + kJaSidePathEffect + kJaScoutRoute +
                      kJaScoutRouteEffect + kJaScoutRouteLocked + kJaPreBattleDeployment +
                      kJaDeploymentInstructions + kJaEnemyForces + kJaDeployAllToStart + kJaBeginBattle +
                      kJaPlaced + kJaUnplaced + kJaOutpost + kJaDiscoveries + kJaNoDiscoveriesYet +
                      kJaEncampment + kJaPioneerOutpost + kJaFrontierSettlement + kJaPioneerCity +
                      kJaAdvanceOutpost + kJaAdvanceOutpostLocked + kJaScoutNetworkDiscovery +
                      kJaFieldMedicineDiscovery + kJaReturnSignalDiscovery +
                      "平地灰塵瓦礫障害物監視所インパッサブル移動コスト通行不可経路完了"
                      "レオンガレスエリンミラ国境斥候行軍槍兵野盗脱走元隊長救急セット野戦治療キット";
    for (jf::UnitClass uc : {jf::UnitClass::MarchCaptain, jf::UnitClass::VeteranGuard,
                              jf::UnitClass::WatchArcher, jf::UnitClass::FrontierScout,
                              jf::UnitClass::Spearman, jf::UnitClass::DawnChirurgeon,
                              jf::UnitClass::Bandit}) {
        charsetSource += jf::toString(uc);
    }

    static const char* kCandidatePaths[] = {
        // raylib's font loader (stb_truetype) can't parse .ttc font
        // collections, so macOS's Hiragino faces (all .ttc) don't work here;
        // Arial Unicode.ttf is a standalone .ttf that still covers Japanese.
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf", // macOS
        "C:\\Windows\\Fonts\\meiryo.ttc",                        // Windows
        "C:\\Windows\\Fonts\\msgothic.ttc",                      // Windows fallback
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",     // Linux
        "/usr/share/fonts/truetype/noto/NotoSansCJKjp-Regular.otf",   // Linux fallback
    };

    int codepointCount = 0;
    int* codepoints = LoadCodepoints(charsetSource.c_str(), &codepointCount);

    for (const char* path : kCandidatePaths) {
        if (!FileExists(path)) continue;
        Font font = LoadFontEx(path, 40, codepoints, codepointCount);
        // A genuinely loaded custom font produces exactly codepointCount
        // glyphs; raylib silently falls back to the 224-glyph default font
        // (e.g. for .ttc font collections stb_truetype can't parse) if the
        // file failed to load, which this distinguishes from a real hit.
        if (font.glyphCount == codepointCount) {
            gFont = font;
            gFontReady = true;
            SetTextureFilter(gFont.texture, TEXTURE_FILTER_BILINEAR);
            break;
        }
        UnloadFont(font);
    }

    UnloadCodepoints(codepoints);
    if (!gFontReady) {
        TraceLog(LOG_WARNING, "No Japanese-capable font found; Japanese text may not render.");
    }
}

bool tileFromScreen(Vector2 mouse, jf::GridPos& out) {
    if (mouse.x < kGridOriginX) return false;
    int col = static_cast<int>((mouse.x - kGridOriginX) / kTileW);
    if (col < 0 || col >= jf::kGridCols) return false;

    for (int row = 0; row < jf::kGridRows; ++row) {
        Rectangle floor = tileRect(jf::GridPos{row, col});
        Rectangle hitArea{floor.x, floor.y - kUnitRadius * 2.15f, floor.width,
                          floor.height + kUnitRadius * 2.15f};
        if (CheckCollisionPointRec(mouse, hitArea)) {
            out = jf::GridPos{row, col};
            return true;
        }
    }
    return false;
}

Color teamColor(jf::Team team) {
    return team == jf::Team::Player ? Color{60, 120, 220, 255} : Color{200, 60, 60, 255};
}

// jf::toString(UnitClass) returns a fixed "English / Japanese" string (it's
// data-layer, so it doesn't know about the display language setting); pick
// out just the half the player currently wants to read.
std::string classNameFor(jf::UnitClass unitClass) {
    std::string full = jf::toString(unitClass);
    std::size_t sep = full.find(" / ");
    if (sep == std::string::npos) return full;
    return gLanguage == Language::Japanese ? full.substr(sep + 3) : full.substr(0, sep);
}

std::string terrainNameFor(jf::TerrainType terrain) {
    switch (terrain) {
        case jf::TerrainType::Floor: return pick("Floor", "平地");
        case jf::TerrainType::Ash: return pick("Ash", "灰塵");
        case jf::TerrainType::Rubble: return pick("Rubble", "瓦礫");
        case jf::TerrainType::Barrier: return pick("Barrier", "障害物");
        case jf::TerrainType::WatchPost: return pick("Watch Post", "監視所");
    }
    return jf::toString(terrain);
}

// Unit/enemy names come from data/units.json (or a stage-specific rename
// like the "Former Captain" boss) as plain English strings - the data
// layer has no concept of display language. Translate by looking the
// current name up in a small dictionary here; anything not in it (a name
// we haven't localized yet) just falls back to the English original.
std::string unitDisplayNameFor(const std::string& englishName) {
    static const std::unordered_map<std::string, std::string> table = {
        {"Leon", "レオン"},
        {"Gareth", "ガレス"},
        {"Erin", "エリン"},
        {"Mira", "ミラ"},
        {"Frontier Scout", "国境斥候"},
        {"March Spearman", "行軍槍兵"},
        {"Raider", "野盗"},
        {"Watch Archer", "監視弓兵"},
        {"Deserter Spearman", "脱走槍兵"},
        {"Former Captain", "元隊長"},
    };
    auto it = table.find(englishName);
    return (gLanguage == Language::Japanese && it != table.end()) ? it->second : englishName;
}

std::string itemFullNameFor(jf::ItemType type) {
    switch (type) {
        case jf::ItemType::FirstAidKit: return pick("First Aid Kit", "救急セット");
        case jf::ItemType::FieldTreatmentKit: return pick("Field Treatment Kit", "野戦治療キット");
        case jf::ItemType::RescuePack: return pick("Rescue Pack", kJaRescuePack);
        case jf::ItemType::CampRations: return pick("Camp Rations", kJaCampRations);
        case jf::ItemType::ProtectiveBoard: return pick("Protective Board", "防御板");
        case jf::ItemType::ReturnFlare: return pick("Return Flare", kJaReturnFlare);
    }
    return "";
}

std::string outpostStageNameFor(jf::OutpostStage stage) {
    switch (stage) {
        case jf::OutpostStage::Encampment: return pick("Encampment", kJaEncampment);
        case jf::OutpostStage::PioneerOutpost: return pick("Pioneer Outpost", kJaPioneerOutpost);
        case jf::OutpostStage::FrontierSettlement: return pick("Frontier Settlement", kJaFrontierSettlement);
        case jf::OutpostStage::PioneerCity: return pick("Pioneer City", kJaPioneerCity);
    }
    return "";
}

// Discovery IDs come straight from BaseState's data-layer constants; this is
// purely the display-language lookup, same pattern as itemFullNameFor.
std::string discoveryNameFor(const jf::DiscoveryId& id) {
    if (id == jf::kCinderwatchReconDiscovery) return pick("Scout Network Records", kJaScoutNetworkDiscovery);
    if (id == jf::kFieldMedicineDiscovery) return pick("Field Medicine Records", kJaFieldMedicineDiscovery);
    if (id == jf::kReturnSignalDiscovery) return pick("Return Signal Records", kJaReturnSignalDiscovery);
    return id;
}

bool containsTile(const std::vector<jf::GridPos>& tiles, jf::GridPos pos) {
    for (const auto& t : tiles) {
        if (t == pos) return true;
    }
    return false;
}

Vector2 unitLogicalCenter(const jf::Unit& unit) {
    Rectangle rect = tileRect(unit.position);
    return Vector2{rect.x + rect.width / 2.0f, rect.y - kUnitRadius + 9.0f};
}

// Smoothly slides each unit's on-screen position toward its logical grid
// tile instead of snapping there instantly. Battle logic (BattleState /
// BattleController) still moves units in a single instant step - this is a
// purely visual interpolation layer keyed by unit id, so it never affects
// move ranges, hit detection, or turn timing.
Vector2 animatedUnitCenter(const jf::Unit& unit, float dt) {
    static std::unordered_map<std::string, Vector2> visualCenters;
    constexpr float kMoveLerpRate = 9.0f;

    Vector2 target = unitLogicalCenter(unit);
    auto it = visualCenters.find(unit.id);
    if (it == visualCenters.end()) {
        visualCenters.emplace(unit.id, target);
        return target;
    }

    Vector2& current = it->second;
    float t = 1.0f - std::exp(-kMoveLerpRate * dt);
    current.x += (target.x - current.x) * t;
    current.y += (target.y - current.y) * t;
    return current;
}

bool button(Rectangle rect, const std::string& labelEn, const std::string& labelJa, Vector2 mouse,
            bool mousePressed) {
    bool hovered = CheckCollisionPointRec(mouse, rect);
    Color top = hovered ? Color{92, 104, 128, 255} : Color{62, 70, 86, 255};
    Color bottom = hovered ? Color{62, 70, 90, 255} : Color{42, 47, 58, 255};
    float roundness = 0.28f;

    DrawRectangleRounded(Rectangle{rect.x + 2.0f, rect.y + 3.0f, rect.width, rect.height}, roundness, 8,
                         kColorShadow);
    DrawRectangleRounded(rect, roundness, 8, bottom);
    DrawRectangleRounded(Rectangle{rect.x, rect.y, rect.width, rect.height * 0.55f}, roundness, 8, top);
    DrawRectangleRoundedLinesEx(rect, roundness, 8, hovered ? 2.0f : 1.5f,
                                hovered ? kColorAccentGold : kColorBorder);

    std::string label = pick(labelEn, labelJa);
    int fontSize = 16;
    int w = textWidth(label, fontSize);
    drawText(label, static_cast<int>(rect.x + (rect.width - w) / 2),
             static_cast<int>(rect.y + (rect.height - fontSize) / 2), fontSize, kColorTextPrimary);
    return hovered && mousePressed;
}

void disabledButton(Rectangle rect, const std::string& label) {
    DrawRectangleRounded(rect, 0.28f, 8, Color{30, 33, 41, 255});
    DrawRectangleRoundedLinesEx(rect, 0.28f, 8, 1.5f, kColorBorderSoft);
    int fontSize = 14;
    int w = textWidth(label, fontSize);
    drawText(label, static_cast<int>(rect.x + (rect.width - w) / 2),
             static_cast<int>(rect.y + (rect.height - fontSize) / 2), fontSize, kColorTextFaint);
}

void drawGrid(const jf::BattleController& controller, float dt) {
    const jf::BattleState& battle = controller.battle();

    drawBattlefieldBackdrop();

    for (int row = 0; row < jf::kGridRows; ++row) {
        for (int col = 0; col < jf::kGridCols; ++col) {
            jf::GridPos pos{row, col};
            Rectangle rect = tileRect(pos);
            jf::TerrainType terrain = battle.terrainAt(pos);
            drawTilePanel(rect, terrainColor(terrain));
            if (terrain == jf::TerrainType::Barrier) {
                DrawLineEx(Vector2{rect.x + 26.0f, rect.y + 12.0f},
                           Vector2{rect.x + rect.width - 26.0f, rect.y + rect.height - 18.0f}, 5.0f,
                           Color{25, 28, 34, 220});
                DrawLineEx(Vector2{rect.x + rect.width - 26.0f, rect.y + 12.0f},
                           Vector2{rect.x + 26.0f, rect.y + rect.height - 18.0f}, 5.0f,
                           Color{25, 28, 34, 220});
            } else if (terrain == jf::TerrainType::WatchPost) {
                DrawRectangleLinesEx(Rectangle{rect.x + 10.0f, rect.y + 8.0f, rect.width - 20.0f, rect.height - 22.0f},
                                     2.0f, Color{175, 208, 184, 130});
            }

            bool reachable = containsTile(controller.reachableTiles(), pos);
            const jf::Unit* occupant = battle.unitAt(pos);
            bool occupiedByAlly = occupant && controller.selectedUnit() &&
                                  occupant->team == controller.selectedUnit()->team;

            // Movement takes visual priority on overlapping tiles, and an
            // ally's occupied tile should never look like an attack target.
            if (containsTile(controller.attackRangeTiles(), pos) && !reachable && !occupiedByAlly)
                DrawRectangleRec(rect, Color{238, 72, 72, 112});
            if (reachable)
                DrawRectangleRec(rect, Color{58, 155, 255, 125});
            if (containsTile(controller.targetableTiles(), pos) ||
                (controller.pendingTarget() && controller.pendingTarget()->position == pos))
                DrawRectangleRec(rect, Color{230, 28, 38, 185});
            if (containsTile(controller.healableTiles(), pos))
                DrawRectangleRec(rect, Color{55, 205, 115, 155});
            if (containsTile(controller.boardTargetTiles(), pos))
                DrawRectangleRec(rect, Color{220, 185, 70, 150});
        }
    }

    for (const jf::Unit& unit : battle.units()) {
        if (!unit.isAlive()) continue;

        Vector2 center = animatedUnitCenter(unit, dt);
        float radius = kUnitRadius;

        Color c = teamColor(unit.team);
        if (unit.hasActed) c = Fade(c, 0.45f);

        DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y + radius * 0.92f),
                   radius * 0.85f, radius * 0.22f, kColorShadow);

        if (controller.selectedUnit() == &unit) {
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius + 7.0f,
                            withAlpha(kColorAccentGold, 235));
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius + 8.5f,
                            withAlpha(kColorAccentGold, 140));
        }

        DrawCircleV(center, radius, c);
        DrawCircleGradient(static_cast<int>(center.x - radius * 0.18f), static_cast<int>(center.y - radius * 0.2f),
                           radius * 0.72f, scaleColor(c, 1.22f), c);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius,
                        Color{10, 14, 22, 230});

        float barWidth = radius * 2.0f;
        float barHeight = 7.0f;
        Rectangle hpBack{center.x - barWidth / 2.0f, center.y + radius + 6.0f, barWidth, barHeight};
        float hpRatio = static_cast<float>(unit.currentHp) / static_cast<float>(unit.stats.maxHp);
        Color hpColor = hpRatio > 0.5f ? Color{95, 205, 120, 255}
                                       : (hpRatio > 0.25f ? Color{230, 165, 70, 255} : Color{215, 75, 75, 255});
        DrawRectangleRounded(hpBack, 0.5f, 6, Color{18, 20, 26, 255});
        if (hpRatio > 0.0f) {
            DrawRectangleRounded(Rectangle{hpBack.x, hpBack.y, hpBack.width * hpRatio, hpBack.height}, 0.5f, 6,
                                 hpColor);
        }
        DrawRectangleRoundedLinesEx(hpBack, 0.5f, 6, 1.0f, withAlpha(BLACK, 120));

        std::string displayName = unitDisplayNameFor(unit.name);
        int fontSize = 13;
        int nameWidth = textWidth(displayName, fontSize);
        float nameX = center.x - nameWidth / 2.0f;
        float nameY = center.y - radius - fontSize - 5.0f;
        DrawRectangleRounded(Rectangle{nameX - 6.0f, nameY - 2.0f, static_cast<float>(nameWidth) + 12.0f,
                                       static_cast<float>(fontSize) + 6.0f},
                             0.35f, 6, Color{14, 16, 22, 150});
        drawText(displayName, static_cast<int>(nameX), static_cast<int>(nameY), fontSize, kColorTextPrimary);
    }
}

void drawPhaseBanner(const jf::BattleController& controller) {
    bool isPlayerPhase = controller.battle().phase() == jf::Phase::PlayerPhase;
    std::string label = pick(isPlayerPhase ? "PLAYER PHASE" : "ENEMY PHASE",
                             isPlayerPhase ? kJaPlayerPhase : kJaEnemyPhase);
    Color color = isPlayerPhase ? Color{60, 120, 220, 255} : Color{200, 60, 60, 255};
    DrawRectangleGradientV(0, 0, kScreenWidth, 40, scaleColor(color, 1.12f), scaleColor(color, 0.8f));
    DrawLine(0, 40, kScreenWidth, 40, withAlpha(BLACK, 130));
    int w = textWidth(label, 22);
    drawText(label, (kScreenWidth - w) / 2, 8, 22, kColorTextPrimary);
    drawText("JOJIFrontier - Battle Prototype", 12, 44, 18, kColorTextFaint);
}

float easeInOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

// Big banner that streams in from the left edge, holds in the center, then
// continues on and streams out the right edge - triggered whenever the
// battle phase changes (Player Phase <-> Enemy Phase). Purely a rendering
// effect: it watches BattleState::phase() but never touches game state.
void drawTurnChangeAnnouncement(jf::Phase currentPhase, float dt) {
    static jf::Phase lastPhase = currentPhase;
    static jf::Phase announcedPhase = currentPhase;
    static float elapsed = -1.0f; // negative = no animation in flight

    constexpr float kSlideIn = 0.28f;
    constexpr float kHold = 0.5f;
    constexpr float kSlideOut = 0.28f;
    constexpr float kTotal = kSlideIn + kHold + kSlideOut;

    if (currentPhase != lastPhase) {
        lastPhase = currentPhase;
        announcedPhase = currentPhase;
        elapsed = 0.0f;
    }

    if (elapsed < 0.0f) return;
    elapsed += dt;
    if (elapsed >= kTotal) {
        elapsed = -1.0f;
        return;
    }

    bool isPlayerPhase = announcedPhase == jf::Phase::PlayerPhase;
    std::string label = pick(isPlayerPhase ? "PLAYER PHASE" : "ENEMY PHASE",
                             isPlayerPhase ? kJaPlayerPhase : kJaEnemyPhase);
    Color bannerColor = isPlayerPhase ? Color{35, 88, 205, 240} : Color{200, 40, 45, 240};

    int fontSize = 42;
    int textW = textWidth(label, fontSize);
    float bannerW = static_cast<float>(textW) + 140.0f;
    float bannerH = 90.0f;
    float centerX = (static_cast<float>(kScreenWidth) - bannerW) / 2.0f;
    float y = static_cast<float>(kScreenHeight) / 2.0f - bannerH / 2.0f;

    float x;
    if (elapsed < kSlideIn) {
        float t = easeInOutCubic(elapsed / kSlideIn);
        x = -bannerW + t * (centerX + bannerW);
    } else if (elapsed < kSlideIn + kHold) {
        x = centerX;
    } else {
        float t = easeInOutCubic((elapsed - kSlideIn - kHold) / kSlideOut);
        x = centerX + t * (static_cast<float>(kScreenWidth) - centerX + bannerW);
    }

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 60});
    Rectangle bannerRect{x, y, bannerW, bannerH};
    DrawRectangleRounded(Rectangle{bannerRect.x + 3.0f, bannerRect.y + 4.0f, bannerRect.width, bannerRect.height},
                         0.16f, 10, kColorShadow);
    DrawRectangleRounded(bannerRect, 0.16f, 10, scaleColor(bannerColor, 0.88f));
    DrawRectangleRounded(Rectangle{bannerRect.x, bannerRect.y, bannerRect.width, bannerRect.height * 0.5f}, 0.16f,
                         10, scaleColor(bannerColor, 1.16f));
    DrawRectangleRoundedLinesEx(bannerRect, 0.16f, 10, 3.0f, withAlpha(kColorAccentGold, 235));
    drawText(label, static_cast<int>(x + (bannerW - static_cast<float>(textW)) / 2.0f),
              static_cast<int>(y + (bannerH - static_cast<float>(fontSize)) / 2.0f), fontSize, kColorTextPrimary);
}

void drawUnitInfo(const jf::Unit& unit, int x, int y) {
    drawText(unit.name, x, y, 22, RAYWHITE);
    drawText(jf::toString(unit.unitClass), x, y + 26, 16, Color{190, 190, 205, 255});
    std::string hp = "HP(" + kJaHp + ") " + std::to_string(unit.currentHp) + " / " + std::to_string(unit.stats.maxHp);
    drawText(hp, x, y + 48, 16, RAYWHITE);
    std::string stats = "STR(" + kJaStr + ") " + std::to_string(unit.stats.strength) + "  MAG(" + kJaMag + ") " +
                         std::to_string(unit.stats.magic) + "  DEF(" + kJaDef + ") " +
                         std::to_string(unit.stats.defense) + "  RES(" + kJaRes + ") " +
                         std::to_string(unit.stats.resistance);
    drawText(stats, x, y + 70, 14, Color{190, 190, 205, 255});
    std::string weapon = kJaWeapon + "/Weapon: " + unit.weapon.name + " (Mt " + std::to_string(unit.weapon.might) +
                          ", Rng " + std::to_string(unit.minimumAttackRange()) + "-" +
                          std::to_string(unit.weapon.maxRange) + ")";
    drawText(weapon, x, y + 92, 14, Color{190, 190, 205, 255});
}

// Standalone popup shown while confirming an attack. The HUD strip at the
// bottom is too cramped/low-contrast for this much text to read clearly, so
// it gets its own panel with a dark background and a bright border instead.
void drawCombatPreviewPopup(const jf::CombatPreview& preview) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 100});

    float panelW = 480.0f;
    float panelH = 196.0f;
    float x = (static_cast<float>(kScreenWidth) - panelW) / 2.0f;
    float y = 120.0f;

    Rectangle panel{x, y, panelW, panelH};
    drawCard(panel, kColorCard, withAlpha(kColorAccentGold, 235), 0.08f);

    int tx = static_cast<int>(x) + 24;
    int ty = static_cast<int>(y) + 18;

    std::string attackerName = unitDisplayNameFor(preview.attackerName);
    std::string targetName = unitDisplayNameFor(preview.targetName);

    drawText(pick("COMBAT PREVIEW", kJaCombatPreview), tx, ty, 20, kColorAccentGold);
    ty += 34;
    drawText(attackerName + "  ->  " + targetName, tx, ty, 22, kColorTextPrimary);
    ty += 30;
    drawText(preview.weaponName, tx, ty, 15, kColorTextMuted);
    ty += 28;
    drawText(pick("Damage", kJaDamage) + ": " + std::to_string(preview.damage), tx, ty, 19,
             Color{255, 140, 120, 255});
    ty += 30;
    drawText(targetName + " HP: " + std::to_string(preview.targetHpBefore) + " -> " +
                 std::to_string(preview.targetHpAfter),
             tx, ty, 19, Color{235, 90, 90, 255});
}

struct TooltipLine {
    std::string text;
    Color color;
    int fontSize;
};

// Small panel anchored beside the cursor, flipped to stay on-screen near
// the edges. Purely informational - it never affects input handling.
void drawTooltipBox(Vector2 mouse, const std::vector<TooltipLine>& lines) {
    if (lines.empty()) return;

    constexpr float kPadding = 12.0f;
    constexpr float kLineGap = 4.0f;
    constexpr float kOffset = 18.0f;

    float maxWidth = 0.0f;
    float totalHeight = kPadding * 2.0f;
    for (const auto& line : lines) {
        maxWidth = std::max(maxWidth, static_cast<float>(textWidth(line.text, line.fontSize)));
        totalHeight += static_cast<float>(line.fontSize) + kLineGap;
    }
    float boxWidth = maxWidth + kPadding * 2.0f;

    float x = mouse.x + kOffset;
    float y = mouse.y + kOffset;
    if (x + boxWidth > static_cast<float>(kScreenWidth) - 8.0f) x = mouse.x - boxWidth - kOffset;
    if (y + totalHeight > static_cast<float>(kScreenHeight) - 8.0f) y = mouse.y - totalHeight - kOffset;
    x = std::max(x, 8.0f);
    y = std::max(y, 8.0f);

    drawCard(Rectangle{x, y, boxWidth, totalHeight}, kColorCard, kColorBorder, 0.14f);

    float ty = y + kPadding;
    for (const auto& line : lines) {
        drawText(line.text, static_cast<int>(x + kPadding), static_cast<int>(ty), line.fontSize, line.color);
        ty += static_cast<float>(line.fontSize) + kLineGap;
    }
}

// Hovering a unit shows its full stat block; hovering an empty tile shows
// that tile's terrain (movement cost / defense bonus). Suppressed while a
// bigger modal (combat preview, victory/defeat) already owns the screen.
void drawHoverInfo(const jf::BattleController& controller, Vector2 mouse) {
    jf::GridPos pos;
    if (!tileFromScreen(mouse, pos)) return;

    const jf::BattleState& battle = controller.battle();
    std::vector<TooltipLine> lines;

    if (const jf::Unit* unit = battle.unitAt(pos)) {
        Color nameColor = teamColor(unit->team);
        lines.push_back({unitDisplayNameFor(unit->name) + "  " + classNameFor(unit->unitClass), nameColor, 17});
        lines.push_back({pick("HP", kJaHp) + " " + std::to_string(unit->currentHp) + " / " +
                              std::to_string(unit->stats.maxHp),
                          Color{190, 205, 215, 255}, 14});
        lines.push_back({"STR " + std::to_string(unit->stats.strength) + "  MAG " +
                              std::to_string(unit->stats.magic) + "  DEF " + std::to_string(unit->stats.defense) +
                              "  RES " + std::to_string(unit->stats.resistance) + "  MOV " +
                              std::to_string(unit->stats.move),
                          Color{170, 180, 195, 255}, 13});
        lines.push_back({unit->weapon.name + " (Mt " + std::to_string(unit->weapon.might) + ", Rng " +
                              std::to_string(unit->minimumAttackRange()) + "-" +
                              std::to_string(unit->weapon.maxRange) + ")",
                          Color{170, 180, 195, 255}, 13});
        jf::TerrainType terrain = battle.terrainAt(pos);
        if (terrain != jf::TerrainType::Floor) {
            lines.push_back({terrainNameFor(terrain) + "  DEF +" + std::to_string(jf::defenseBonus(terrain)),
                              Color{175, 208, 184, 255}, 12});
        }
    } else {
        jf::TerrainType terrain = battle.terrainAt(pos);
        lines.push_back({terrainNameFor(terrain), RAYWHITE, 16});
        if (!jf::isPassable(terrain)) {
            lines.push_back({pick("Impassable", "通行不可"), Color{225, 120, 120, 255}, 13});
        } else {
            lines.push_back({pick("Move cost", "移動コスト") + " " + std::to_string(jf::movementCost(terrain)),
                              Color{190, 190, 205, 255}, 13});
            int def = jf::defenseBonus(terrain);
            if (def > 0) lines.push_back({"DEF +" + std::to_string(def), Color{175, 208, 184, 255}, 13});
        }
    }

    drawTooltipBox(mouse, lines);
}

void drawBattleHud(jf::GameApp& app, Vector2 mouse, bool clicked) {
    jf::BattleController& controller = app.battle();
    constexpr int hudTop = static_cast<int>(kHudY);
    constexpr int buttonWidth = 108;
    constexpr int buttonHeight = 38;
    constexpr int buttonGap = 8;
    int fifthActionX = kScreenWidth - buttonWidth - 18;
    int fourthActionX = fifthActionX - buttonWidth - buttonGap;
    int thirdActionX = fourthActionX - buttonWidth - buttonGap;
    int secondActionX = thirdActionX - buttonWidth - buttonGap;
    int firstActionX = secondActionX - buttonWidth - buttonGap;

    DrawRectangleGradientV(0, hudTop, kScreenWidth, kScreenHeight - hudTop, Color{24, 28, 39, 255},
                           Color{15, 17, 24, 255});
    DrawLine(0, hudTop, kScreenWidth, hudTop, withAlpha(kColorBorder, 200));
    drawText(app.currentMissionName() + "  " + pick("Seed", kJaSeed) + " " + std::to_string(app.expeditionSeed()),
             18, hudTop - 25, 15, kColorAccentGold);

    if (jf::Unit* selected = controller.selectedUnit()) {
        drawText(unitDisplayNameFor(selected->name) + "  " + classNameFor(selected->unitClass), 18, hudTop + 12, 19,
                 kColorTextPrimary);
        drawText("HP " + std::to_string(selected->currentHp) + "/" + std::to_string(selected->stats.maxHp),
                 18, hudTop + 42, 16, kColorTextMuted);
        std::string stats = "STR " + std::to_string(selected->stats.strength) + "   MAG " +
                            std::to_string(selected->stats.magic) + "   DEF " +
                            std::to_string(selected->stats.defense) + "   RES " +
                            std::to_string(selected->stats.resistance);
        drawText(stats, 108, hudTop + 42, 14, kColorTextFaint);
    } else {
        drawText(pick("Select a unit to act.", kJaSelectUnit), 18, hudTop + 33, 15, kColorTextMuted);
    }

    std::string stepLabel;
    switch (controller.inputState()) {
        case jf::BattleInputState::SelectMove:
            stepLabel = pick("Choose destination", kJaChooseMove);
            break;
        case jf::BattleInputState::SelectAction:
            stepLabel = pick("Choose action", kJaChooseAction);
            break;
        case jf::BattleInputState::SelectTarget:
            stepLabel = pick("Choose target", kJaChooseTarget);
            break;
        case jf::BattleInputState::SelectHealTarget:
            stepLabel = pick("Choose ally", kJaHeal);
            break;
        case jf::BattleInputState::SelectBoardTarget:
            stepLabel = pick("Place barrier", kJaPlaceBarrier);
            break;
        default:
            break;
    }
    if (!stepLabel.empty()) drawText(stepLabel, 420, hudTop + 12, 15, kColorAccentGold);

    switch (controller.inputState()) {
        case jf::BattleInputState::SelectAction:
            if (button(Rectangle{static_cast<float>(firstActionX), hudTop + 4.0f, buttonWidth, 22},
                       "Back", kJaBack, mouse, clicked)) controller.returnToMoveSelection();
            if (button(Rectangle{static_cast<float>(firstActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                       "Attack", kJaAttack, mouse, clicked))
                controller.chooseAttack();
            if (controller.selectedUnit() && jf::canHeal(controller.selectedUnit()->unitClass)) {
                if (button(Rectangle{static_cast<float>(secondActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Heal", kJaHeal, mouse, clicked))
                    controller.chooseHeal();
            } else if (controller.selectedUnit() && app.expedition().count(jf::ItemType::FirstAidKit) > 0 &&
                controller.selectedUnit()->currentHp < controller.selectedUnit()->stats.maxHp) {
                std::string potionCount = std::to_string(app.expedition().count(jf::ItemType::FirstAidKit));
                if (button(Rectangle{static_cast<float>(secondActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "First Aid " + potionCount, kJaFirstAidShort + " " + potionCount, mouse, clicked))
                    app.useBattleHealingItem(jf::ItemType::FirstAidKit);
            } else {
                std::string potionCount = std::to_string(app.expedition().count(jf::ItemType::FirstAidKit));
                disabledButton(Rectangle{static_cast<float>(secondActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                               pick("First Aid " + potionCount, kJaFirstAidShort + " " + potionCount));
            }
            if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                       "Field " + std::to_string(app.expedition().count(jf::ItemType::FieldTreatmentKit)),
                       kJaFieldShort + " " + std::to_string(app.expedition().count(jf::ItemType::FieldTreatmentKit)),
                       mouse, clicked))
                app.useBattleHealingItem(jf::ItemType::FieldTreatmentKit);
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                       "Board " + std::to_string(app.expedition().count(jf::ItemType::ProtectiveBoard)),
                       kJaBoardShort + " " + std::to_string(app.expedition().count(jf::ItemType::ProtectiveBoard)),
                       mouse, clicked))
                app.chooseProtectiveBoard();
            if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                       "Wait", kJaWait, mouse, clicked))
                controller.chooseWait();
            break;
        case jf::BattleInputState::ConfirmAttack:
            // The readable preview is drawn as its own popup (see
            // drawCombatPreviewPopup); the HUD here just keeps the buttons.
            if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 43.0f, buttonWidth, buttonHeight},
                       "Confirm", kJaConfirm, mouse, clicked))
                controller.confirmAttack();
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 43.0f, buttonWidth, buttonHeight},
                       "Cancel", kJaCancel, mouse, clicked))
                controller.cancelAttackSelection();
            break;
        case jf::BattleInputState::SelectMove:
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                       "Cancel", kJaCancel, mouse, clicked))
                controller.cancelToUnitSelect();
            break;
        case jf::BattleInputState::SelectTarget:
        case jf::BattleInputState::SelectHealTarget:
        case jf::BattleInputState::SelectBoardTarget:
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                       "Back", kJaBack, mouse, clicked))
                controller.cancelAttackSelection();
            break;
        default:
            break;
    }
}

void drawVictoryOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 190});
    drawCard(Rectangle{static_cast<float>(kScreenWidth) / 2.0f - 230.0f, 220.0f, 460.0f, 230.0f}, kColorCard,
            withAlpha(kColorAccentGold, 220), 0.1f);
    std::string title = pick("VICTORY", kJaVictory);
    drawText(title, kScreenWidth / 2 - textWidth(title, 44) / 2, 260, 44, kColorAccentGold);
    std::string subtitle = pick("Proceed to Camp", kJaProceedToCamp);
    drawText(subtitle, kScreenWidth / 2 - textWidth(subtitle, 18) / 2, 320, 18, kColorTextPrimary);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 380, 260, 50};
    if (button(rect, "Proceed to Camp", kJaProceedToCamp, mouse, clicked)) {
        app.proceedToCamp();
    }
}

void drawDefeatOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 190});
    drawCard(Rectangle{static_cast<float>(kScreenWidth) / 2.0f - 230.0f, 220.0f, 460.0f, 230.0f}, kColorCard,
            withAlpha(Color{200, 70, 70, 255}, 220), 0.1f);
    std::string title = pick("EXPEDITION FAILED", kJaDefeatTitle);
    drawText(title, kScreenWidth / 2 - textWidth(title, 38) / 2, 260, 38, Color{225, 90, 90, 255});
    std::string line = pick("All expedition loot would be lost.", kJaLootLostLine);
    drawText(line, kScreenWidth / 2 - textWidth(line, 16) / 2, 312, 16, kColorTextPrimary);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 380, 260, 50};
    if (button(rect, "Return to Base", kJaReturnToBase, mouse, clicked)) {
        app.acknowledgeDefeat();
    }
}

// Small gold accent bar preceding a section heading, matching the same
// "gold picks out something the player should notice" language used by
// the battle HUD's step label and combat preview title.
void drawSectionHeading(const std::string& text, int x, int y, int fontSize) {
    DrawRectangleRounded(Rectangle{static_cast<float>(x) - 14.0f, static_cast<float>(y) + 3.0f, 5.0f,
                                   static_cast<float>(fontSize) - 4.0f},
                         0.5f, 4, kColorAccentGold);
    drawText(text, x, y, fontSize, kColorTextPrimary);
}

void drawCampScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    DrawRectangleGradientV(0, 0, kScreenWidth, 70, Color{40, 46, 62, 255}, Color{24, 27, 38, 255});
    DrawLine(0, 70, kScreenWidth, 70, withAlpha(kColorBorder, 160));
    drawText(pick("CAMP", kJaCamp), 40, 30, 32, kColorTextPrimary);

    if (app.justSecuredLoot()) {
        drawSectionHeading(pick("Loot Secured", kJaLootSecured), 54, 90, 24);
        int y = 140;
        for (const std::string& item : app.lastSecuredLoot()) {
            drawText("- " + item, 60, y, 18, kColorTextPrimary);
            y += 26;
        }
        if (app.lastSecuredLoot().empty()) {
            drawText("(" + pick("no loot this expedition", kJaNoLootThisExpedition) + ")", 60, y, 16,
                     kColorTextMuted);
            y += 26;
        }
        Rectangle rect{40, static_cast<float>(y + 30), 260, 50};
        if (button(rect, "Continue", kJaContinue, mouse, clicked)) {
            app.acknowledgeLootSecured();
        }
        return;
    }

    int y = 90;
    drawSectionHeading(pick("Party HP", kJaPartyHp), 54, y, 20);
    y += 30;
    for (const jf::Unit& unit : app.battle().battle().units()) {
        if (unit.team != jf::Team::Player) continue;
        std::string line = unitDisplayNameFor(unit.name) + " (" + classNameFor(unit.unitClass) + "): " +
                            std::to_string(unit.currentHp) + " / " + std::to_string(unit.stats.maxHp) +
                            (unit.isAlive() ? "" : " - " + pick("Down", kJaDown));
        drawText(line, 60, y, 16, unit.isAlive() ? kColorTextPrimary : Color{225, 100, 100, 255});
        if (unit.isAlive()) {
            if (button(Rectangle{620, static_cast<float>(y - 8), 125, 30}, "First Aid", kJaFirstAidShort, mouse,
                       clicked))
                app.useCampItem(jf::ItemType::FirstAidKit, unit.id);
            if (button(Rectangle{755, static_cast<float>(y - 8), 125, 30}, "Field Kit", kJaFieldTreatmentFull, mouse,
                       clicked))
                app.useCampItem(jf::ItemType::FieldTreatmentKit, unit.id);
        } else if (button(Rectangle{620, static_cast<float>(y - 8), 160, 30}, "Rescue Pack", kJaRescuePack, mouse,
                          clicked)) {
            app.useCampItem(jf::ItemType::RescuePack, unit.id);
        }
        y += 24;
    }

    y += 20;
    drawSectionHeading(pick("Pending Expedition Loot", kJaPendingLoot), 54, y, 20);
    y += 30;
    if (app.expedition().pendingLoot.empty()) {
        drawText("(" + pick("none yet", kJaNoneYet) + ")", 60, y, 16, kColorTextMuted);
        y += 24;
    }
    for (const jf::LootStack& item : app.expedition().pendingLoot) {
        drawText("- " + item.id + " x" + std::to_string(item.quantity), 60, y, 16, kColorTextPrimary);
        y += 22;
    }
    for (const jf::DiscoveryId& discovery : app.expedition().pendingDiscoveries) {
        drawText("- " + pick("Discovery: Reconnaissance Records", "発見: 偵察資料"), 360, y - 22, 16,
                 kColorAccentGold);
        (void)discovery;
    }

    y += 10;
    std::string won = pick("Battles Won", kJaBattlesWon) + ": " + std::to_string(app.expedition().battlesWon);
    drawText(won, 40, y, 16, kColorTextPrimary);
    drawText(pick("Bag", kJaBag) + ": " + std::to_string(app.expedition().bag.size()) + " / 6", 260, y,
             16, kColorTextMuted);
    y += 30;
    DrawLine(40, y, kScreenWidth - 40, y, withAlpha(kColorBorderSoft, 200));
    y += 10;

    Rectangle continueRect{40, static_cast<float>(y), 300, 50};
    Rectangle returnRect{360, static_cast<float>(y), 260, 50};
    Rectangle rationRect{640, static_cast<float>(y), 220, 50};
    Rectangle flareRect{880, static_cast<float>(y), 220, 50};
    if (app.expeditionComplete()) {
        disabledButton(continueRect, pick("ROUTE COMPLETE", "経路完了"));
    } else {
        if (button(continueRect, "Continue Expedition", kJaContinueExpedition, mouse, clicked)) {
            app.continueExpedition();
        }
    }
    if (button(returnRect, "Return to Base", kJaReturnToBase, mouse, clicked)) {
        app.returnToBase();
    }
    if (button(rationRect, "Camp Rations " + std::to_string(app.expedition().count(jf::ItemType::CampRations)),
               kJaCampRations + " " + std::to_string(app.expedition().count(jf::ItemType::CampRations)), mouse,
               clicked))
        app.useCampItem(jf::ItemType::CampRations);
    if (button(flareRect, "Return Flare " + std::to_string(app.expedition().count(jf::ItemType::ReturnFlare)),
               kJaReturnFlare + " " + std::to_string(app.expedition().count(jf::ItemType::ReturnFlare)), mouse,
               clicked))
        app.useCampItem(jf::ItemType::ReturnFlare);
}

void drawBaseScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    drawText(pick("EXPEDITION PREPARATION", kJaExpeditionPrep), 38, 30, 30, kColorTextPrimary);
    drawSectionHeading(pick("Party - choose 4", kJaPartyChoose4), 52, 92, 20);
    int y = 125;
    for (const auto& unit : app.roster()) {
        bool selected = std::find(app.selectedPartyIds().begin(), app.selectedPartyIds().end(), unit.id) != app.selectedPartyIds().end();
        std::string label = std::string(selected ? "[X] " : "[ ] ") + unitDisplayNameFor(unit.name) + " - " +
                            classNameFor(unit.classId);
        if (button(Rectangle{40, static_cast<float>(y), 390, 40}, label, "", mouse, clicked)) app.togglePartyMember(unit.id);
        y += 45;
    }
    drawSectionHeading(pick("Supplies", kJaSupplies), 492, 92, 20);
    y = 125;
    for (const auto& item : jf::kItemCatalog) {
        std::string label = itemFullNameFor(item.type) + "  (" + pick("Add", kJaAdd) + ")";
        if (button(Rectangle{480, static_cast<float>(y), 300, 40}, label, "", mouse, clicked))
            app.addPreparedItem(item.type);
        y += 45;
    }
    drawSectionHeading(pick("Bag - 6 slots", kJaBagSlots), 842, 92, 20);
    y = 125;
    for (std::size_t i = 0; i < jf::ExpeditionState::kBagCapacity; ++i) {
        Rectangle slot{830, static_cast<float>(y), 370, 40};
        if (i < app.preparedBag().size()) {
            std::string label = itemFullNameFor(app.preparedBag()[i]) + "  (" + pick("Remove", kJaRemove) + ")";
            if (button(slot, label, "", mouse, clicked)) app.removePreparedItem(i);
        } else disabledButton(slot, pick("Empty slot", kJaEmptySlot));
        y += 45;
    }
    Rectangle start{830, 430, 370, 58};
    if (app.selectedPartyIds().size() == 4) {
        if (button(start, "Begin Expedition", kJaBeginExpedition, mouse, clicked)) app.startExpedition();
    } else disabledButton(start, pick("SELECT EXACTLY 4 UNITS", kJaSelectExactly4));

    const jf::BaseState& base = app.baseState();
    drawSectionHeading(pick("Outpost", kJaOutpost), 40, 520, 20);
    drawText(outpostStageNameFor(base.outpostStage), 40, 552, 22, kColorTextPrimary);
    Rectangle advanceRect{40, 588, 390, 46};
    if (base.outpostStage == jf::OutpostStage::Encampment &&
        jf::eligibleForOutpostStage(base, jf::OutpostStage::PioneerOutpost)) {
        if (button(advanceRect, "Advance the Outpost", kJaAdvanceOutpost, mouse, clicked)) app.advanceOutpostStage();
    } else if (base.outpostStage == jf::OutpostStage::Encampment) {
        disabledButton(advanceRect, pick("Requires bringing home the Ashveil Fang", kJaAdvanceOutpostLocked));
    }

    drawSectionHeading(pick("Discoveries", kJaDiscoveries), 492, 520, 20);
    if (base.discoveryRegistry.empty()) {
        drawText(pick("No discoveries yet", kJaNoDiscoveriesYet), 492, 552, 18, kColorTextMuted);
    } else {
        int discoveryY = 552;
        for (const jf::DiscoveryId& discovery : base.discoveryRegistry) {
            drawText(discoveryNameFor(discovery), 492, discoveryY, 18, kColorTextPrimary);
            discoveryY += 28;
        }
    }
}

void drawExplorationScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    drawText(pick("EXPLORATION", kJaExploration), 42, 30, 28, kColorAccentGold);
    drawText("Cinderwatch Gate", 42, 78, 34, kColorTextPrimary);
    drawText(pick("Beyond the collapsed gate stands an abandoned watch post. Fresh tracks mark the main path.",
                  kJaCinderwatchSituation), 42, 135, 18, kColorTextMuted);

    Rectangle frontal{70, 225, 520, 120};
    Rectangle sidePath{650, 225, 520, 120};
    if (button(frontal, "A. Frontal Advance", "A. " + kJaFrontalAdvance, mouse, clicked))
        app.chooseCinderwatchRoute(jf::ExplorationChoice::FrontalAdvance);
    drawText(pick("No attrition / Standard battle", kJaFrontalEffect), 92, 310, 15, kColorTextMuted);
    if (button(sidePath, "B. Take the Collapsed Side Path", "B. " + kJaSidePath, mouse, clicked))
        app.chooseCinderwatchRoute(jf::ExplorationChoice::CollapsedSidePath);
    drawText(pick("All living allies: HP -2 / One fewer enemy", kJaSidePathEffect), 672, 310, 15,
             kColorTextMuted);

    Rectangle scoutRect{360, 400, 560, 90};
    if (app.partyHasFrontierScout()) {
        if (button(scoutRect, "C. [Frontier Scout] Scout from High Ground", "C. " + kJaScoutRoute, mouse, clicked))
            app.chooseCinderwatchRoute(jf::ExplorationChoice::ScoutRoute);
        drawText(pick("No attrition / Freely deploy in the left 3 columns", kJaScoutRouteEffect), 382, 470, 15,
                 kColorTextMuted);
    } else {
        disabledButton(scoutRect, pick("C. [Frontier Scout] Scout from High Ground", "C. " + kJaScoutRoute));
        drawText(pick("Requires a Frontier Scout in the party", kJaScoutRouteLocked), 382, 470, 15,
                 Color{200, 110, 110, 255});
    }
}

// Which of the 4 party slots free-placement clicks currently apply to.
// Purely UI state - GameApp only knows about confirmed placements.
int gDeploymentSelectedSlot = 0;

void drawPreBattleDeploymentScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{16, 18, 26, 255});
    DrawRectangleGradientV(0, 0, kScreenWidth, 70, Color{40, 46, 62, 255}, Color{24, 27, 38, 255});
    DrawLine(0, 70, kScreenWidth, 70, withAlpha(kColorBorder, 160));
    drawText(pick("PRE-BATTLE DEPLOYMENT", kJaPreBattleDeployment), 40, 20, 26, kColorTextPrimary);
    drawText(pick("Place your 4 allies on passable tiles in the left 3 columns.", kJaDeploymentInstructions), 40,
             50, 15, kColorTextMuted);
    drawText(pick("Enemy Forces (Preview)", kJaEnemyForces), kScreenWidth - 300, 50, 15, kColorTextFaint);

    const std::vector<jf::Unit>& players = app.deploymentPlayers();
    int maxCol = app.deploymentMaxColumn();

    for (int row = 0; row < jf::kGridRows; ++row) {
        for (int col = 0; col < jf::kGridCols; ++col) {
            jf::GridPos pos{row, col};
            Rectangle rect = tileRect(pos);
            jf::TerrainType terrain = app.deploymentTerrain()[row * jf::kGridCols + col];
            drawTilePanel(rect, terrainColor(terrain));
            if (terrain == jf::TerrainType::Barrier) {
                DrawLineEx(Vector2{rect.x + 26.0f, rect.y + 12.0f},
                           Vector2{rect.x + rect.width - 26.0f, rect.y + rect.height - 18.0f}, 5.0f,
                           Color{25, 28, 34, 220});
                DrawLineEx(Vector2{rect.x + rect.width - 26.0f, rect.y + 12.0f},
                           Vector2{rect.x + 26.0f, rect.y + rect.height - 18.0f}, 5.0f, Color{25, 28, 34, 220});
            } else if (terrain == jf::TerrainType::WatchPost) {
                DrawRectangleLinesEx(Rectangle{rect.x + 10.0f, rect.y + 8.0f, rect.width - 20.0f, rect.height - 22.0f},
                                     2.0f, Color{175, 208, 184, 130});
            }
            if (col <= maxCol && jf::isPassable(terrain)) {
                DrawRectangleRec(rect, Color{58, 155, 255, 60});
            }
        }
    }

    // Enemy preview: shown so the player can weigh their free placement
    // against what they're about to face, but never clickable here.
    for (const jf::Unit& enemy : app.deploymentEnemyPreview()) {
        Rectangle rect = tileRect(enemy.position);
        Vector2 center{rect.x + rect.width / 2.0f, rect.y - kUnitRadius + 9.0f};
        DrawCircleV(center, kUnitRadius, Fade(teamColor(jf::Team::Enemy), 0.55f));
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), kUnitRadius,
                        Color{10, 14, 22, 200});
        std::string name = unitDisplayNameFor(enemy.name);
        int nw = textWidth(name, 12);
        drawText(name, static_cast<int>(center.x - nw / 2.0f), static_cast<int>(center.y - kUnitRadius - 16), 12,
                 kColorTextFaint);
    }

    // Placed allies.
    for (std::size_t i = 0; i < players.size(); ++i) {
        if (!app.isDeploymentUnitPlaced(i)) continue;
        Rectangle rect = tileRect(players[i].position);
        Vector2 center{rect.x + rect.width / 2.0f, rect.y - kUnitRadius + 9.0f};
        Color c = teamColor(jf::Team::Player);
        if (static_cast<int>(i) == gDeploymentSelectedSlot) {
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), kUnitRadius + 7.0f,
                            withAlpha(kColorAccentGold, 235));
        }
        DrawCircleV(center, kUnitRadius, c);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), kUnitRadius,
                        Color{10, 14, 22, 230});
        std::string name = unitDisplayNameFor(players[i].name);
        int nw = textWidth(name, 12);
        drawText(name, static_cast<int>(center.x - nw / 2.0f), static_cast<int>(center.y - kUnitRadius - 16), 12,
                 kColorTextPrimary);
    }

    if (clicked) {
        jf::GridPos pos;
        if (tileFromScreen(mouse, pos)) {
            if (app.placeDeploymentUnit(static_cast<std::size_t>(gDeploymentSelectedSlot), pos)) {
                for (std::size_t i = 0; i < players.size(); ++i) {
                    if (!app.isDeploymentUnitPlaced(i)) {
                        gDeploymentSelectedSlot = static_cast<int>(i);
                        break;
                    }
                }
            }
        }
    }

    int hudTop = static_cast<int>(kHudY);
    DrawRectangleGradientV(0, hudTop, kScreenWidth, kScreenHeight - hudTop, Color{24, 28, 39, 255},
                           Color{15, 17, 24, 255});
    DrawLine(0, hudTop, kScreenWidth, hudTop, withAlpha(kColorBorder, 200));

    constexpr float kSlotWidth = 300.0f;
    for (std::size_t i = 0; i < players.size(); ++i) {
        bool placed = app.isDeploymentUnitPlaced(i);
        std::string status = pick(placed ? "Placed" : "Unplaced", placed ? kJaPlaced : kJaUnplaced);
        std::string label = unitDisplayNameFor(players[i].name) + " (" + classNameFor(players[i].unitClass) +
                            ")  [" + status + "]";
        Rectangle slotRect{18.0f + static_cast<float>(i) * (kSlotWidth + 8.0f), hudTop + 8.0f, kSlotWidth, 34.0f};
        if (button(slotRect, label, "", mouse, clicked)) gDeploymentSelectedSlot = static_cast<int>(i);
        if (static_cast<int>(i) == gDeploymentSelectedSlot)
            DrawRectangleRoundedLinesEx(slotRect, 0.28f, 8, 3.0f, withAlpha(kColorAccentGold, 255));
    }

    if (!app.allDeploymentUnitsPlaced()) {
        drawText(pick("Place all 4 to begin", kJaDeployAllToStart), 18, hudTop + 54, 15, kColorTextMuted);
    }

    Rectangle backRect{static_cast<float>(kScreenWidth) - 460.0f, hudTop + 48.0f, 220.0f, 40.0f};
    Rectangle beginRect{static_cast<float>(kScreenWidth) - 230.0f, hudTop + 48.0f, 212.0f, 40.0f};
    if (button(backRect, "Back", kJaBack, mouse, clicked)) {
        app.cancelDeployment();
        gDeploymentSelectedSlot = 0;
    }
    if (app.allDeploymentUnitsPlaced()) {
        if (button(beginRect, "Begin Battle", kJaBeginBattle, mouse, clicked)) {
            app.confirmDeployment();
            gDeploymentSelectedSlot = 0;
        }
    } else {
        disabledButton(beginRect, pick("Begin Battle", kJaBeginBattle));
    }
}

// Small always-on-top corner button + modal for switching the display
// language. Purely a rendering/UI concern (see the Language enum above),
// so it lives entirely in this file and never touches GameApp/BattleState.
// Draw this last on every screen so it sits above any other overlay.
void drawSettingsOverlay(Vector2 mouse, bool clicked) {
    Rectangle cornerBtn{static_cast<float>(kScreenWidth) - 100.0f, 4.0f, 92.0f, 32.0f};
    if (button(cornerBtn, "Settings", kJaSettings, mouse, gSettingsOpen ? false : clicked)) {
        gSettingsOpen = !gSettingsOpen;
    }

    if (!gSettingsOpen) return;

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 150});

    Rectangle panel{static_cast<float>(kScreenWidth) / 2.0f - 190.0f, static_cast<float>(kScreenHeight) / 2.0f - 120.0f,
                    380.0f, 240.0f};
    drawCard(panel, kColorCard, withAlpha(kColorAccentGold, 230), 0.1f);

    drawText(pick("Settings", kJaSettings), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 22), 24,
             kColorTextPrimary);
    drawText(pick("Language", kJaLanguage), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 66), 15,
             kColorTextMuted);

    Rectangle enBtn{panel.x + 26, panel.y + 96, 150, 48};
    Rectangle jaBtn{panel.x + 26 + 150 + 16, panel.y + 96, 150, 48};

    if (button(enBtn, "English", "", mouse, clicked)) gLanguage = Language::English;
    if (button(jaBtn, kJaJapaneseNative, "", mouse, clicked)) gLanguage = Language::Japanese;

    Rectangle activeHighlight = gLanguage == Language::English ? enBtn : jaBtn;
    DrawRectangleRoundedLinesEx(activeHighlight, 0.28f, 8, 3.0f, withAlpha(kColorAccentGold, 255));

    Rectangle closeBtn{panel.x + 26, panel.y + 164, 328, 46};
    if (button(closeBtn, "Close", kJaClose, mouse, clicked)) gSettingsOpen = false;
}

void handleGridClick(jf::GameApp& app, jf::GridPos pos) {
    jf::BattleController& controller = app.battle();
    switch (controller.inputState()) {
        case jf::BattleInputState::SelectUnit: {
            if (jf::Unit* unit = controller.battle().unitAt(pos)) {
                controller.selectUnit(*unit);
            }
            break;
        }
        case jf::BattleInputState::SelectMove:
            controller.selectMoveTile(pos);
            break;
        case jf::BattleInputState::SelectTarget:
            controller.selectTargetTile(pos);
            break;
        case jf::BattleInputState::SelectHealTarget:
            controller.selectHealTarget(pos);
            break;
        case jf::BattleInputState::SelectBoardTarget:
            app.selectBoardTarget(pos);
            break;
        default:
            break;
    }
}

} // namespace

int main() {
    InitWindow(kScreenWidth, kScreenHeight, "JOJIFrontier");
    SetTargetFPS(60);
    loadAppFont();

    auto gameData = jf::loadGameData("data");
    if (!gameData) {
        // Fall back for the case where the executable is launched from a
        // different working directory than the copied data/ folder.
        gameData = jf::loadGameData("../data");
    }
    if (!gameData) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            drawText(pick("Failed to load data/ (classes.json, units.json, weapons.json).", kJaDataLoadFailed), 20,
                     20, 18, RED);
            EndDrawing();
        }
        CloseWindow();
        return 1;
    }

    jf::GameApp app(*gameData);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Vector2 mouse = GetMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        // While the Settings modal is open, it owns all clicks; nothing
        // underneath should react even though it's still drawn (dimmed).
        bool sceneClicked = clicked && !gSettingsOpen;

        app.update(dt);

        if (app.screen() == jf::Screen::Battle) {
            jf::BattleController& controller = app.battle();

            if (sceneClicked) {
                jf::GridPos pos;
                if (tileFromScreen(mouse, pos)) {
                    handleGridClick(app, pos);
                }
            }

            BeginDrawing();
            ClearBackground(Color{16, 18, 26, 255});
            drawPhaseBanner(controller);
            drawGrid(controller, dt);
            drawBattleHud(app, mouse, sceneClicked);
            drawTurnChangeAnnouncement(controller.battle().phase(), dt);

            if (controller.inputState() != jf::BattleInputState::ConfirmAttack &&
                controller.inputState() != jf::BattleInputState::Victory &&
                controller.inputState() != jf::BattleInputState::Defeat) {
                drawHoverInfo(controller, mouse);
            }

            if (controller.inputState() == jf::BattleInputState::ConfirmAttack) {
                if (auto preview = controller.pendingPreview()) {
                    drawCombatPreviewPopup(*preview);
                }
            }

            if (controller.inputState() == jf::BattleInputState::Victory) {
                drawVictoryOverlay(app, mouse, sceneClicked);
            } else if (controller.inputState() == jf::BattleInputState::Defeat) {
                drawDefeatOverlay(app, mouse, sceneClicked);
            }
            drawSettingsOverlay(mouse, clicked);
            EndDrawing();
        } else if (app.screen() == jf::Screen::Camp) {
            BeginDrawing();
            drawCampScreen(app, mouse, sceneClicked);
            drawSettingsOverlay(mouse, clicked);
            EndDrawing();
        } else if (app.screen() == jf::Screen::Exploration) {
            BeginDrawing();
            drawExplorationScreen(app, mouse, sceneClicked);
            drawSettingsOverlay(mouse, clicked);
            EndDrawing();
        } else if (app.screen() == jf::Screen::PreBattleDeployment) {
            BeginDrawing();
            drawPreBattleDeploymentScreen(app, mouse, sceneClicked);
            drawSettingsOverlay(mouse, clicked);
            EndDrawing();
        } else {
            BeginDrawing();
            drawBaseScreen(app, mouse, sceneClicked);
            drawSettingsOverlay(mouse, clicked);
            EndDrawing();
        }
    }

    if (gFontReady) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
