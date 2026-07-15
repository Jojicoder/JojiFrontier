#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/core/GameApp.hpp"
#include "jf/core/Grid.hpp"
#include "jf/data/GameData.hpp"

namespace {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 800;

Rectangle logicalViewport() {
    const float scale = std::min(GetScreenWidth() / static_cast<float>(kScreenWidth),
                                 GetScreenHeight() / static_cast<float>(kScreenHeight));
    const float width = kScreenWidth * scale;
    const float height = kScreenHeight * scale;
    return {(GetScreenWidth() - width) * 0.5f, (GetScreenHeight() - height) * 0.5f, width, height};
}

Vector2 logicalMousePosition() {
    const Rectangle viewport = logicalViewport();
    const Vector2 mouse = GetMousePosition();
    return {(mouse.x - viewport.x) * kScreenWidth / viewport.width,
            (mouse.y - viewport.y) * kScreenHeight / viewport.height};
}

void beginLogicalFrame() {
    const Rectangle viewport = logicalViewport();
    BeginDrawing();
    ClearBackground(Color{8, 10, 15, 255});
    BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                     static_cast<int>(viewport.width), static_cast<int>(viewport.height));
    Camera2D camera{};
    camera.offset = Vector2{viewport.x, viewport.y};
    camera.zoom = viewport.width / static_cast<float>(kScreenWidth);
    BeginMode2D(camera);
}

void endLogicalFrame() {
    EndMode2D();
    EndScissorMode();
    EndDrawing();
}

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
        case jf::TerrainType::Brush: return Color{64, 105, 76, 255};
        case jf::TerrainType::HerbPatch: return Color{72, 122, 91, 255};
        case jf::TerrainType::Shallows: return Color{69, 104, 122, 255};
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
const std::string kJaSkills = "スキル";
const std::string kJaPotion = "ポーション";
const std::string kJaHeal = "治療";
const std::string kJaWait = "待機";
const std::string kJaEndTurn = "ターン終了";
const std::string kJaChooseMove = "移動先を選択";
const std::string kJaChooseAction = "行動を選択";
const std::string kJaChooseItem = "使用するアイテムを選択";
const std::string kJaNoUsableItems = "使用可能なアイテムはありません";
const std::string kJaChooseSkill = "使用するスキルを選択";
const std::string kJaNoUsableSkills = "使用可能なスキルはありません";
const std::string kJaChooseTarget = "攻撃対象を選択";
const std::string kJaChooseItemTarget = "アイテムを使う味方を選択";
const std::string kJaConfirm = "決定";
const std::string kJaCancel = "キャンセル";
const std::string kJaCombatPreview = "戦闘予測";
const std::string kJaDamage = "ダメージ";
const std::string kJaHit = "命中率";
const std::string kJaEvasion = "回避";
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
const std::string kJaMaterialsLabel = "必要素材";
const std::string kJaNoMaterialCost = "素材コストなし";
const std::string kJaWindow = "ウィンドウ";
const std::string kJaMaximizeWindow = "ウィンドウを最大化";
const std::string kJaRestoreWindow = "ウィンドウを元に戻す";
const std::string kJaMaximizeShortcut = "ショートカット: F11";
const std::string kJaExpeditionSection = "遠征";
const std::string kJaRetireExpedition = "遠征リタイア";
const std::string kJaRetireExpeditionNote = "この遠征の未確定の戦利品は失われます。";
const std::string kJaSaveDataSection = "セーブデータ";
const std::string kJaExportSave = "エクスポート";
const std::string kJaImportSave = "インポート";
const std::string kJaImportConfirm = "この内容で置き換えますか？";
const std::string kJaImportApply = "インポートを実行";
const std::string kJaImportCancel = "キャンセル";
const std::string kJaImportBaseOnly = "拠点画面でのみ実行できます";
const std::string kJaImportNoFile = "importsフォルダにファイルがありません";
const std::string kJaExportOk = "エクスポートしました: ";
const std::string kJaExportFailed = "エクスポートに失敗しました: ";
const std::string kJaImportFailed = "インポートに失敗しました: ";
const std::string kJaImportApplied = "インポートを適用しました";
const std::string kJaStatusPoison = "毒";
const std::string kJaStatusBurn = "炎上";
const std::string kJaStatusMoveDown = "移動低下";
const std::string kJaStatusDefenseDown = "防御低下";
const std::string kJaStatusStagger = "よろめき";
const std::string kJaStatusRemainingProcs = "残り";
const std::string kJaStatusUntilPhaseEnd = "このPhase終了まで";
const std::string kJaStatusNextActionNoMove = "次の行動は移動不可";
const std::string kJaAttackHits = "の攻撃が";
const std::string kJaAttackHitsSuffix = "に命中！";
const std::string kJaDamageSuffix = "ダメージ！";
const std::string kJaAttackMisses = "の攻撃は";
const std::string kJaAttackMissesSuffix = "に外れた！";
const std::string kJaOnTheBrinkSuffix = "が瀕死状態に！";
const std::string kJaFallenSuffix = "が戦闘不能に！";
const std::string kJaJapaneseNative = "日本語";
const std::string kJaExpeditionPrep = "遠征準備";
const std::string kJaPartyChoose4 = "パーティ(4人選択)";
const std::string kJaSupplies = "物資";
const std::string kJaBagSlots = "荷物(6枠)";
const std::string kJaEmptySlot = "空き枠";
const std::string kJaBeginExpedition = "遠征開始";
const std::string kJaSelectExactly4 = "ちょうど4人選んでください";
const std::string kJaAdd = "追加";
const std::string kJaCraft = "製作";
const std::string kJaRemove = "削除";
const std::string kJaPlaceBarrier = "障害物を設置";
const std::string kJaBag = "荷物";
const std::string kJaBattleLocation = "戦闘地点";
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
const std::string kJaHerbThicketDiscovery = "薬草群生地の記録";
const std::string kJaAshboughForestSurveyCompleteDiscovery = "灰枝の森踏査記録";
const std::string kJaFacilities = "施設";
const std::string kJaFacilitySlots = "施設枠";
const std::string kJaUnlock = "解放";
const std::string kJaBuild = "建設";
const std::string kJaDismantle = "解体";
const std::string kJaRebuild = "再建";
const std::string kJaBuilt = "稼働中";
const std::string kJaUnlocked = "解放済み";
const std::string kJaDismantled = "解体済み(再建可)";
const std::string kJaNeedsStage = "段階: ";
const std::string kJaNeedsDiscovery = "必要な発見物が不足";
const std::string kJaNeedsFacilityBuilt = "施設が未稼働";
const std::string kJaNeedsMaterial = "素材不足: ";
const std::string kJaNoOpenSlot = "空き施設枠なし";
const std::string kJaCurrentWeapon = "現在の武器: ";
const std::string kJaEquipTrait = "調整特性を装備";
const std::string kJaUnequipTrait = "調整特性を解除";
const std::string kJaTraitLocked = "調整特性(未解放)";

// docs/implementation_roadmap.md "Phase 3.5": Base screen region select and
// Ashbough Verge-specific exploration text, so Ashbough Forest and the
// 周回・地域経路開拓 loop are reachable through normal play instead of only
// through GameApp's test-only API surface.
const std::string kJaExpeditionRegionSection = "遠征先";
const std::string kJaRegionLockedAshboughForest = "灰枝の森を攻略すると解放されます";
const std::string kJaAshboughVergeSituation = "灰枝の森の入口。絡み合う木々の奥で、狼の群れが縄張りを張っている。";
const std::string kJaAshboughFrontal = "A. 痕跡を調べる";
const std::string kJaAshboughFrontalEffect = "一部情報公開 / 通常戦闘";
const std::string kJaAshboughSidePath = "B. 急いで奥へ進む";
const std::string kJaAshboughSidePathEffect = "生存中の味方全員 HP-2 / 敵1体減少 / 通常勝利の木材なし";
const std::string kJaAshboughScoutRoute = "C. 【辺境斥候】獣道を調査";
const std::string kJaAshboughScoutRouteEffect = "全情報公開 / 左3列に自由配置 / 獣皮+1";
const std::string kJaSiteStatus = "地点状態: ";
const std::string kJaSiteUnknown = "未踏";
const std::string kJaSiteSurveyed = "踏査済み";
const std::string kJaSiteSecured = "経路確保済み";
const std::string kJaSafePassage = "安全路を進む";
const std::string kJaSafePassageEffect = "戦闘・探索3択・報酬すべて省略して先へ進みます";
const std::string kJaReconnaissance = "危険区域を再調査";
const std::string kJaReconnaissanceEffect = "新しい戦闘で通常素材のみ再取得できます（初回報酬は対象外）";

// --- Language setting -------------------------------------------------
// Purely a display concern (like the font/colors above), not part of
// battle rules, so it lives here rather than in GameApp/BattleController.
enum class Language { English, Japanese };
Language gLanguage = Language::English;
bool gSettingsOpen = false;
// Base screen sub-view toggle: purely a rendering concern (which panel the
// Base screen shows), so - like gSettingsOpen - it lives here rather than in
// GameApp. Facility node actions still validate against GameApp's own
// Screen::Base state regardless of which sub-view is currently drawn.
bool gShowFacilities = false;
// Base screen's region-select choice (docs/implementation_roadmap.md "Phase
// 3.5"). Ashbough Forest is always unlocked, so it's a safe default; the
// Begin Expedition button re-validates against GameApp before starting.
jf::RegionId gSelectedRegionId = jf::RegionId::AshboughForest;
std::optional<jf::FacilityId> gVisitedFacility;
std::optional<jf::UnitClass> gForgeCraftClass;
std::optional<std::string> gViewedUnitId;
bool gBattleItemMenuOpen = false;
bool gBattleSkillMenuOpen = false;
bool gCampItemMenuOpen = false;
std::optional<jf::ItemType> gCampSelectedItem;

// Save Data (Export/Import, docs/save_system.md). Lives alongside the other
// Settings-overlay UI state; gSaveStore itself is the same store main() uses
// for autosave, just hoisted to a global so the overlay can reach it without
// threading a parameter through every draw*Screen() call.
std::optional<jf::SaveStore> gSaveStore;
bool gAutoSaveEnabled = true;
std::optional<jf::SaveData> gPendingImport;
std::string gPendingImportFilename;
std::string gSaveStatusMessage;
std::string gSaveStatusMessageJa;
double gSaveStatusExpiresAt = 0.0;

void setSaveStatus(const std::string& en, const std::string& ja, double durationSeconds = 4.0) {
    gSaveStatusMessage = en;
    gSaveStatusMessageJa = ja;
    gSaveStatusExpiresAt = GetTime() + durationSeconds;
}

// Picks the string for whichever language is currently selected. Falls
// back to `en` if no Japanese variant was given (e.g. a proper noun).
std::string pick(const std::string& en, const std::string& ja) {
    return (gLanguage == Language::Japanese && !ja.empty()) ? ja : en;
}

// --- Japanese-capable font handling ---------------------------------------
Font gFont{};
bool gFontReady = false;

int displayFontSize(int requestedSize) {
    // Small operational text needs a stronger increase than already-large
    // titles. Keeping this centralized makes every screen and width
    // calculation use the same readable scale.
    if (requestedSize <= 14) return requestedSize + 10;
    if (requestedSize <= 20) return requestedSize + 12;
    return static_cast<int>(std::round(requestedSize * 1.30f));
}

void drawText(const std::string& text, int x, int y, int fontSize, Color color) {
    const int displaySize = displayFontSize(fontSize);
    if (gFontReady) {
        DrawTextEx(gFont, text.c_str(), Vector2{static_cast<float>(x), static_cast<float>(y)},
                   static_cast<float>(displaySize), 1.0f, color);
    } else {
        DrawText(text.c_str(), x, y, displaySize, color);
    }
}

int textWidth(const std::string& text, int fontSize) {
    const int displaySize = displayFontSize(fontSize);
    if (gFontReady) {
        return static_cast<int>(MeasureTextEx(gFont, text.c_str(), static_cast<float>(displaySize), 1.0f).x);
    }
    return MeasureText(text.c_str(), displaySize);
}

// Truncates on whole UTF-8 codepoints (never splits a multi-byte Japanese
// character) and appends "..." once the text no longer fits maxWidth. This
// is the safety net for narrow UI cells (e.g. Facilities screen node rows)
// so long labels/reasons never overlap a neighboring column.
std::string clipTextToWidth(const std::string& text, int fontSize, int maxWidth) {
    if (textWidth(text, fontSize) <= maxWidth) return text;
    const std::string ellipsis = "...";
    std::string result;
    const char* p = text.c_str();
    while (*p) {
        int bytes = 0;
        GetCodepointNext(p, &bytes);
        if (bytes <= 0) break;
        std::string candidate = result + std::string(p, static_cast<std::size_t>(bytes));
        if (textWidth(candidate + ellipsis, fontSize) > maxWidth) break;
        result = candidate;
        p += bytes;
    }
    return result.empty() ? ellipsis : result + ellipsis;
}

// Wraps on UTF-8 codepoint boundaries, so Japanese descriptions can use the
// full tooltip width without being truncated or splitting a glyph.
std::string wrapTextToWidth(const std::string& text, int fontSize, int maxWidth) {
    std::string result;
    std::string line;
    const char* p = text.c_str();
    while (*p) {
        int bytes = 0;
        int codepoint = GetCodepointNext(p, &bytes);
        if (bytes <= 0) break;
        if (codepoint == '\n') {
            result += line + "\n";
            line.clear();
            p += bytes;
            continue;
        }
        std::string glyph(p, static_cast<std::size_t>(bytes));
        if (!line.empty() && textWidth(line + glyph, fontSize) > maxWidth) {
            result += line + "\n";
            line.clear();
        }
        line += glyph;
        p += bytes;
    }
    result += line;
    return result;
}

std::vector<std::string> textLines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return lines;
}

float textLineHeight(int fontSize) { return static_cast<float>(displayFontSize(fontSize)) + 5.0f; }

std::string classNameFor(jf::UnitClass unitClass);
std::string classRoleFor(jf::UnitClass unitClass);
std::string itemFullNameFor(jf::ItemType type);
std::string itemDescriptionFor(jf::ItemType type);
std::string materialNameFor(const std::string& id);
std::string weaponNameFor(const std::string& weaponId, const std::string& englishName);

// Loads a system font that covers both ASCII and the Japanese glyphs this
// UI needs (raylib's built-in default font only covers Latin-1). Falls
// back to the default font - and thus tofu/missing glyphs for Japanese -
// if no such font is found, so the game still runs everywhere.
void loadAppFont() {
    std::string charsetSource;
    for (int c = 32; c <= 126; ++c) charsetSource += static_cast<char>(c);
    charsetSource += kJaPlayerPhase + kJaEnemyPhase + kJaSelectUnit + kJaAttack + kJaBack + kJaItems + kJaSkills +
                     kJaHeal + kJaWait + kJaEndTurn + kJaChooseMove + kJaChooseAction + kJaChooseItem + kJaNoUsableItems +
                     kJaChooseSkill + kJaNoUsableSkills + kJaChooseTarget + kJaConfirm +
                     kJaChooseItemTarget +
                      kJaCancel + kJaCombatPreview + kJaDamage + kJaHit + kJaEvasion + kJaVictory + kJaProceedToCamp +
                      kJaDefeatTitle + kJaLootLostLine + kJaReturnToBase + kJaCamp + kJaLootSecured +
                      kJaNoLootThisExpedition + kJaContinue + kJaPartyHp + kJaPendingLoot + kJaNoneYet +
                      kJaBattlesWon + kJaContinueExpedition + kJaDown + kJaHp + kJaWeapon + kJaStr + kJaMag +
                      kJaDef + kJaRes + kJaDataLoadFailed + kJaSettings + kJaLanguage + kJaClose + kJaJapaneseNative +
                      kJaWindow + kJaMaximizeWindow + kJaRestoreWindow + kJaMaximizeShortcut +
                      kJaExpeditionSection + kJaRetireExpedition + kJaRetireExpeditionNote +
                      kJaSaveDataSection + kJaExportSave + kJaImportSave + kJaImportConfirm + kJaImportApply +
                      kJaImportCancel + kJaImportBaseOnly + kJaImportNoFile + kJaExportOk + kJaExportFailed +
                      kJaImportFailed + kJaImportApplied +
                      kJaStatusPoison + kJaStatusBurn + kJaStatusMoveDown + kJaStatusDefenseDown +
                      kJaStatusStagger + kJaStatusRemainingProcs + kJaStatusUntilPhaseEnd +
                      kJaStatusNextActionNoMove +
                      kJaAttackHits + kJaAttackHitsSuffix + kJaDamageSuffix + kJaAttackMisses +
                      kJaAttackMissesSuffix + kJaOnTheBrinkSuffix + kJaFallenSuffix +
                      kJaMaterialsLabel + kJaNoMaterialCost +
                      kJaPotion + kJaExpeditionPrep + kJaPartyChoose4 + kJaSupplies + kJaBagSlots + kJaEmptySlot +
                      kJaBeginExpedition + kJaSelectExactly4 + kJaAdd + kJaCraft + kJaRemove + kJaPlaceBarrier + kJaBag +
                      kJaBattleLocation + kJaFirstAidShort + kJaFieldShort + kJaBoardShort + kJaFieldTreatmentFull +
                      kJaRescuePack + kJaCampRations + kJaReturnFlare + kJaExploration + kJaCinderwatchSituation +
                      kJaFrontalAdvance + kJaFrontalEffect + kJaSidePath + kJaSidePathEffect + kJaScoutRoute +
                      kJaScoutRouteEffect + kJaScoutRouteLocked + kJaPreBattleDeployment +
                      kJaDeploymentInstructions + kJaEnemyForces + kJaDeployAllToStart + kJaBeginBattle +
                      kJaPlaced + kJaUnplaced + kJaOutpost + kJaDiscoveries + kJaNoDiscoveriesYet +
                      kJaEncampment + kJaPioneerOutpost + kJaFrontierSettlement + kJaPioneerCity +
                      kJaAdvanceOutpost + kJaAdvanceOutpostLocked + kJaScoutNetworkDiscovery +
                      kJaFieldMedicineDiscovery + kJaReturnSignalDiscovery + kJaHerbThicketDiscovery +
                      kJaAshboughForestSurveyCompleteDiscovery + kJaFacilities +
                      kJaFacilitySlots +
                      kJaUnlock + kJaBuild + kJaDismantle + kJaRebuild + kJaBuilt + kJaUnlocked + kJaDismantled +
                      kJaNeedsStage + kJaNeedsDiscovery + kJaNeedsFacilityBuilt + kJaNeedsMaterial + kJaNoOpenSlot +
                      kJaCurrentWeapon + kJaEquipTrait + kJaUnequipTrait + kJaTraitLocked +
                      kJaExpeditionRegionSection + kJaRegionLockedAshboughForest + kJaAshboughVergeSituation +
                      kJaAshboughFrontal + kJaAshboughFrontalEffect + kJaAshboughSidePath +
                      kJaAshboughSidePathEffect + kJaAshboughScoutRoute + kJaAshboughScoutRouteEffect +
                      kJaSiteStatus + kJaSiteUnknown + kJaSiteSurveyed + kJaSiteSecured + kJaSafePassage +
                      kJaSafePassageEffect + kJaReconnaissance + kJaReconnaissanceEffect +
                      "灰枝の森沈黙した監視所群灰枝の林縁" +
                      "不明なスキルクールダウン: あとこの戦闘では使用済み" +
                      "平地灰塵瓦礫障害物監視所インパッサブル移動コスト通行不可経路完了"
                      "レオンガレスエリンミラネッサローワン国境斥候行軍槍兵野盗脱走元隊長救急セット野戦治療キット茂み薬草地点✓"
                      "次のフィールド情報経路攻略完了拠点へ帰還すると戦利品を確定します灰の街道灰地では移動しにくく瓦礫は通行できません"
                      "敵戦力体最後の信号塔地形監視所と障害物が多く防御側が有利です元隊長を確認使用対象使用可能なアイテム"
                      "司令所訓練所鍛冶場診療所工房宿舎訪れる施設一覧へ強化解放施設情報項目カーソル合わせる効果必要素材確認できます"
                      "詳細ユニット編成一覧へ装備変更能力簡易鍛冶台建設稼働変更可能ありません扱制作未実装鍛冶場へ"
                      "遠征先敵情報ルート段階的開示兵種技術戦術役割訓練施設武器変更調整特性分岐"
                      "治療具救命手段強化探索道具盤面操作工作装備仲間受入専門区画連携機能拡張未稼働"
                      "鉄剣斧槍長重迎撃姿勢候刃暁杖ランス"
                      "隣接味方防御与敵移動妨害距離攻機力高地形無視者対得負傷回復特殊能持"
                      "戦闘中野営時HP不能人全員空障害物設置利品確定即座帰還"
                      "辺境暁衛生兵不明木材獣皮薬草関門工具灰街道地図野戦医療資材台帳信号レンズ隊長印章大牙"
                      "隣接する味方に防御ボーナスを与える隣接マスの敵の移動を妨害する遠距離攻撃"
                      "隣接する敵には攻撃できない機動力が高く灰塵地形の移動ペナルティを無視する"
                      "以上移動した攻撃者に対しボーナスを得る迎撃負傷した味方を回復できる"
                      "特殊能力を持たない攻撃的な野盗上昇通過重視"
                      "戦闘地点シンダーウォッチ関門アイアンウォッチ物資庫最後信号塔"
                      "薬草の沢折れ木の縄張り森の経路は次の探索地点へ続いています地点到達"
                      "この地点の探索選択と戦闘内容は次の実装工程で追加します現在の未確定戦利品保存されています"
                      "次の地点では探索から開始します荷物そのまま引き継がれます";
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) charsetSource += node.nameJa + node.effectJa;
    for (const jf::SkillDefinition& skill : jf::skillRegistry()) charsetSource += skill.nameJa + skill.effectJa;
    for (jf::UnitClass uc : {jf::UnitClass::MarchCaptain, jf::UnitClass::VeteranGuard,
                              jf::UnitClass::WatchArcher, jf::UnitClass::FrontierScout,
                              jf::UnitClass::Spearman, jf::UnitClass::DawnChirurgeon,
                              jf::UnitClass::Bandit, jf::UnitClass::Wolf, jf::UnitClass::AshenhornBoar}) {
        charsetSource += jf::toString(uc);
    }
    const Language previousLanguage = gLanguage;
    gLanguage = Language::Japanese;
    for (jf::UnitClass uc : {jf::UnitClass::MarchCaptain, jf::UnitClass::VeteranGuard,
                              jf::UnitClass::WatchArcher, jf::UnitClass::FrontierScout,
                              jf::UnitClass::Spearman, jf::UnitClass::DawnChirurgeon,
                              jf::UnitClass::Bandit, jf::UnitClass::Wolf, jf::UnitClass::AshenhornBoar}) {
        charsetSource += classNameFor(uc) + classRoleFor(uc);
    }
    for (const jf::ItemDefinition& item : jf::kItemCatalog)
        charsetSource += itemFullNameFor(item.type) + itemDescriptionFor(item.type);
    for (const char* id : {"wood", "hide", "herb", "gate_tools", "ash_road_map", "field_medicine",
                           "watch_ledger", "signal_lens", "captains_seal", jf::kAshveilFangMaterial,
                           jf::kAshenhornFangMaterial, "quality_herb", "ashenhorn_fragment"})
        charsetSource += materialNameFor(id);
    for (const char* weaponId : {"wolf_bite", "boar_tusks"}) charsetSource += weaponNameFor(weaponId, "");
    gLanguage = previousLanguage;

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
    switch (unitClass) {
        case jf::UnitClass::MarchCaptain: return pick("March Captain", "行軍隊長");
        case jf::UnitClass::VeteranGuard: return pick("Veteran Guard", "古参守備兵");
        case jf::UnitClass::WatchArcher: return pick("Watch Archer", "監視弓兵");
        case jf::UnitClass::FrontierScout: return pick("Frontier Scout", "辺境斥候");
        case jf::UnitClass::Spearman: return pick("Spearman", "槍兵");
        case jf::UnitClass::DawnChirurgeon: return pick("Dawn Chirurgeon", "暁の衛生兵");
        case jf::UnitClass::Bandit: return pick("Bandit", "盗賊");
        case jf::UnitClass::Wolf: return pick("Wolf", "狼");
        case jf::UnitClass::AshenhornBoar: return pick("Ashenhorn Boar", "灰角大猪");
    }
    return pick("Unknown", "不明");
}

// Status-effect UI (docs/status_effects.md "UI"): one badge per currently
// active effect, in the doc's fixed display order. `glyph` is a single
// character for the small on-grid badge; `label`/`detail` are the fuller
// text used in the hover tooltip.
struct StatusBadge {
    std::string glyph;
    std::string label;
    std::string detail;
    Color color;
};

std::vector<StatusBadge> activeStatusBadges(const jf::Unit& unit) {
    std::vector<StatusBadge> badges;
    if (unit.poisonRemainingProcs > 0) {
        badges.push_back({pick("Po", "毒"), pick("Poison", kJaStatusPoison),
                          pick("Poison", kJaStatusPoison) + " (" + pick("left", kJaStatusRemainingProcs) + " " +
                              std::to_string(unit.poisonRemainingProcs) + ")",
                          Color{150, 95, 205, 255}});
    }
    if (unit.burnRemainingProcs > 0) {
        badges.push_back({pick("Bu", "炎"), pick("Burn", kJaStatusBurn),
                          pick("Burn", kJaStatusBurn) + " (" + pick("left", kJaStatusRemainingProcs) + " " +
                              std::to_string(unit.burnRemainingProcs) + ")",
                          Color{235, 120, 60, 255}});
    }
    if (unit.moveDownActive) {
        badges.push_back({pick("Mv", "遅"), pick("Move Down", kJaStatusMoveDown),
                          pick("Move Down", kJaStatusMoveDown) + " (" + pick("until phase end", kJaStatusUntilPhaseEnd) + ")",
                          Color{90, 150, 210, 255}});
    }
    if (unit.defenseDownActive) {
        badges.push_back({pick("Df", "弱"), pick("Def Down", kJaStatusDefenseDown),
                          pick("Def Down", kJaStatusDefenseDown) + " (" + pick("until phase end", kJaStatusUntilPhaseEnd) + ")",
                          Color{210, 90, 90, 255}});
    }
    if (unit.staggerActive) {
        badges.push_back({pick("St", "怯"), pick("Stagger", kJaStatusStagger),
                          pick("Stagger", kJaStatusStagger) + " (" + pick("next action", kJaStatusNextActionNoMove) + ")",
                          Color{205, 195, 90, 255}});
    }
    return badges;
}

std::string terrainNameFor(jf::TerrainType terrain) {
    switch (terrain) {
        case jf::TerrainType::Floor: return pick("Floor", "平地");
        case jf::TerrainType::Ash: return pick("Ash", "灰塵");
        case jf::TerrainType::Rubble: return pick("Rubble", "瓦礫");
        case jf::TerrainType::Barrier: return pick("Barrier", "障害物");
        case jf::TerrainType::WatchPost: return pick("Watch Post", "監視所");
        case jf::TerrainType::Brush: return pick("Brush", "茂み");
        case jf::TerrainType::HerbPatch: return pick("Herb Patch", "薬草地点");
        case jf::TerrainType::Shallows: return pick("Shallows", "浅瀬");
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
        {"Nessa", "ネッサ"},
        {"Rowan", "ローワン"},
        {"Frontier Scout", "辺境斥候"},
        {"March Spearman", "行軍槍兵"},
        {"Raider", "野盗"},
        {"Watch Archer", "監視弓兵"},
        {"Deserter Spearman", "脱走槍兵"},
        {"Former Captain", "元隊長"},
        {"Wolf", "狼"},
        {"Ashenhorn Boar", "灰角大猪"},
    };
    auto it = table.find(englishName);
    return (gLanguage == Language::Japanese && it != table.end()) ? it->second : englishName;
}

std::string materialNameFor(const std::string& id) {
    static const std::unordered_map<std::string, std::string> japanese = {
        {"wood", "木材"},
        {"hide", "獣皮"},
        {"herb", "薬草"},
        {"gate_tools", "関門工具"},
        {"ash_road_map", "灰街道の地図"},
        {"field_medicine", "野戦医療資材"},
        {"watch_ledger", "監視所の台帳"},
        {"signal_lens", "信号レンズ"},
        {"captains_seal", "隊長の印章"},
        {jf::kAshveilFangMaterial, "灰霧の大牙"},
        {jf::kAshenhornFangMaterial, "灰角の大牙"},
        {"quality_herb", "上質な薬草"},
        {"ashenhorn_fragment", "灰角の欠片"},
    };
    auto it = japanese.find(id);
    return gLanguage == Language::Japanese && it != japanese.end() ? it->second : id;
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

// Weapon names come from data/weapons.json as plain English strings, same as
// unit/enemy names - look up the Japanese half by the stable weapon id here.
std::string weaponNameFor(const std::string& weaponId, const std::string& englishName) {
    static const std::unordered_map<std::string, std::string> table = {
        {"iron_sword", "鉄の剣"},   {"iron_lance", "鉄のランス"}, {"iron_axe", "鉄の斧"},
        {"watch_bow", "監視弓"},    {"scout_blade", "斥候の刃"},  {"dawn_staff", "暁の杖"},
        {"iron_spear", "鉄の槍"},   {"long_spear", "長槍"},       {"heavy_spear", "重槍"},
        {"guard_spear", "迎撃槍"},  {"wolf_bite", "牙"},          {"boar_tusks", "牙(大猪)"},
    };
    auto it = table.find(weaponId);
    return (gLanguage == Language::Japanese && it != table.end()) ? it->second : englishName;
}

// One-line role summary for a unit class, matching its actual mechanical
// trait (see jf::hasBrace/hasZoneOfControl/etc. in UnitClass.cpp) - shown in
// the roster hover tooltip.
std::string classRoleFor(jf::UnitClass unitClass) {
    switch (unitClass) {
        case jf::UnitClass::MarchCaptain:
            return pick("Grants a defense bonus to adjacent allies.", "隣接する味方の防御を上昇させる。");
        case jf::UnitClass::VeteranGuard:
            return pick("Blocks enemy movement through adjacent tiles (Zone of Control).",
                        "隣接するマスを敵が通過するのを妨害する。");
        case jf::UnitClass::WatchArcher:
            return pick("Ranged attacker; cannot strike adjacent enemies.",
                        "遠距離攻撃。隣接する敵には攻撃できない。");
        case jf::UnitClass::FrontierScout:
            return pick("High mobility; ignores the Ash terrain movement penalty.",
                        "機動力が高く、灰塵地形の移動ペナルティを無視する。");
        case jf::UnitClass::Spearman:
            return pick("Gains a bonus against attackers who moved 2+ tiles (Brace).",
                        "2マス以上移動して攻撃してきた敵に対し、防御が上昇する。");
        case jf::UnitClass::DawnChirurgeon:
            return pick("Can heal adjacent wounded allies.", "隣接する負傷した味方を回復できる。");
        case jf::UnitClass::Bandit:
            return pick("Aggressive raider with no special ability.", "特殊能力を持たない、攻撃重視の盗賊。");
        case jf::UnitClass::Wolf:
            return pick("Hunts in a pack; hesitates to close in alone.", "群れで狩る。単独では迂闊に近づかない。");
        case jf::UnitClass::AshenhornBoar:
            return pick("Boss: telegraphed charges and sweeping attacks; enrages at half HP.",
                        "ボス。予告突進と薙ぎ払いを使う。HPが半分以下で激昂する。");
    }
    return "";
}

// Bilingual counterpart to jf::kItemCatalog's (English-only) descriptions.
std::string itemDescriptionFor(jf::ItemType type) {
    switch (type) {
        case jf::ItemType::FirstAidKit:
            return pick("Restore 20 HP at camp.", "野営時のみ、味方1人のHPを20回復する。");
        case jf::ItemType::FieldTreatmentKit:
            return pick("Restore 10 HP in battle or camp.", "戦闘中または野営時にHPを10回復する。");
        case jf::ItemType::RescuePack:
            return pick("Revive one ally at camp with 25% HP.", "野営時に戦闘不能の味方1人をHP25%で復帰させる。");
        case jf::ItemType::CampRations:
            return pick("Restore 5 HP to every living ally at camp.", "野営時に生存中の味方全員のHPを5回復する。");
        case jf::ItemType::ProtectiveBoard:
            return pick("Place a barrier on an adjacent empty tile.", "隣接する空きマスに障害物を設置する。");
        case jf::ItemType::ReturnFlare:
            return pick("Secure loot and return from camp immediately.", "戦利品を確定し、野営から即座に帰還する。");
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

// Short form used only in the Facilities screen's cramped node-row action
// zone (~130px) - the full name is always available in the node's hover
// tooltip, so nothing is actually lost by abbreviating here.
std::string outpostStageShortNameFor(jf::OutpostStage stage) {
    switch (stage) {
        case jf::OutpostStage::Encampment: return pick("Camp", "野営地");
        case jf::OutpostStage::PioneerOutpost: return pick("Outpost", "拠点");
        case jf::OutpostStage::FrontierSettlement: return pick("Settlement", "集落");
        case jf::OutpostStage::PioneerCity: return pick("City", "都市");
    }
    return "";
}

// Discovery IDs come straight from BaseState's data-layer constants; this is
// purely the display-language lookup, same pattern as itemFullNameFor.
std::string discoveryNameFor(const jf::DiscoveryId& id) {
    if (id == jf::kCinderwatchReconDiscovery) return pick("Scout Network Records", kJaScoutNetworkDiscovery);
    if (id == jf::kFieldMedicineDiscovery) return pick("Field Medicine Records", kJaFieldMedicineDiscovery);
    if (id == jf::kReturnSignalDiscovery) return pick("Return Signal Records", kJaReturnSignalDiscovery);
    if (id == jf::kHerbThicketDiscovery) return pick("Herb Thicket Records", kJaHerbThicketDiscovery);
    if (id == jf::kAshboughForestSurveyCompleteDiscovery)
        return pick("Ashbough Forest Survey Records", kJaAshboughForestSurveyCompleteDiscovery);
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
// Attack lunge: a brief, purely-visual "body slam" toward the target and
// back, triggered off BattleController::attackEventId() (see drawGrid).
// Never touches battle logic/positions - only offsets where the unit is
// drawn for a fraction of a second.
struct AttackLunge {
    Vector2 direction{0.0f, 0.0f};
    float timer = 0.0f;
};

constexpr float kAttackLungeDuration = 0.26f;
constexpr float kAttackLungeDistance = 18.0f;

std::unordered_map<std::string, AttackLunge>& attackLunges() {
    static std::unordered_map<std::string, AttackLunge> lunges;
    return lunges;
}

void triggerAttackLunge(const jf::Unit& attacker, const jf::Unit& target) {
    Vector2 from = unitLogicalCenter(attacker);
    Vector2 to = unitLogicalCenter(target);
    Vector2 direction{to.x - from.x, to.y - from.y};
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0.001f) {
        direction.x /= length;
        direction.y /= length;
    }
    attackLunges()[attacker.id] = AttackLunge{direction, kAttackLungeDuration};
}

// Bump curve: 0 -> 1 -> 0 over the lunge's lifetime, so the unit lurches
// toward the target and eases back rather than snapping.
Vector2 attackLungeOffset(const std::string& unitId, float dt) {
    auto& lunges = attackLunges();
    auto it = lunges.find(unitId);
    if (it == lunges.end()) return Vector2{0.0f, 0.0f};

    AttackLunge& lunge = it->second;
    lunge.timer -= dt;
    if (lunge.timer <= 0.0f) {
        lunges.erase(it);
        return Vector2{0.0f, 0.0f};
    }
    float progress = 1.0f - (lunge.timer / kAttackLungeDuration);
    float bump = std::sin(progress * 3.14159265f);
    return Vector2{lunge.direction.x * kAttackLungeDistance * bump,
                  lunge.direction.y * kAttackLungeDistance * bump};
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
    Vector2 lunge = attackLungeOffset(unit.id, dt);
    auto it = visualCenters.find(unit.id);
    if (it == visualCenters.end()) {
        visualCenters.emplace(unit.id, target);
        return Vector2{target.x + lunge.x, target.y + lunge.y};
    }

    Vector2& current = it->second;
    float t = 1.0f - std::exp(-kMoveLerpRate * dt);
    current.x += (target.x - current.x) * t;
    current.y += (target.y - current.y) * t;
    return Vector2{current.x + lunge.x, current.y + lunge.y};
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
    while (fontSize > 12 && textWidth(label, fontSize) > static_cast<int>(rect.width - 14.0f)) --fontSize;
    int w = textWidth(label, fontSize);
    drawText(label, static_cast<int>(rect.x + (rect.width - w) / 2),
             static_cast<int>(rect.y + (rect.height - displayFontSize(fontSize)) / 2), fontSize, kColorTextPrimary);
    return hovered && mousePressed;
}

void disabledButton(Rectangle rect, const std::string& label) {
    DrawRectangleRounded(rect, 0.28f, 8, Color{30, 33, 41, 255});
    DrawRectangleRoundedLinesEx(rect, 0.28f, 8, 1.5f, kColorBorderSoft);
    int fontSize = 14;
    while (fontSize > 11 && textWidth(label, fontSize) > static_cast<int>(rect.width - 12.0f)) --fontSize;
    int w = textWidth(label, fontSize);
    drawText(label, static_cast<int>(rect.x + (rect.width - w) / 2),
             static_cast<int>(rect.y + (rect.height - displayFontSize(fontSize)) / 2), fontSize, kColorTextFaint);
}

// Battle messages: brief fading banners for combat feedback (hit/miss/
// damage, a unit falling, a unit dropping to critical HP) triggered off the
// same BattleController::attackEventId() the lunge animation uses. Purely
// informational overlay text - never affects input handling or state.
struct BattleMessage {
    std::string text;
    Color color;
    float timer = 0.0f;
};

constexpr float kBattleMessageDuration = 1.8f;
constexpr float kBattleMessageFadeTime = 0.35f;
constexpr std::size_t kMaxBattleMessages = 3;

std::vector<BattleMessage>& battleMessages() {
    static std::vector<BattleMessage> messages;
    return messages;
}

void pushBattleMessage(const std::string& text, Color color) {
    auto& messages = battleMessages();
    messages.push_back({text, color, kBattleMessageDuration});
    if (messages.size() > kMaxBattleMessages) messages.erase(messages.begin());
}

void drawBattleMessages(float dt) {
    auto& messages = battleMessages();
    float y = 84.0f;
    for (auto it = messages.begin(); it != messages.end();) {
        it->timer -= dt;
        if (it->timer <= 0.0f) {
            it = messages.erase(it);
            continue;
        }
        float alpha = it->timer < kBattleMessageFadeTime ? (it->timer / kBattleMessageFadeTime) : 1.0f;
        int fontSize = 20;
        int textW = textWidth(it->text, fontSize);
        float x = (static_cast<float>(kScreenWidth) - textW) / 2.0f;
        Rectangle box{x - 16.0f, y, static_cast<float>(textW) + 32.0f, 34.0f};
        drawCard(box, withAlpha(kColorCard, static_cast<unsigned char>(220 * alpha)),
                withAlpha(kColorAccentGold, static_cast<unsigned char>(200 * alpha)), 0.24f);
        drawText(it->text, static_cast<int>(x), static_cast<int>(y + 7), fontSize,
                withAlpha(it->color, static_cast<unsigned char>(255 * alpha)));
        y += 40.0f;
        ++it;
    }
}

// Only warns once per drop below the threshold (tracked per unit id) so it
// doesn't repeat on every subsequent hit while a unit stays critical.
std::unordered_set<std::string>& lowHpWarnedUnits() {
    static std::unordered_set<std::string> warned;
    return warned;
}
constexpr float kLowHpRatioThreshold = 0.25f;

std::string hitMessageText(const std::string& attackerName, const std::string& targetName, int damage) {
    return pick(attackerName + "'s attack hits " + targetName + " for " + std::to_string(damage) + "!",
               attackerName + kJaAttackHits + targetName + kJaAttackHitsSuffix + std::to_string(damage) +
                   kJaDamageSuffix);
}

std::string missMessageText(const std::string& attackerName, const std::string& targetName) {
    return pick(attackerName + "'s attack misses " + targetName + "!",
               attackerName + kJaAttackMisses + targetName + kJaAttackMissesSuffix);
}

std::string lowHpMessageText(const std::string& targetName) {
    return pick(targetName + " is on the brink of collapse!", targetName + kJaOnTheBrinkSuffix);
}

std::string fallenMessageText(const std::string& targetName) {
    return pick(targetName + " has fallen!", targetName + kJaFallenSuffix);
}

void drawGrid(const jf::BattleController& controller, float dt) {
    const jf::BattleState& battle = controller.battle();

    // Detect a newly-resolved attack (player or enemy) and kick off its
    // lunge animation exactly once, whichever screen/state notices it first.
    static std::uint64_t lastSeenAttackEvent = 0;
    if (controller.attackEventId() != lastSeenAttackEvent) {
        lastSeenAttackEvent = controller.attackEventId();
        if (const jf::Unit* attacker = controller.lastAttacker()) {
            if (const jf::Unit* target = controller.lastAttackTarget()) {
                triggerAttackLunge(*attacker, *target);

                std::string attackerName = unitDisplayNameFor(attacker->name);
                std::string targetName = unitDisplayNameFor(target->name);
                if (controller.lastAttackHit()) {
                    pushBattleMessage(hitMessageText(attackerName, targetName, controller.lastDamage()),
                                      Color{255, 205, 120, 255});
                    if (!target->isAlive()) {
                        pushBattleMessage(fallenMessageText(targetName), Color{225, 90, 90, 255});
                        lowHpWarnedUnits().erase(target->id);
                    } else {
                        float hpRatio = static_cast<float>(target->currentHp) /
                                       static_cast<float>(target->stats.maxHp);
                        if (hpRatio <= kLowHpRatioThreshold && lowHpWarnedUnits().insert(target->id).second) {
                            pushBattleMessage(lowHpMessageText(targetName), Color{235, 140, 90, 255});
                        }
                    }
                } else {
                    pushBattleMessage(missMessageText(attackerName, targetName), Color{190, 198, 210, 255});
                }
            }
        }
    }

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
            } else if (terrain == jf::TerrainType::Brush) {
                DrawCircle(static_cast<int>(rect.x + rect.width * 0.38f), static_cast<int>(rect.y + 24.0f), 8.0f,
                           Color{43, 82, 56, 210});
                DrawCircle(static_cast<int>(rect.x + rect.width * 0.52f), static_cast<int>(rect.y + 19.0f), 10.0f,
                           Color{53, 98, 65, 220});
                DrawCircle(static_cast<int>(rect.x + rect.width * 0.65f), static_cast<int>(rect.y + 25.0f), 8.0f,
                           Color{43, 82, 56, 210});
            } else if (terrain == jf::TerrainType::HerbPatch) {
                DrawCircle(static_cast<int>(rect.x + rect.width * 0.5f), static_cast<int>(rect.y + 22.0f), 7.0f,
                           Color{105, 190, 105, 230});
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
            if (containsTile(controller.itemTargetTiles(), pos))
                DrawRectangleRec(rect, Color{70, 210, 145, 175});
            if (containsTile(controller.boardTargetTiles(), pos))
                DrawRectangleRec(rect, Color{220, 185, 70, 150});
            if (containsTile(controller.skillTargetTiles(), pos))
                DrawRectangleRec(rect, Color{90, 200, 235, 165});
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

        // Status-effect badges (docs/status_effects.md "UI"): up to 3 icons,
        // a 4th+ collapses into a "+N" badge instead of growing the row.
        std::vector<StatusBadge> badges = activeStatusBadges(unit);
        if (!badges.empty()) {
            constexpr float kBadgeRadius = 9.0f;
            constexpr float kBadgeGap = 4.0f;
            int shown = static_cast<int>(std::min<std::size_t>(badges.size(), 3));
            float rowWidth = shown * (kBadgeRadius * 2.0f) + (shown - 1) * kBadgeGap;
            float badgeY = hpBack.y + hpBack.height + 6.0f + kBadgeRadius;
            float badgeX = center.x - rowWidth / 2.0f + kBadgeRadius;
            for (int i = 0; i < shown; ++i) {
                bool overflowSlot = (i == 2 && badges.size() > 3);
                Vector2 badgeCenter{badgeX, badgeY};
                DrawCircleV(badgeCenter, kBadgeRadius, overflowSlot ? Color{70, 74, 84, 255} : badges[i].color);
                DrawCircleLines(static_cast<int>(badgeCenter.x), static_cast<int>(badgeCenter.y), kBadgeRadius,
                                withAlpha(BLACK, 150));
                std::string glyph = overflowSlot ? "+" + std::to_string(static_cast<int>(badges.size()) - 2)
                                                  : badges[i].glyph;
                int glyphWidth = textWidth(glyph, 9);
                drawText(glyph, static_cast<int>(badgeCenter.x - glyphWidth / 2.0f),
                         static_cast<int>(badgeCenter.y - 6.0f), 9, RAYWHITE);
                badgeX += kBadgeRadius * 2.0f + kBadgeGap;
            }
        }

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

    drawBattleMessages(dt);
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
    // Only dim the battlefield above the HUD strip - the HUD (with the
    // Confirm/Cancel buttons for this exact popup) is drawn before this and
    // must stay fully lit/interactive-looking, not washed out underneath it.
    DrawRectangle(0, 0, kScreenWidth, static_cast<int>(kHudY), Color{0, 0, 0, 100});

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
    drawText(weaponNameFor(preview.weaponId, preview.weaponName), tx, ty, 15, kColorTextMuted);
    ty += 28;
    drawText(pick("Damage", kJaDamage) + ": " + std::to_string(preview.damage), tx, ty, 19,
             Color{255, 140, 120, 255});
    ty += 30;
    drawText(pick("Hit", kJaHit) + ": " + std::to_string(preview.hitChance) + "%", tx + 235, ty - 30, 17,
             preview.hitChance < 100 ? kColorAccentGold : kColorTextMuted);
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
    constexpr float kOffset = 18.0f;
    constexpr float kMaxTextWidth = 560.0f;

    std::vector<TooltipLine> wrappedLines;
    for (const TooltipLine& line : lines) {
        const std::string wrapped = wrapTextToWidth(line.text, line.fontSize, static_cast<int>(kMaxTextWidth));
        for (const std::string& visualLine : textLines(wrapped)) {
            wrappedLines.push_back(TooltipLine{visualLine, line.color, line.fontSize});
        }
    }

    float maxWidth = 0.0f;
    float totalHeight = kPadding * 2.0f;
    for (const auto& line : wrappedLines) {
        maxWidth = std::max(maxWidth, static_cast<float>(textWidth(line.text, line.fontSize)));
        totalHeight += textLineHeight(line.fontSize);
    }
    float boxWidth = std::min(kMaxTextWidth, maxWidth) + kPadding * 2.0f;

    float x = mouse.x + kOffset;
    float y = mouse.y + kOffset;
    if (x + boxWidth > static_cast<float>(kScreenWidth) - 8.0f) x = mouse.x - boxWidth - kOffset;
    if (y + totalHeight > static_cast<float>(kScreenHeight) - 8.0f) y = mouse.y - totalHeight - kOffset;
    x = std::max(x, 8.0f);
    y = std::max(y, 8.0f);

    drawCard(Rectangle{x, y, boxWidth, totalHeight}, kColorCard, kColorBorder, 0.14f);

    float ty = y + kPadding;
    for (const auto& line : wrappedLines) {
        drawText(line.text, static_cast<int>(x + kPadding), static_cast<int>(ty), line.fontSize, line.color);
        ty += textLineHeight(line.fontSize);
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
                              std::to_string(unit->stats.magic) + "  DEF " + std::to_string(unit->effectiveDefense()) +
                              "  RES " + std::to_string(unit->stats.resistance) + "  MOV " +
                              std::to_string(unit->effectiveMove()),
                          Color{170, 180, 195, 255}, 13});
        lines.push_back({weaponNameFor(unit->weapon.id, unit->weapon.name) + " (Mt " +
                              std::to_string(unit->weapon.might) + ", Rng " +
                              std::to_string(unit->minimumAttackRange()) + "-" +
                              std::to_string(unit->weapon.maxRange) + ")",
                          Color{170, 180, 195, 255}, 13});
        for (const StatusBadge& badge : activeStatusBadges(*unit)) {
            lines.push_back({badge.detail, badge.color, 12});
        }
        jf::TerrainType terrain = battle.terrainAt(pos);
        if (terrain != jf::TerrainType::Floor) {
            std::string effect = terrainNameFor(terrain);
            if (jf::defenseBonus(terrain) > 0) effect += "  DEF +" + std::to_string(jf::defenseBonus(terrain));
            if (jf::evasionBonus(terrain) > 0)
                effect += "  " + pick("Evasion", kJaEvasion) + " +" + std::to_string(jf::evasionBonus(terrain)) + "%";
            if (terrain == jf::TerrainType::HerbPatch) effect += "  HP +5";
            lines.push_back({effect, Color{175, 208, 184, 255}, 12});
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
            int evasion = jf::evasionBonus(terrain);
            if (evasion > 0)
                lines.push_back({pick("Evasion", kJaEvasion) + " +" + std::to_string(evasion) + "%",
                                 Color{175, 208, 184, 255}, 13});
            if (terrain == jf::TerrainType::HerbPatch)
                lines.push_back({pick("Heal 5 HP after acting", "行動終了時にHP5回復"),
                                 Color{130, 210, 145, 255}, 13});
        }
    }

    drawTooltipBox(mouse, lines);
}

void drawBattleHud(jf::GameApp& app, Vector2 mouse, bool clicked) {
    jf::BattleController& controller = app.battle();
    if (controller.inputState() != jf::BattleInputState::SelectAction &&
        controller.inputState() != jf::BattleInputState::SelectUnit) {
        gBattleItemMenuOpen = false;
        gBattleSkillMenuOpen = false;
    }
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
    DrawRectangle(0, hudTop - 44, kScreenWidth, 44, Color{16, 20, 29, 245});
    DrawLine(0, hudTop - 44, kScreenWidth, hudTop - 44, withAlpha(kColorBorderSoft, 180));
    std::string missionName = pick(app.currentMissionName(), app.currentMissionNameJa());
    drawText(pick("Battle Location: ", kJaBattleLocation + ": ") + missionName,
             18, hudTop - 36, 17, kColorAccentGold);

    if (jf::Unit* selected = controller.selectedUnit()) {
        drawText(unitDisplayNameFor(selected->name) + "  " + classNameFor(selected->unitClass), 18, hudTop + 12, 19,
                 kColorTextPrimary);
        const std::string hpText = "HP " + std::to_string(selected->currentHp) + "/" +
                                   std::to_string(selected->stats.maxHp);
        drawText(hpText, 18, hudTop + 42, 16, kColorTextMuted);
        std::string stats = "STR " + std::to_string(selected->stats.strength) + "   MAG " +
                            std::to_string(selected->stats.magic) + "   DEF " +
                            std::to_string(selected->stats.defense) + "   RES " +
                            std::to_string(selected->stats.resistance);
        drawText(stats, 18 + textWidth(hpText, 16) + 28, hudTop + 42, 14, kColorTextFaint);
    } else {
        drawText(pick("Select a unit to act.", kJaSelectUnit), 18, hudTop + 33, 15, kColorTextMuted);
    }

    std::string stepLabel;
    switch (controller.inputState()) {
        case jf::BattleInputState::SelectMove:
            stepLabel = pick("Choose destination", kJaChooseMove);
            break;
        case jf::BattleInputState::SelectAction:
            if (gBattleItemMenuOpen) stepLabel = pick("Choose an item", kJaChooseItem);
            else if (gBattleSkillMenuOpen) stepLabel = pick("Choose a skill", kJaChooseSkill);
            else stepLabel = pick("Choose action", kJaChooseAction);
            break;
        case jf::BattleInputState::SelectItemTarget:
            stepLabel = pick("Choose an ally to use the item on", kJaChooseItemTarget);
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
        case jf::BattleInputState::SelectSkillTarget:
            stepLabel = pick("Choose a skill target", kJaChooseTarget);
            break;
        default:
            break;
    }
    if (!stepLabel.empty()) drawText(stepLabel, 420, hudTop + 12, 15, kColorAccentGold);

    switch (controller.inputState()) {
        case jf::BattleInputState::SelectUnit:
            if (!gBattleItemMenuOpen) {
                if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Items", kJaItems, mouse, clicked)) {
                    gBattleItemMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "End Turn", kJaEndTurn, mouse, clicked)) {
                    controller.endPlayerTurn();
                }
                break;
            }
            {
                int itemX = firstActionX;
                int usableItemCount = 0;
                const bool hasHealingTarget = std::any_of(
                    controller.battle().units().begin(), controller.battle().units().end(), [](const jf::Unit& unit) {
                        return unit.team == jf::Team::Player && unit.isAlive() && !unit.hasActed &&
                               unit.currentHp < unit.stats.maxHp;
                    });
                const auto drawNeutralHealingItem = [&](jf::ItemType type, const std::string& en,
                                                        const std::string& ja) {
                    const int count = app.expedition().count(type);
                    if (!hasHealingTarget || count <= 0) return;
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, buttonWidth, buttonHeight},
                               en + " " + std::to_string(count), ja + " " + std::to_string(count), mouse, clicked) &&
                        app.chooseNeutralBattleHealingItem(type)) {
                        gBattleItemMenuOpen = false;
                    }
                    itemX += buttonWidth + buttonGap;
                    ++usableItemCount;
                };
                drawNeutralHealingItem(jf::ItemType::FieldTreatmentKit, "Field Kit", kJaFieldShort);
                if (usableItemCount == 0) {
                    drawText(pick("No usable items", kJaNoUsableItems), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Back", kJaBack, mouse, clicked)) {
                    gBattleItemMenuOpen = false;
                }
            }
            break;
        case jf::BattleInputState::SelectAction:
            if (!gBattleItemMenuOpen && !gBattleSkillMenuOpen) {
                if (button(Rectangle{static_cast<float>(firstActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Attack", kJaAttack, mouse, clicked)) {
                    controller.chooseAttack();
                }
                if (button(Rectangle{static_cast<float>(secondActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Skills", kJaSkills, mouse, clicked)) {
                    gBattleSkillMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Items", kJaItems, mouse, clicked)) {
                    gBattleItemMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Wait", kJaWait, mouse, clicked)) {
                    controller.chooseWait();
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Back", kJaBack, mouse, clicked)) {
                    controller.returnToMoveSelection();
                }
                break;
            }

            if (gBattleSkillMenuOpen) {
                jf::Unit* selected = controller.selectedUnit();
                bool anySkillShown = false;
                if (selected && jf::canHeal(selected->unitClass)) {
                    anySkillShown = true;
                    if (button(Rectangle{static_cast<float>(firstActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                               "Heal", kJaHeal, mouse, clicked)) {
                        controller.chooseHeal();
                        if (controller.inputState() != jf::BattleInputState::SelectAction) {
                            gBattleSkillMenuOpen = false;
                        }
                    }
                }
                // docs/implementation_roadmap.md M4 item 1: the 2 equipped
                // Tier-1 skill slots, shown per docs/skill_system.md "使用
                //不能スキルは非表示にせず、理由付きで無効表示" - an equipped
                // but currently-unusable skill still shows, grayed out, with
                // its reason on hover, rather than disappearing.
                if (selected) {
                    const auto skills = controller.selectedUnitSkills();
                    const int slotX[2] = {secondActionX, thirdActionX};
                    for (std::size_t i = 0; i < skills.size(); ++i) {
                        if (skills[i].skillId.empty()) continue;
                        anySkillShown = true;
                        const jf::SkillDefinition* def = jf::findSkill(skills[i].skillId);
                        std::string label = def ? pick(def->nameEn, def->nameJa) : skills[i].skillId;
                        Rectangle rect{static_cast<float>(slotX[i]), hudTop + 29.0f, buttonWidth, buttonHeight};
                        if (skills[i].available) {
                            if (button(rect, label, label, mouse, clicked)) {
                                controller.chooseSkill(static_cast<int>(i));
                                if (controller.inputState() != jf::BattleInputState::SelectAction) {
                                    gBattleSkillMenuOpen = false;
                                }
                            }
                        } else {
                            disabledButton(rect, label);
                            if (CheckCollisionPointRec(mouse, rect)) {
                                drawText(pick(skills[i].reasonEn, skills[i].reasonJa), slotX[i],
                                         static_cast<int>(hudTop) + 74, 12, kColorTextMuted);
                            }
                        }
                    }
                }
                if (!anySkillShown) {
                    drawText(pick("No usable skills", kJaNoUsableSkills), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Back", kJaBack, mouse, clicked)) {
                    gBattleSkillMenuOpen = false;
                }
                break;
            }

            {
                jf::Unit* selected = controller.selectedUnit();
                const bool needsHealing = selected && selected->currentHp < selected->stats.maxHp;
                int itemX = firstActionX;
                int usableItemCount = 0;
                const auto drawHealingItem = [&](jf::ItemType type, const std::string& en, const std::string& ja) {
                    const int count = app.expedition().count(type);
                    if (!needsHealing || count <= 0) return;
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, buttonWidth, buttonHeight},
                               en + " " + std::to_string(count), ja + " " + std::to_string(count), mouse, clicked) &&
                        app.useBattleHealingItem(type)) {
                        gBattleItemMenuOpen = false;
                    }
                    itemX += buttonWidth + buttonGap;
                    ++usableItemCount;
                };
                drawHealingItem(jf::ItemType::FieldTreatmentKit, "Field Kit", kJaFieldShort);

                const int boardCount = app.expedition().count(jf::ItemType::ProtectiveBoard);
                if (boardCount > 0) {
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, buttonWidth, buttonHeight},
                               "Board " + std::to_string(boardCount),
                               kJaBoardShort + " " + std::to_string(boardCount), mouse, clicked) &&
                        app.chooseProtectiveBoard()) {
                        gBattleItemMenuOpen = false;
                    }
                    ++usableItemCount;
                }
                if (usableItemCount == 0) {
                    drawText(pick("No usable items", kJaNoUsableItems), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, buttonWidth, buttonHeight},
                           "Back", kJaBack, mouse, clicked)) {
                    gBattleItemMenuOpen = false;
                }
            }
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
        case jf::BattleInputState::SelectItemTarget:
        case jf::BattleInputState::SelectBoardTarget:
        case jf::BattleInputState::SelectSkillTarget:
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
    drawCard(Rectangle{static_cast<float>(kScreenWidth) / 2.0f - 230.0f, 220.0f, 460.0f, 200.0f}, kColorCard,
            withAlpha(kColorAccentGold, 220), 0.1f);
    std::string title = pick("VICTORY", kJaVictory);
    drawText(title, kScreenWidth / 2 - textWidth(title, 44) / 2, 260, 44, kColorAccentGold);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 345, 260, 50};
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
            drawText("- " + materialNameFor(item), 60, y, 18, kColorTextPrimary);
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
        y += 24;
    }

    Rectangle nextFieldBox{650, 88, 580, 210};
    drawCard(nextFieldBox, Color{22, 27, 38, 255}, withAlpha(kColorAccentGold, 210), 0.05f);
    drawText(pick("NEXT FIELD", "次のフィールド情報"), 674, 108, 20, kColorAccentGold);
    if (app.expeditionComplete()) {
        drawText(pick("Route complete", "経路攻略完了"), 674, 150, 20, kColorTextPrimary);
        drawText(pick("Return to base to secure the expedition loot.", "拠点へ帰還すると戦利品を確定します。"),
                 674, 190, 15, kColorTextMuted);
    } else if (app.expedition().regionId == jf::RegionId::AshboughForest) {
        drawText(app.nextMissionNameJa().value_or("次地点"), 674, 146, 22, kColorTextPrimary);
        drawText(pick("The next site begins with exploration.", "次の地点では探索から開始します。"),
                 674, 184, 15, kColorTextMuted);
        drawText(pick("Party HP, bag contents, and pending loot carry over.",
                      "HP・荷物・未確定戦利品はそのまま引き継がれます。"),
                 674, 216, 14, kColorTextMuted);
        // Same "Scout Network" gate as the Exploration screen's enemy
        // preview (docs/campaign_balance.md "敵種...の事前公開" is meant to be
        // earned progression, not a free Camp-screen giveaway).
        if (app.scoutNetworkUnlocked()) {
            const auto roster = app.nextSiteEnemyRosterNames();
            if (roster && !roster->empty()) {
                drawText(pick("Enemy Forces (Scout Network)", kJaEnemyForces), 674, 244, 14, kColorAccentGold);
                int enemyX = 674;
                for (const std::string& name : *roster) {
                    std::string label = unitDisplayNameFor(name);
                    drawText(label, enemyX, 264, 13, kColorTextFaint);
                    enemyX += textWidth(label, 13) + 20;
                }
            }
        }
    } else if (app.expedition().stageIndex == 0) {
        drawText(pick("Ironwatch Stores", "アイアンウォッチ物資庫"), 674, 146, 22, kColorTextPrimary);
        drawText(pick("Field: Ash Road", "地形: 灰の街道"), 674, 184, 15, kColorTextMuted);
        drawText(pick("Ash slows movement, and rubble is impassable.",
                      "灰地では移動しにくく、瓦礫は通行できません。"), 674, 216, 14, kColorTextMuted);
        drawText(pick("Enemy force: 4 units", "敵戦力: 4体"), 674, 246, 14, kColorTextFaint);
    } else {
        drawText(pick("The Last Signal", "最後の信号塔"), 674, 146, 22, kColorTextPrimary);
        drawText(pick("Field: Signal Tower", "地形: 信号塔"), 674, 184, 15, kColorTextMuted);
        drawText(pick("Watch posts and barriers favor defensive positions.",
                      "監視所と障害物が多く、防御側が有利な地形です。"), 674, 216, 14, kColorTextMuted);
        drawText(pick("Enemy force: 4 units / Former Captain", "敵戦力: 4体 / 元隊長を確認"),
                 674, 246, 14, kColorTextFaint);
    }

    y += 20;
    drawSectionHeading(pick("Pending Expedition Loot", kJaPendingLoot), 54, y, 20);
    y += 30;
    if (app.expedition().pendingLoot.empty()) {
        drawText("(" + pick("none yet", kJaNoneYet) + ")", 60, y, 16, kColorTextMuted);
        y += 24;
    }
    for (const jf::LootStack& item : app.expedition().pendingLoot) {
        drawText("- " + materialNameFor(item.id) + " x" + std::to_string(item.quantity), 60, y, 16,
                 kColorTextPrimary);
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
    // docs/campaign_balance.md "情報と安全路を持ち帰る正規の進行にする": the
    // continue-vs-return decision needs the remaining bag contents visible
    // at a glance, not just a slot count - the Items submenu below only
    // lists items usable on the party's current HP state, which hides
    // anything held in reserve.
    {
        int bagY = y + 24;
        static const jf::ItemType kBagItemKinds[] = {
            jf::ItemType::FirstAidKit,  jf::ItemType::FieldTreatmentKit, jf::ItemType::RescuePack,
            jf::ItemType::CampRations,  jf::ItemType::ProtectiveBoard,   jf::ItemType::ReturnFlare,
        };
        int bagX = 260;
        for (jf::ItemType type : kBagItemKinds) {
            const int count = app.expedition().count(type);
            if (count <= 0) continue;
            std::string label = itemFullNameFor(type) + " x" + std::to_string(count);
            drawText(label, bagX, bagY, 13, kColorTextFaint);
            bagX += textWidth(label, 13) + 18;
        }
    }
    constexpr float commandY = 724.0f;
    DrawLine(40, commandY - 14, kScreenWidth - 40, commandY - 14, withAlpha(kColorBorderSoft, 200));
    Rectangle continueRect{40, commandY, 360, 50};
    Rectangle returnRect{420, commandY, 360, 50};
    Rectangle itemsRect{800, commandY, 360, 50};
    if (!gCampItemMenuOpen) {
        if (app.expeditionComplete()) {
            disabledButton(continueRect, pick("ROUTE COMPLETE", "経路完了"));
        } else {
            if (button(continueRect, "Continue Expedition", kJaContinueExpedition, mouse, clicked)) {
                app.continueExpedition();
            }
        }
        if (button(returnRect, "Return to Base", kJaReturnToBase, mouse, clicked)) {
            gCampItemMenuOpen = false;
            gCampSelectedItem.reset();
            app.returnToBase();
        }
        if (button(itemsRect, "Items", kJaItems, mouse, clicked)) {
            gCampItemMenuOpen = true;
            gCampSelectedItem.reset();
        }
    }

    if (!gCampItemMenuOpen) return;

    Rectangle itemPanel{620, 300, 610, 390};
    DrawRectangle(0, 70, kScreenWidth, kScreenHeight - 70, Color{0, 0, 0, 105});
    drawCard(itemPanel, Color{20, 25, 36, 255}, withAlpha(kColorAccentGold, 230), 0.05f);
    drawText(gCampSelectedItem ? pick("CHOOSE TARGET", "使用対象を選択")
                               : pick("USABLE ITEMS", "使用可能なアイテム"),
             646, 324, 21, kColorAccentGold);

    const auto& units = app.battle().battle().units();
    const bool hasWounded = std::any_of(units.begin(), units.end(), [](const jf::Unit& unit) {
        return unit.team == jf::Team::Player && unit.isAlive() && unit.currentHp < unit.stats.maxHp;
    });
    const bool hasDefeated = std::any_of(units.begin(), units.end(), [](const jf::Unit& unit) {
        return unit.team == jf::Team::Player && !unit.isAlive();
    });

    if (!gCampSelectedItem) {
        int itemY = 372;
        int usableCount = 0;
        const auto itemChoice = [&](jf::ItemType type, bool usable) {
            const int count = app.expedition().count(type);
            if (!usable || count <= 0) return;
            if (button(Rectangle{646, static_cast<float>(itemY), 430, 48},
                       itemFullNameFor(type) + "  x" + std::to_string(count), "", mouse, clicked)) {
                if (type == jf::ItemType::CampRations) {
                    if (app.useCampItem(type)) gCampItemMenuOpen = false;
                } else if (type == jf::ItemType::ReturnFlare) {
                    if (app.useCampItem(type)) gCampItemMenuOpen = false;
                } else {
                    gCampSelectedItem = type;
                }
            }
            itemY += 56;
            ++usableCount;
        };
        itemChoice(jf::ItemType::FirstAidKit, hasWounded);
        itemChoice(jf::ItemType::FieldTreatmentKit, hasWounded);
        itemChoice(jf::ItemType::RescuePack, hasDefeated);
        itemChoice(jf::ItemType::CampRations, hasWounded);
        itemChoice(jf::ItemType::ReturnFlare, true);
        if (usableCount == 0)
            drawText(pick("No usable items", kJaNoUsableItems), 646, 390, 16, kColorTextMuted);
    } else {
        int targetY = 372;
        for (const jf::Unit& unit : units) {
            if (unit.team != jf::Team::Player) continue;
            const bool rescue = *gCampSelectedItem == jf::ItemType::RescuePack;
            const bool valid = rescue ? !unit.isAlive() : unit.isAlive() && unit.currentHp < unit.stats.maxHp;
            if (!valid) continue;
            std::string label = unitDisplayNameFor(unit.name) + "  HP " + std::to_string(unit.currentHp) + "/" +
                                std::to_string(unit.stats.maxHp);
            if (button(Rectangle{646, static_cast<float>(targetY), 430, 48}, label, "", mouse, clicked) &&
                app.useCampItem(*gCampSelectedItem, unit.id)) {
                gCampSelectedItem.reset();
                gCampItemMenuOpen = false;
                break;
            }
            targetY += 58;
        }
    }

    if (button(Rectangle{1090, 626, 116, 44}, "Back", kJaBack, mouse, clicked)) {
        if (gCampSelectedItem) gCampSelectedItem.reset();
        else gCampItemMenuOpen = false;
    }
}

void drawBaseScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    drawText(pick("EXPEDITION PREPARATION", kJaExpeditionPrep), 38, 30, 30, kColorTextPrimary);
    Rectangle facilitiesRect{static_cast<float>(kScreenWidth) - 218.0f, 4.0f, 110.0f, 32.0f};
    if (button(facilitiesRect, "Facilities", kJaFacilities, mouse, clicked)) {
        gVisitedFacility.reset();
        gForgeCraftClass.reset();
        gShowFacilities = true;
    }
    std::vector<TooltipLine> hoverLines;

    drawSectionHeading(pick("Party - choose 4", kJaPartyChoose4), 52, 92, 20);
    int y = 125;
    for (const auto& unit : app.roster()) {
        bool selected = std::find(app.selectedPartyIds().begin(), app.selectedPartyIds().end(), unit.id) != app.selectedPartyIds().end();
        std::string label = std::string(selected ? "[✓] " : "[ ] ") + unitDisplayNameFor(unit.name) + " - " +
                            classNameFor(unit.classId);
        Rectangle rowRect{40, static_cast<float>(y), 300, 40};
        Rectangle detailRect{348, static_cast<float>(y), 82, 40};
        if (button(rowRect, label, "", mouse, clicked)) app.togglePartyMember(unit.id);
        if (button(detailRect, "Details", "詳細", mouse, clicked)) gViewedUnitId = unit.id;
        if (CheckCollisionPointRec(mouse, rowRect)) {
            const jf::Stats& stats = app.gameData().classDefinition(unit.classId).baseStats;
            hoverLines = {
                {unitDisplayNameFor(unit.name) + "  " + classNameFor(unit.classId), kColorAccentGold, 17},
                {classRoleFor(unit.classId), kColorTextMuted, 13},
                {"HP " + std::to_string(stats.maxHp) + "  STR " + std::to_string(stats.strength) + "  MAG " +
                     std::to_string(stats.magic) + "  DEF " + std::to_string(stats.defense) + "  RES " +
                     std::to_string(stats.resistance) + "  MOV " + std::to_string(stats.move),
                 Color{170, 180, 195, 255}, 13},
            };
        }
        y += 45;
    }
    drawSectionHeading(pick("Supplies", kJaSupplies), 492, 92, 20);
    y = 125;
    for (const auto& item : jf::kItemCatalog) {
        const int owned = app.baseState().ownedItemCount(item.type);
        const std::vector<jf::ItemCraftCost> cost = jf::itemCraftCost(item.type);
        bool affordable = true;
        std::string costLabel;
        for (const jf::ItemCraftCost& line : cost) {
            if (app.baseState().storageCount(line.materialId) < line.quantity) affordable = false;
            if (!costLabel.empty()) costLabel += " ";
            costLabel += materialNameFor(line.materialId) + std::to_string(line.quantity);
        }

        std::string ownedLabel = itemFullNameFor(item.type) + " x" + std::to_string(owned);
        Rectangle nameRect{480, static_cast<float>(y) + 10, 145, 20};
        drawText(ownedLabel, static_cast<int>(nameRect.x), static_cast<int>(nameRect.y), 14, kColorTextPrimary);

        Rectangle craftRect{628, static_cast<float>(y), 87, 40};
        if (affordable) {
            if (button(craftRect, "Craft", kJaCraft, mouse, clicked)) app.craftItem(item.type);
        } else {
            disabledButton(craftRect, pick("Craft", kJaCraft));
        }

        Rectangle addRect{719, static_cast<float>(y), 61, 40};
        if (owned > 0) {
            if (button(addRect, "Add", kJaAdd, mouse, clicked)) app.addPreparedItem(item.type);
        } else {
            disabledButton(addRect, pick("Add", kJaAdd));
        }

        if (CheckCollisionPointRec(mouse, nameRect) || CheckCollisionPointRec(mouse, craftRect)) {
            hoverLines = {
                {itemFullNameFor(item.type), kColorAccentGold, 16},
                {itemDescriptionFor(item.type), kColorTextMuted, 13},
                {pick("Required materials", kJaMaterialsLabel) + ": " + costLabel, kColorTextMuted, 13},
            };
        } else if (CheckCollisionPointRec(mouse, addRect)) {
            hoverLines = {
                {itemFullNameFor(item.type), kColorAccentGold, 16},
                {itemDescriptionFor(item.type), kColorTextMuted, 13},
            };
        }
        y += 45;
    }

    drawSectionHeading(pick("Expedition Region", kJaExpeditionRegionSection), 480, 405, 18);
    y = 433;
    for (const auto& summary : app.regionSummaries()) {
        Rectangle rowRect{480, static_cast<float>(y), 300, 40};
        std::string marker = summary.id == gSelectedRegionId ? "> " : "";
        if (summary.unlocked) {
            if (button(rowRect, marker + summary.displayNameEn, marker + summary.displayNameJa, mouse, clicked))
                gSelectedRegionId = summary.id;
        } else {
            disabledButton(rowRect, pick(summary.displayNameEn, summary.displayNameJa));
            if (CheckCollisionPointRec(mouse, rowRect)) {
                hoverLines = {
                    {pick(summary.displayNameEn, summary.displayNameJa), kColorAccentGold, 16},
                    {pick("Unlocks after clearing Ashbough Forest", kJaRegionLockedAshboughForest), kColorTextMuted, 13},
                };
            }
        }
        y += 45;
    }

    drawSectionHeading(pick("Bag - 6 slots", kJaBagSlots), 842, 92, 20);
    y = 125;
    for (std::size_t i = 0; i < jf::ExpeditionState::kBagCapacity; ++i) {
        Rectangle slot{830, static_cast<float>(y), 370, 40};
        if (i < app.preparedBag().size()) {
            std::string label = itemFullNameFor(app.preparedBag()[i]) + "  (" + pick("Remove", kJaRemove) + ")";
            if (button(slot, label, "", mouse, clicked)) app.removePreparedItem(i);
            if (CheckCollisionPointRec(mouse, slot)) {
                hoverLines = {
                    {itemFullNameFor(app.preparedBag()[i]), kColorAccentGold, 16},
                    {itemDescriptionFor(app.preparedBag()[i]), kColorTextMuted, 13},
                };
            }
        } else disabledButton(slot, pick("Empty slot", kJaEmptySlot));
        y += 45;
    }
    Rectangle start{830, 430, 370, 58};
    if (app.selectedPartyIds().size() == 4) {
        if (button(start, "Begin Expedition", kJaBeginExpedition, mouse, clicked)) {
            jf::RegionId toStart = app.isRegionUnlocked(gSelectedRegionId) ? gSelectedRegionId : jf::RegionId::AshboughForest;
            app.startExpedition(toStart);
        }
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

    drawTooltipBox(mouse, hoverLines);
}

// docs/exploration_system.md "周回と地域経路の開拓" Japanese labels for
// SiteAccessState - purely a display concern, so it lives here rather than
// on the enum itself.
std::string siteAccessLabel(jf::SiteAccessState state) {
    switch (state) {
        case jf::SiteAccessState::Unknown: return pick("Unexplored", kJaSiteUnknown);
        case jf::SiteAccessState::Surveyed: return pick("Surveyed", kJaSiteSurveyed);
        case jf::SiteAccessState::Secured: return pick("Secured", kJaSiteSecured);
    }
    return pick("Unexplored", kJaSiteUnknown);
}

void drawExplorationScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    drawText(pick("EXPLORATION", kJaExploration), 42, 30, 28, kColorAccentGold);
    drawText(pick(app.currentMissionName(), app.currentMissionNameJa()), 42, 78, 34, kColorTextPrimary);

    const bool isAshbough = app.expedition().regionId == jf::RegionId::AshboughForest;
    drawText(isAshbough && app.currentMissionNameJa() == "灰枝の林縁"
                 ? pick("The Ashbough Forest begins here. Wolves hold the tangled verge ahead.",
                        kJaAshboughVergeSituation)
                 : isAshbough
                       ? pick("The route opens into the next forest site.", "森の経路は次の探索地点へ続いています。")
                        : pick("Beyond the collapsed gate stands an abandoned watch post. Fresh tracks mark the main path.",
                                kJaCinderwatchSituation),
             42, 135, 18, kColorTextMuted);
    {
        std::string statusText = pick("Site status: ", kJaSiteStatus) + siteAccessLabel(app.currentSiteAccess());
        int statusWidth = textWidth(statusText, 16);
        drawText(statusText, kScreenWidth - statusWidth - 42, 34, 16, kColorAccentGold);
    }

    if (!app.currentSiteContentImplemented()) {
        Rectangle pendingBox{160, 260, 960, 180};
        drawCard(pendingBox, Color{22, 27, 38, 255}, withAlpha(kColorAccentGold, 180), 0.04f);
        drawText(pick("SITE REACHED", "地点到達"), 194, 294, 20, kColorAccentGold);
        drawText(pick("Exploration choices and battle content will be added in the next implementation phase.",
                      "この地点の探索選択と戦闘内容は次の実装工程で追加します。"),
                 194, 344, 18, kColorTextPrimary);
        drawText(pick("Current expedition state has been checkpointed.",
                      "現在のHP・荷物・未確定戦利品は保存されています。"),
                 194, 386, 16, kColorTextMuted);
        return;
    }

    if (app.currentSiteAccess() == jf::SiteAccessState::Secured) {
        // docs/exploration_system.md "確保済み地点の通過": no battle, no
        // exploration choice, no reward for the safe route; a fresh battle
        // for ordinary-material-only rewards for reconnaissance.
        Rectangle safeRect{160, 260, 960, 120};
        if (button(safeRect, "Take the Safe Route", kJaSafePassage, mouse, clicked)) app.chooseSafePassage();
        drawText(pick("No battle, no exploration choice, no reward", kJaSafePassageEffect), 182, 345, 16,
                 kColorTextMuted);

        Rectangle reconRect{160, 410, 960, 120};
        if (button(reconRect, "Reconnoiter the Danger Zone", kJaReconnaissance, mouse, clicked))
            app.chooseReconnaissance();
        drawText(pick("A fresh battle for ordinary materials only (no first-time rewards)", kJaReconnaissanceEffect),
                 182, 495, 16, kColorTextMuted);
        return;
    }

    // Command Post "Scout Network" node effect: reveal what's waiting ahead
    // regardless of which route gets picked.
    if (app.scoutNetworkUnlocked()) {
        drawText(pick("Enemy Forces (Scout Network)", kJaEnemyForces), 42, 168, 16, kColorAccentGold);
        int enemyX = 42;
        for (const jf::Unit& enemy : app.explorationEnemyPreview()) {
            drawText(unitDisplayNameFor(enemy.name), enemyX, 192, 15, kColorTextMuted);
            enemyX += textWidth(unitDisplayNameFor(enemy.name), 15) + 26;
        }
    }

    Rectangle frontal{70, 225, 520, 120};
    Rectangle sidePath{650, 225, 520, 120};
    if (button(frontal, isAshbough ? "A. Investigate the Tracks" : "A. Frontal Advance",
               isAshbough ? kJaAshboughFrontal : "A. " + kJaFrontalAdvance, mouse, clicked))
        app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance);
    drawText(isAshbough ? pick("Partial intel / Standard battle", kJaAshboughFrontalEffect)
                        : pick("No attrition / Standard battle", kJaFrontalEffect),
             92, 310, 15, kColorTextMuted);
    if (button(sidePath, isAshbough ? "B. Hurry Onward" : "B. Take the Collapsed Side Path",
               isAshbough ? kJaAshboughSidePath : "B. " + kJaSidePath, mouse, clicked))
        app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath);
    drawText(isAshbough
                 ? pick("All living allies: HP -2 / One fewer enemy / no wood from a normal win",
                        kJaAshboughSidePathEffect)
                 : pick("All living allies: HP -2 / One fewer enemy", kJaSidePathEffect),
             672, 310, 15, kColorTextMuted);

    Rectangle scoutRect{360, 400, 560, 90};
    std::string scoutLabelEn = isAshbough ? "C. [Frontier Scout] Survey the Beast Trail"
                                          : "C. [Frontier Scout] Scout from High Ground";
    std::string scoutLabelJa = "C. " + (isAshbough ? kJaAshboughScoutRoute : kJaScoutRoute);
    if (app.partyHasFrontierScout()) {
        if (button(scoutRect, scoutLabelEn, scoutLabelJa, mouse, clicked))
            app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute);
        drawText(isAshbough ? pick("Full intel / Freely deploy in the left 3 columns / +1 hide",
                                    kJaAshboughScoutRouteEffect)
                            : pick("No attrition / Freely deploy in the left 3 columns", kJaScoutRouteEffect),
                 382, 470, 15, kColorTextMuted);
    } else {
        disabledButton(scoutRect, pick(scoutLabelEn, scoutLabelJa));
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
    drawText(pick("Place your 4 allies in the left 3 columns.", kJaDeploymentInstructions), 40,
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
            } else if (terrain == jf::TerrainType::Brush) {
                DrawCircle(static_cast<int>(rect.x + rect.width * 0.38f), static_cast<int>(rect.y + 24.0f), 8.0f,
                           Color{43, 82, 56, 210});
                DrawCircle(static_cast<int>(rect.x + rect.width * 0.52f), static_cast<int>(rect.y + 19.0f), 10.0f,
                           Color{53, 98, 65, 220});
                DrawCircle(static_cast<int>(rect.x + rect.width * 0.65f), static_cast<int>(rect.y + 25.0f), 8.0f,
                           Color{43, 82, 56, 210});
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

std::string facilityIdNameFor(jf::FacilityId id) {
    switch (id) {
        case jf::FacilityId::CommandPost: return pick("Command Post", "司令所");
        case jf::FacilityId::TrainingGround: return pick("Training Ground", "訓練所");
        case jf::FacilityId::Forge: return pick("Forge", "鍛冶場");
        case jf::FacilityId::Infirmary: return pick("Infirmary", "診療所");
        case jf::FacilityId::Workshop: return pick("Workshop", "工房");
        case jf::FacilityId::Barracks: return pick("Barracks", "宿舎");
    }
    return "";
}

std::string facilityRoleFor(jf::FacilityId id) {
    switch (id) {
        case jf::FacilityId::CommandPost:
            return pick("Reveals routes, enemy forces, and\ndeeper-region intelligence.",
                        "遠征先、敵情報、ルート情報を\n段階的に開示する施設。");
        case jf::FacilityId::TrainingGround:
            return pick("Unlocks class techniques and new\ntactical roles for the roster.",
                        "兵種技術と新しい戦術役割を\n解放する訓練施設。");
        case jf::FacilityId::Forge:
            return pick("Changes weapons, tuning traits,\nand horizontal weapon branches.",
                        "武器変更、調整特性、武器分岐を\n扱う鍛冶施設。");
        case jf::FacilityId::Infirmary:
            return pick("Improves medicine and survival\noptions used during expeditions.",
                        "遠征中に使う治療具と救命手段を\n強化する施設。");
        case jf::FacilityId::Workshop:
            return pick("Produces exploration tools and\nbattlefield-control equipment.",
                        "探索道具と盤面を操作する\n工作装備を作る施設。");
        case jf::FacilityId::Barracks:
            return pick("Expands recruitment, specialist\nhousing, and companion links.",
                        "仲間の受け入れ、専門区画、\n連携機能を拡張する施設。");
    }
    return "";
}

int facilityLevel(const jf::BaseState& base, jf::FacilityId facility) {
    int level = 0;
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.facility == facility && base.unlockedNodeIds.count(node.id)) ++level;
    }
    return level;
}

bool facilityIsActive(const jf::BaseState& base, jf::FacilityId facility) {
    bool hasSlottedRoot = false;
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.facility != facility || !node.occupiesFacilitySlot) continue;
        hasSlottedRoot = true;
        if (base.builtNodeIds.count(node.id)) return true;
    }
    if (hasSlottedRoot) return false;
    return facilityLevel(base, facility) > 0;
}

void drawFacilityTooltip(jf::FacilityId facility, const jf::BaseState& base, Vector2 mouse) {
    constexpr float width = 560.0f;
    const std::string role = wrapTextToWidth(facilityRoleFor(facility), 13, static_cast<int>(width - 44.0f));
    const int roleLines = static_cast<int>(textLines(role).size());
    const float height = 132.0f + roleLines * textLineHeight(13) + 18.0f;
    float x = std::min(mouse.x + 16.0f, kScreenWidth - width - 12.0f);
    float y = std::min(mouse.y + 16.0f, kScreenHeight - height - 12.0f);
    Rectangle rect{x, y, width, height};
    drawCard(rect, Color{19, 23, 33, 255}, kColorAccentGold, 0.06f);
    const std::string title = facilityIdNameFor(facility) + "  Lv " + std::to_string(facilityLevel(base, facility));
    drawText(title, static_cast<int>(x + 22), static_cast<int>(y + 18), 18, kColorAccentGold);
    const bool active = facilityIsActive(base, facility);
    drawText(active ? pick("ACTIVE", "稼働中") : pick("INACTIVE", "未稼働"),
             static_cast<int>(x + 22), static_cast<int>(y + 62), 12,
             active ? Color{105, 205, 145, 255} : Color{180, 125, 125, 255});
    DrawLineEx(Vector2{x + 22, y + 100}, Vector2{x + width - 22, y + 100}, 1.5f, kColorBorderSoft);
    drawText(role, static_cast<int>(x + 22), static_cast<int>(y + 116), 13, kColorTextPrimary);
}

// Forward declaration: defined below, but the tooltip (drawn earlier in the
// file) needs the full, non-abbreviated reason text too.
std::string facilityNodeBlockedReason(const jf::BaseState& base, const jf::FacilityNode& node, bool compact);

// Hover tooltip for a single facility node: its effect summary plus every
// required material with a live have/need count (green once satisfied, red
// while short), so the player can see exactly what a node does and what it
// still needs without clicking Unlock/Build first. If the node is currently
// locked, the full (untruncated) reason is shown too - the node-row action
// zone is only ~130px wide and has to abbreviate, but the tooltip never does.
void drawFacilityNodeTooltip(const jf::FacilityNode& node, const jf::BaseState& base, Vector2 mouse) {
    constexpr float width = 700.0f;
    const std::string effect = wrapTextToWidth(pick(node.effectEn, node.effectJa), 13,
                                               static_cast<int>(width - 44.0f));
    const int effectLineCount = 1 + static_cast<int>(std::count(effect.begin(), effect.end(), '\n'));
    const int materialLineCount = node.materialCosts.empty() ? 1 : static_cast<int>(node.materialCosts.size());
    const bool unlocked = base.unlockedNodeIds.count(node.id) > 0;
    const std::string reason = unlocked ? "" : wrapTextToWidth(facilityNodeBlockedReason(base, node, false), 13,
                                                               static_cast<int>(width - 44.0f));
    const int reasonLineCount = reason.empty() ? 0 : static_cast<int>(textLines(reason).size());
    const float effectHeight = effectLineCount * textLineHeight(13);
    const float reasonHeight = reasonLineCount * textLineHeight(13);
    const float height = 72.0f + effectHeight + 42.0f + materialLineCount * 32.0f + 18.0f + reasonHeight;
    float x = std::min(mouse.x + 16.0f, kScreenWidth - width - 12.0f);
    float y = std::min(mouse.y + 16.0f, kScreenHeight - height - 12.0f);
    Rectangle rect{x, y, width, height};
    drawCard(rect, Color{19, 23, 33, 255}, kColorAccentGold, 0.06f);

    drawText(pick(node.nameEn, node.nameJa), static_cast<int>(x + 22), static_cast<int>(y + 16), 17,
             kColorAccentGold);
    drawText(effect, static_cast<int>(x + 22), static_cast<int>(y + 56), 13, kColorTextPrimary);

    float rowY = y + 56 + effectHeight + 14.0f;
    DrawLineEx(Vector2{x + 22, rowY - 8}, Vector2{x + width - 22, rowY - 8}, 1.5f, kColorBorderSoft);
    drawText(pick("Required materials", kJaMaterialsLabel), static_cast<int>(x + 22),
             static_cast<int>(rowY), 12, kColorTextFaint);
    rowY += 34;
    if (node.materialCosts.empty()) {
        drawText(pick("None", kJaNoMaterialCost), static_cast<int>(x + 30), static_cast<int>(rowY), 13,
                 kColorTextMuted);
        rowY += 32;
    } else {
        for (const jf::LootStack& cost : node.materialCosts) {
            int have = base.storageCount(cost.id);
            bool enough = have >= cost.quantity;
            std::string line = materialNameFor(cost.id) + "  " + std::to_string(have) + " / " +
                               std::to_string(cost.quantity);
            drawText(line, static_cast<int>(x + 30), static_cast<int>(rowY), 13,
                     enough ? Color{140, 210, 150, 255} : Color{215, 130, 130, 255});
            rowY += 32;
        }
    }
    if (!reason.empty()) {
        rowY += 8;
        DrawLineEx(Vector2{x + 22, rowY - 8}, Vector2{x + width - 22, rowY - 8}, 1.5f, kColorBorderSoft);
        drawText(reason, static_cast<int>(x + 22), static_cast<int>(rowY), 13, Color{215, 130, 130, 255});
    }
}

// Mirrors facilityNodeEligible()'s checks in order, and reports which one
// is currently the blocker - keeps the UI text data-driven rather than a
// second hardcoded copy of the unlock rules.
// `compact` picks the abbreviated stage name for the ~130px-wide node-row
// action zone; the hover tooltip calls this with compact=false so the full,
// untruncated requirement is always available on hover.
std::string facilityNodeBlockedReason(const jf::BaseState& base, const jf::FacilityNode& node, bool compact = true) {
    if (static_cast<int>(base.outpostStage) < static_cast<int>(node.requiredStage)) {
        return pick("Stage: ", kJaNeedsStage) +
               (compact ? outpostStageShortNameFor(node.requiredStage) : outpostStageNameFor(node.requiredStage));
    }
    for (const jf::DiscoveryId& discovery : node.requiredDiscoveries)
        if (!base.discoveryRegistry.count(discovery)) return pick("Need discovery", kJaNeedsDiscovery);
    for (const std::string& prereqId : node.prerequisiteNodeIds) {
        const jf::FacilityNode* prereq = jf::findFacilityNode(prereqId);
        bool satisfied = prereq && prereq->occupiesFacilitySlot ? base.builtNodeIds.count(prereqId) > 0
                                                                 : base.unlockedNodeIds.count(prereqId) > 0;
        if (!satisfied) return pick("Not built", kJaNeedsFacilityBuilt);
    }
    for (const jf::LootStack& cost : node.materialCosts) {
        if (base.storageCount(cost.id) < cost.quantity)
            return pick("Need: " + cost.id, kJaNeedsMaterial + materialNameFor(cost.id));
    }
    if (node.occupiesFacilitySlot &&
        static_cast<int>(base.builtNodeIds.size()) >= jf::facilitySlotCapacity(base.outpostStage)) {
        return pick("No open slot", kJaNoOpenSlot);
    }
    return "";
}

void drawFacilityNodeRow(jf::GameApp& app, const jf::FacilityNode& node, float x, float y, float width,
                         Vector2 mouse, bool clicked) {
    const jf::BaseState& base = app.baseState();
    bool unlocked = base.unlockedNodeIds.count(node.id) > 0;
    bool built = base.builtNodeIds.count(node.id) > 0;

    constexpr float kActionZoneWidth = 132;
    constexpr float kNameLeftPad = 14;
    Rectangle actionRect{x + width - kActionZoneWidth, y, kActionZoneWidth, 24};
    // Leaves a visible gap before the action zone so long names/reasons can
    // never visually run into the button/label on the right.
    const int nameMaxWidth = static_cast<int>(actionRect.x - (x + kNameLeftPad) - 10);

    DrawCircle(static_cast<int>(x + 5), static_cast<int>(y + 12), 3.0f,
               built ? Color{95, 205, 140, 255} : unlocked ? kColorAccentGold : kColorTextFaint);
    drawText(clipTextToWidth(pick(node.nameEn, node.nameJa), 14, nameMaxWidth), static_cast<int>(x + kNameLeftPad),
             static_cast<int>(y) + 5, 14, unlocked ? kColorTextPrimary : kColorTextMuted);

    if (unlocked && node.occupiesFacilitySlot) {
        if (built) {
            if (button(actionRect, "Dismantle", kJaDismantle, mouse, clicked)) app.dismantleFacilityNode(node.id);
        } else if (static_cast<int>(base.builtNodeIds.size()) < jf::facilitySlotCapacity(base.outpostStage)) {
            if (button(actionRect, "Rebuild", kJaRebuild, mouse, clicked)) app.rebuildFacilityNode(node.id);
        } else {
            disabledButton(actionRect, pick("Dismantled", kJaDismantled));
        }
    } else if (unlocked) {
        drawText(pick("Unlocked", kJaUnlocked), static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13,
                 kColorTextFaint);
    } else if (jf::facilityNodeEligible(base, node)) {
        std::string label = node.occupiesFacilitySlot ? pick("Build", kJaBuild) : pick("Unlock", kJaUnlock);
        if (button(actionRect, label, label, mouse, clicked)) app.unlockFacilityNode(node.id);
    } else {
        std::string reason = clipTextToWidth(facilityNodeBlockedReason(base, node), 13,
                                             static_cast<int>(actionRect.width - 4));
        drawText(reason, static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13,
                 Color{205, 135, 135, 255});
    }
}

// English half of a Forge weapon's name, used both as the display fallback
// and as the lookup key fed into weaponNameFor() for the Japanese half.
std::string weaponEnglishName(const std::string& weaponId) {
    if (weaponId == "iron_spear") return "Iron Spear";
    if (weaponId == "long_spear") return "Long Spear";
    if (weaponId == "heavy_spear") return "Heavy Spear";
    if (weaponId == "guard_spear") return "Guard Spear";
    return weaponId;
}

// Only Spearman currently has Forge branch weapons/traits to manage
// (docs/base_development.md's Iron Spear branches + Hide-Wrapped Grip).
void drawForgeEquipmentPanel(jf::GameApp& app, const jf::UnitTemplate& unit, float x, float y, float width,
                             Vector2 mouse, bool clicked) {
    drawSectionHeading(pick("EQUIPMENT", "装備変更"), static_cast<int>(x),
                       static_cast<int>(y), 18);
    const jf::BaseState& base = app.baseState();
    auto overrideIt = app.weaponOverrides().find(unit.id);
    std::string current = overrideIt != app.weaponOverrides().end() ? overrideIt->second : "iron_spear";
    drawText(pick("Current: ", kJaCurrentWeapon) + weaponNameFor(current, weaponEnglishName(current)),
             static_cast<int>(x), static_cast<int>(y) + 30, 14, kColorTextMuted);

    struct Candidate { const char* id; const char* nodeId; };
    static const Candidate kCandidates[] = {
        {"iron_spear", nullptr},
        {"long_spear", "craft_long_spear"},
        {"heavy_spear", "craft_heavy_spear"},
        {"guard_spear", "craft_guard_spear"},
    };
    float by = y + 58;
    const float candidateWidth = (width - 12.0f) / 2.0f;
    for (int index = 0; index < 4; ++index) {
        const Candidate& candidate = kCandidates[index];
        bool available = candidate.nodeId == nullptr || base.unlockedNodeIds.count(candidate.nodeId) > 0;
        Rectangle rect{x + (index % 2) * (candidateWidth + 12.0f), by + (index / 2) * 46.0f,
                       candidateWidth, 34};
        std::string labelEn = weaponEnglishName(candidate.id);
        std::string labelJa = weaponNameFor(candidate.id, weaponEnglishName(candidate.id));
        if (available) {
            if (button(rect, labelEn, labelJa, mouse, clicked))
                app.equipWeaponForUnit(unit.id, candidate.id);
        } else {
            disabledButton(rect, pick(labelEn, labelJa));
        }
    }

    by += 100;
    bool traitUnlocked = base.unlockedNodeIds.count("trait_hide_wrapped_grip") > 0;
    bool traitEquipped = app.equippedTraits().count(unit.id) > 0;
    Rectangle traitRect{x, by, 280, 34};
    if (traitUnlocked) {
        if (button(traitRect, traitEquipped ? "Unequip Hide-Wrapped Grip" : "Equip Hide-Wrapped Grip",
                  traitEquipped ? kJaUnequipTrait : kJaEquipTrait, mouse, clicked)) {
            app.equipTuningTraitForUnit(unit.id,
                                        traitEquipped ? jf::TuningTraitId::None : jf::TuningTraitId::HideWrappedGrip);
        }
    } else {
        disabledButton(traitRect, pick("Hide-Wrapped Grip (locked)", kJaTraitLocked));
    }
}

void drawUnitScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    auto unit = std::find_if(app.roster().begin(), app.roster().end(), [&](const jf::UnitTemplate& candidate) {
        return gViewedUnitId && candidate.id == *gViewedUnitId;
    });
    if (unit == app.roster().end()) {
        gViewedUnitId.reset();
        return;
    }

    ClearBackground(Color{18, 21, 30, 255});
    drawText(pick("UNIT", "ユニット"), 38, 24, 28, kColorTextPrimary);
    Rectangle backRect{static_cast<float>(kScreenWidth) - 258.0f, 4.0f, 150.0f, 32.0f};
    if (button(backRect, "Party List", "編成一覧へ", mouse, clicked)) {
        gViewedUnitId.reset();
        return;
    }

    Rectangle identity{42, 104, 470, 500};
    drawCard(identity, kColorCard, kColorBorderSoft, 0.04f);
    drawText(unitDisplayNameFor(unit->name), 72, 132, 28, kColorAccentGold);
    drawText(classNameFor(unit->classId), 72, 184, 20, kColorTextPrimary);
    const std::string role = wrapTextToWidth(classRoleFor(unit->classId), 14, 400);
    drawText(role, 72, 230, 14, kColorTextMuted);
    const jf::Stats& stats = app.gameData().classDefinition(unit->classId).baseStats;
    drawSectionHeading(pick("STATS", "能力"), 72, 330, 18);
    drawText("HP " + std::to_string(stats.maxHp) + "    STR " + std::to_string(stats.strength) +
                 "    MAG " + std::to_string(stats.magic), 72, 374, 16, kColorTextPrimary);
    drawText("DEF " + std::to_string(stats.defense) + "    RES " + std::to_string(stats.resistance) +
                 "    MOV " + std::to_string(stats.move), 72, 418, 16, kColorTextPrimary);

    Rectangle equipment{548, 104, 690, 500};
    drawCard(equipment, kColorCard, kColorBorderSoft, 0.04f);
    if (unit->classId == jf::UnitClass::Spearman) {
        drawForgeEquipmentPanel(app, *unit, 580, 136, 626, mouse, clicked);
        if (!app.baseState().builtNodeIds.count("simple_forge"))
            drawText(pick("Build and activate the Simple Forge to change equipment.",
                          "装備変更には簡易鍛冶台の建設と稼働が必要です。"),
                     580, 390, 14, Color{205, 135, 135, 255});
    } else {
        drawSectionHeading(pick("EQUIPMENT", "装備"), 580, 136, 18);
        const std::string weaponId = app.gameData().classDefinition(unit->classId).weaponId;
        drawText(pick("Current weapon: ", "現在の武器: ") + weaponNameFor(weaponId, weaponEnglishName(weaponId)),
                 580, 188, 16, kColorTextPrimary);
        drawText(pick("No alternate equipment is available for this unit yet.",
                      "このユニットには、まだ変更可能な装備がありません。"),
                 580, 246, 14, kColorTextMuted);
    }
}

void drawFacilitiesScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    const jf::BaseState& base = app.baseState();
    Rectangle backRect{static_cast<float>(kScreenWidth) - 258.0f, 4.0f, 150.0f, 32.0f};

    if (!gVisitedFacility) {
        drawText(pick("FACILITIES", kJaFacilities), 38, 24, 28, kColorTextPrimary);
        std::string slots = pick("Facility Slots: ", kJaFacilitySlots + ": ") +
                            std::to_string(base.builtNodeIds.size()) + " / " +
                            std::to_string(jf::facilitySlotCapacity(base.outpostStage));
        drawText(slots, 38, 60, 16, kColorTextMuted);
        if (button(backRect, "Back", kJaBack, mouse, clicked)) {
            gVisitedFacility.reset();
            gForgeCraftClass.reset();
            gShowFacilities = false;
        }

        const jf::FacilityId facilities[] = {
            jf::FacilityId::CommandPost, jf::FacilityId::TrainingGround, jf::FacilityId::Forge,
            jf::FacilityId::Infirmary, jf::FacilityId::Workshop, jf::FacilityId::Barracks,
        };
        bool hasHoveredFacility = false;
        jf::FacilityId hoveredFacility = jf::FacilityId::CommandPost;
        for (int index = 0; index < 6; ++index) {
            const jf::FacilityId facility = facilities[index];
            const int col = index % 2;
            const int row = index / 2;
            Rectangle card{42.0f + col * 610.0f, 112.0f + row * 166.0f, 574.0f, 138.0f};
            drawCard(card, kColorCard, kColorBorderSoft, 0.04f);
            drawText(facilityIdNameFor(facility), static_cast<int>(card.x + 22), static_cast<int>(card.y + 18),
                     21, kColorTextPrimary);
            const bool active = facilityIsActive(base, facility);
            drawText("Lv " + std::to_string(facilityLevel(base, facility)), static_cast<int>(card.x + 22),
                     static_cast<int>(card.y + 59), 14, kColorAccentGold);
            drawText(active ? pick("ACTIVE", "稼働中") : pick("INACTIVE", "未稼働"),
                     static_cast<int>(card.x + 112), static_cast<int>(card.y + 59), 14,
                     active ? Color{105, 205, 145, 255} : Color{180, 125, 125, 255});
            Rectangle visitRect{card.x + card.width - 174.0f, card.y + 82.0f, 150.0f, 40.0f};
            if (button(visitRect, "Visit", "訪れる", mouse, clicked)) {
                gVisitedFacility = facility;
                gForgeCraftClass.reset();
            }
            if (CheckCollisionPointRec(mouse, card) && !CheckCollisionPointRec(mouse, visitRect)) {
                hasHoveredFacility = true;
                hoveredFacility = facility;
            }
        }
        if (hasHoveredFacility) drawFacilityTooltip(hoveredFacility, base, mouse);
        return;
    }

    const jf::FacilityId facility = *gVisitedFacility;
    const bool forgeCraftPage = facility == jf::FacilityId::Forge && gForgeCraftClass.has_value();
    const std::string pageTitle = forgeCraftPage
                                      ? pick("CRAFT: ", "制作: ") + classNameFor(*gForgeCraftClass)
                                      : facilityIdNameFor(facility);
    drawText(pageTitle, 38, 24, 28, kColorTextPrimary);
    drawText("Lv " + std::to_string(facilityLevel(base, facility)), 38, 64, 16, kColorAccentGold);
    drawText(facilityRoleFor(facility), 138, 58, 14, kColorTextMuted);
    if (forgeCraftPage) {
        if (button(backRect, "Forge", "鍛冶場へ", mouse, clicked)) gForgeCraftClass.reset();
    } else if (button(backRect, "Facility List", "施設一覧へ", mouse, clicked)) {
        gVisitedFacility.reset();
    }

    drawSectionHeading(forgeCraftPage ? pick("RECIPES", "制作レシピ") : pick("UPGRADES", "強化・解放"),
                       42, 132, 18);
    const jf::FacilityNode* hoveredNode = nullptr;
    float nodeY = 174.0f;
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.facility != facility) continue;
        const bool isWeaponRecipe = node.id.rfind("craft_", 0) == 0;
        if (facility == jf::FacilityId::Forge) {
            if (forgeCraftPage && (!isWeaponRecipe || *gForgeCraftClass != jf::UnitClass::Spearman)) continue;
            if (!forgeCraftPage && isWeaponRecipe) continue;
        }
        Rectangle rowPanel{36.0f, nodeY - 5.0f, 696.0f, 38.0f};
        DrawRectangleRec(rowPanel, Color{25, 30, 42, 255});
        drawFacilityNodeRow(app, node, 48.0f, nodeY, 672.0f, mouse, clicked);
        if (CheckCollisionPointRec(mouse, rowPanel)) hoveredNode = &node;
        nodeY += 44.0f;
    }

    Rectangle infoPanel{770.0f, 128.0f, 470.0f, 510.0f};
    drawCard(infoPanel, kColorCard, kColorBorderSoft, 0.04f);
    drawSectionHeading(forgeCraftPage ? pick("CLASS RECIPES", "兵種別レシピ")
                                      : pick("FACILITY DETAILS", "施設情報"),
                       794, 150, 18);
    if (facility == jf::FacilityId::Forge && !forgeCraftPage) {
        drawText(pick("Choose a class to open its weapon recipes.",
                      "制作する兵種を選択してください。"),
                 794, 192, 14, kColorTextPrimary);
        const jf::UnitClass craftClasses[] = {
            jf::UnitClass::MarchCaptain, jf::UnitClass::VeteranGuard, jf::UnitClass::WatchArcher,
            jf::UnitClass::FrontierScout, jf::UnitClass::Spearman, jf::UnitClass::DawnChirurgeon,
        };
        for (int index = 0; index < 6; ++index) {
            Rectangle craftRect{794.0f, 232.0f + index * 58.0f, 410.0f, 46.0f};
            const std::string en = "Craft: " + classNameFor(craftClasses[index]);
            const std::string ja = "制作: " + classNameFor(craftClasses[index]);
            if (craftClasses[index] == jf::UnitClass::Spearman) {
                if (button(craftRect, en, ja, mouse, clicked)) gForgeCraftClass = craftClasses[index];
            } else {
                disabledButton(craftRect,
                               pick("Craft: " + classNameFor(craftClasses[index]) + " (planned)",
                                    "制作: " + classNameFor(craftClasses[index]) + " (未実装)"));
            }
        }
    } else {
        drawText(forgeCraftPage ? pick("Crafted weapons become available on the unit page.",
                                       "制作した武器はユニットページで装備できます。")
                                : facilityRoleFor(facility),
                 794, 192, 14, kColorTextPrimary);
        drawText(pick("Select or hover over an upgrade to view its\neffect and required materials.",
                      "強化項目にカーソルを合わせると、\n効果と必要素材を確認できます。"),
                 794, 292, 14, kColorTextMuted);
    }
    if (hoveredNode) drawFacilityNodeTooltip(*hoveredNode, base, mouse);
}

// Small always-on-top corner button + modal for switching the display
// language. Purely a rendering/UI concern (see the Language enum above),
// so it lives entirely in this file and never touches GameApp/BattleState.
// Draw this last on every screen so it sits above any other overlay.
void drawSettingsOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    Rectangle cornerBtn{static_cast<float>(kScreenWidth) - 100.0f, 4.0f, 92.0f, 32.0f};
    if (button(cornerBtn, "Settings", kJaSettings, mouse, gSettingsOpen ? false : clicked)) {
        gSettingsOpen = !gSettingsOpen;
    }

    if (!gSettingsOpen) return;

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 150});

    Rectangle panel{static_cast<float>(kScreenWidth) / 2.0f - 190.0f, static_cast<float>(kScreenHeight) / 2.0f - 320.0f,
                    380.0f, 640.0f};
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

    drawText(pick("Window", kJaWindow), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 158), 15,
             kColorTextMuted);
    bool maximized = IsWindowState(FLAG_WINDOW_MAXIMIZED);
    Rectangle windowBtn{panel.x + 26, panel.y + 188, 328, 46};
    if (button(windowBtn, maximized ? "Restore Window" : "Maximize Window",
              maximized ? kJaRestoreWindow : kJaMaximizeWindow, mouse, clicked)) {
        if (maximized) RestoreWindow();
        else MaximizeWindow();
    }
    drawText(pick("Shortcut: F11", kJaMaximizeShortcut), static_cast<int>(panel.x + 26),
             static_cast<int>(panel.y + 238), 12, kColorTextFaint);

    drawText(pick("Expedition", kJaExpeditionSection), static_cast<int>(panel.x + 26),
             static_cast<int>(panel.y + 266), 15, kColorTextMuted);
    Rectangle retireBtn{panel.x + 26, panel.y + 296, 328, 46};
    bool canRetire = app.screen() != jf::Screen::Base;
    if (canRetire) {
        if (button(retireBtn, "Retire Expedition", kJaRetireExpedition, mouse, clicked)) {
            if (app.retireExpedition()) gSettingsOpen = false;
        }
    } else {
        disabledButton(retireBtn, pick("Retire Expedition", kJaRetireExpedition));
    }
    drawText(pick("Forfeits this run's unsecured loot.", kJaRetireExpeditionNote),
             static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 346), 12, kColorTextFaint);

    drawText(pick("Save Data", kJaSaveDataSection), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 380),
              15, kColorTextMuted);

    Rectangle exportBtn{panel.x + 26, panel.y + 408, 150, 46};
    Rectangle importBtn{panel.x + 26 + 150 + 16, panel.y + 408, 150, 46};

    if (button(exportBtn, "Export", kJaExportSave, mouse, clicked)) {
        const std::string language = gLanguage == Language::Japanese ? "ja" : "en";
        std::string exportError;
        std::string exportedPath = jf::exportSaveData(app.createSaveData(language), &exportError);
        if (!exportedPath.empty()) setSaveStatus("Exported to " + exportedPath, kJaExportOk + exportedPath);
        else setSaveStatus("Export failed: " + exportError, kJaExportFailed + exportError);
    }

    const bool canImport = app.screen() == jf::Screen::Base;
    if (gPendingImport) {
        drawText(clipTextToWidth(gPendingImportFilename + " - " + pick("Replace current save?", kJaImportConfirm), 13,
                                  328),
                 static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 462), 13, kColorTextPrimary);
        Rectangle applyBtn{panel.x + 26, panel.y + 488, 150, 44};
        Rectangle cancelBtn{panel.x + 26 + 150 + 16, panel.y + 488, 150, 44};
        if (button(applyBtn, "Apply Import", kJaImportApply, mouse, clicked)) {
            std::string importError;
            if (gSaveStore && gSaveStore->importFrom(*gPendingImport, &importError) && app.applySaveData(*gPendingImport)) {
                gLanguage = gPendingImport->language == "ja" ? Language::Japanese : Language::English;
                gAutoSaveEnabled = true;
                setSaveStatus("Import applied", kJaImportApplied);
            } else {
                setSaveStatus("Import failed: " + importError, kJaImportFailed + importError);
            }
            gPendingImport.reset();
            gPendingImportFilename.clear();
        }
        if (button(cancelBtn, "Cancel", kJaImportCancel, mouse, clicked)) {
            gPendingImport.reset();
            gPendingImportFilename.clear();
        }
    } else if (canImport) {
        if (button(importBtn, "Import", kJaImportSave, mouse, clicked)) {
            auto candidates = jf::listImportCandidates();
            if (candidates.empty()) {
                setSaveStatus("No import file in imports/", kJaImportNoFile);
            } else {
                std::string loadError;
                if (auto data = jf::loadImportCandidate(candidates.front().path, &loadError)) {
                    gPendingImport = data;
                    gPendingImportFilename = candidates.front().filename;
                } else {
                    setSaveStatus("Import failed: " + loadError, kJaImportFailed + loadError);
                }
            }
        }
    } else {
        disabledButton(importBtn, pick("Import", kJaImportSave));
        drawText(pick("Return to Base first.", kJaImportBaseOnly), static_cast<int>(panel.x + 26),
                 static_cast<int>(panel.y + 462), 12, kColorTextFaint);
    }

    if (!gSaveStatusMessage.empty() && GetTime() < gSaveStatusExpiresAt) {
        drawText(clipTextToWidth(pick(gSaveStatusMessage, gSaveStatusMessageJa), 12, 328),
                 static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 544), 12, kColorAccentGold);
    }

    Rectangle closeBtn{panel.x + 26, panel.y + 580, 328, 40};
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
        case jf::BattleInputState::SelectItemTarget:
            app.selectNeutralBattleHealingTarget(pos);
            break;
        case jf::BattleInputState::SelectBoardTarget:
            app.selectBoardTarget(pos);
            break;
        case jf::BattleInputState::SelectSkillTarget:
            controller.selectSkillTarget(pos);
            break;
        default:
            break;
    }
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(kScreenWidth, kScreenHeight, "JOJIFrontier");
    SetWindowMinSize(960, 600);
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
            drawText(pick("Failed to load game definitions from data/.", kJaDataLoadFailed), 20,
                     20, 18, RED);
            EndDrawing();
        }
        CloseWindow();
        return 1;
    }

    jf::GameApp app(*gameData);
    gSaveStore.emplace(jf::defaultSavePath());
    std::string saveLoadError;
    if (auto save = gSaveStore->load(&saveLoadError)) {
        if (app.applySaveData(*save)) gLanguage = save->language == "ja" ? Language::Japanese : Language::English;
    }
    // A corrupt or unsupported save is never overwritten automatically. Its
    // file and backup remain available for a later recovery/import flow -
    // Settings > Save Data > Import can still replace it explicitly.
    gAutoSaveEnabled = saveLoadError.empty();
    std::uint64_t savedRevision = app.persistentRevision();
    std::uint64_t savedExpeditionRevision = app.expeditionRevision();
    Language savedLanguage = gLanguage;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Vector2 mouse = logicalMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        // While the Settings modal is open, it owns all clicks; nothing
        // underneath should react even though it's still drawn (dimmed).
        bool sceneClicked = clicked && !gSettingsOpen;

        // F11 (and macOS's usual Cmd+Ctrl+F fullscreen shortcut) toggle a
        // real window maximize/restore, in case the native title-bar zoom
        // button isn't reachable (e.g. some window managers/sandboxes).
        bool wantsMaximizeToggle =
            IsKeyPressed(KEY_F11) ||
            (IsKeyPressed(KEY_F) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) &&
             (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)));
        if (wantsMaximizeToggle) {
            if (IsWindowState(FLAG_WINDOW_MAXIMIZED)) RestoreWindow();
            else MaximizeWindow();
        }

        app.update(dt);

        if (app.screen() == jf::Screen::Battle) {
            jf::BattleController& controller = app.battle();

            if (sceneClicked) {
                jf::GridPos pos;
                if (tileFromScreen(mouse, pos)) {
                    handleGridClick(app, pos);
                }
            }

            beginLogicalFrame();
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
            drawSettingsOverlay(app, mouse, clicked);
            endLogicalFrame();
        } else if (app.screen() == jf::Screen::Camp) {
            beginLogicalFrame();
            drawCampScreen(app, mouse, sceneClicked);
            drawSettingsOverlay(app, mouse, clicked);
            endLogicalFrame();
        } else if (app.screen() == jf::Screen::Exploration) {
            beginLogicalFrame();
            drawExplorationScreen(app, mouse, sceneClicked);
            drawSettingsOverlay(app, mouse, clicked);
            endLogicalFrame();
        } else if (app.screen() == jf::Screen::PreBattleDeployment) {
            beginLogicalFrame();
            drawPreBattleDeploymentScreen(app, mouse, sceneClicked);
            drawSettingsOverlay(app, mouse, clicked);
            endLogicalFrame();
        } else {
            beginLogicalFrame();
            if (gShowFacilities) drawFacilitiesScreen(app, mouse, sceneClicked);
            else if (gViewedUnitId) drawUnitScreen(app, mouse, sceneClicked);
            else drawBaseScreen(app, mouse, sceneClicked);
            drawSettingsOverlay(app, mouse, clicked);
            endLogicalFrame();
        }

        if (gAutoSaveEnabled &&
            (savedRevision != app.persistentRevision() || savedLanguage != gLanguage ||
             savedExpeditionRevision != app.expeditionRevision())) {
            const std::string language = gLanguage == Language::Japanese ? "ja" : "en";
            if (gSaveStore->save(app.createSaveData(language))) {
                savedRevision = app.persistentRevision();
                savedLanguage = gLanguage;
                savedExpeditionRevision = app.expeditionRevision();
            }
        }
    }

    if (gFontReady) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
