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
#include "jf/core/Locale.hpp"
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
// Common buttons, phase headers, and generic error/validation text have
// moved to data/locales/{ja,en}.json + jf::tr() (docs/localization.md M3-B).
// kJaJapaneseNative is the one deliberate exception: a language picker's
// native-script label for switching TO that language ("日本語") is not
// itself translated (same reasoning as the "English" literal beside it),
// so it stays a plain constant rather than a Locale Key.
const std::string kJaJapaneseNative = "日本語";

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

// docs/inventory_overflow.md「倉庫整理画面」: independent of GameApp::Screen
// (same convention as gSettingsOpen/gCampItemMenuOpen above) so it can
// overlay either Camp (opened automatically when returnToBase() is blocked
// by the 200-Stack pending ceiling) or Base (opened manually for routine
// cleanup).
bool gWarehouseCleanupOpen = false;
enum class WarehouseDiscardKind { Material, Item, Overflow };
struct WarehouseDiscardTarget {
    WarehouseDiscardKind kind;
    jf::LootId materialId;
    jf::ItemType itemType = jf::ItemType::FirstAidKit;
    std::size_t overflowIndex = 0;
    int quantity = 0;
    std::string displayName;
};
std::optional<WarehouseDiscardTarget> gWarehouseDiscardConfirm;

// docs/save_system.md「破損復旧画面」: set at startup (see main()) when the
// primary save could not be read AND no automatic ".bak" fallback inside
// SaveStore::load() succeeded either - i.e. there is truly nothing to fall
// back to silently. Drawn instead of (not on top of) the normal screen
// dispatch, since there's no meaningful "underneath" game state to show yet.
bool gSaveRecoveryOpen = false;
bool gSaveRecoveryStartNewConfirm = false;
std::string gSaveRecoveryMessage;

// docs/save_system.md「保存状態HUD」: minimal Idle/Saving/Saved/Failed state
// machine drawn in the screen's bottom-right corner. Desktop saves are
// synchronous file I/O, so `Saving` is only ever visible for a single frame;
// the state machine exists chiefly so `Failed`+manual retry actually works,
// and to match the spec's transition shape for when a slower (Web/IDBFS)
// backend is added.
enum class SaveHudState { Idle, Saving, Saved, Failed };
SaveHudState gSaveHudState = SaveHudState::Idle;
double gSaveHudSavedUntil = 0.0;
std::string gSaveHudFailReason;
int gSaveHudRetryCount = 0;
double gSaveHudNextRetryAt = 0.0;
constexpr int kSaveHudMaxAutoRetries = 3;
constexpr double kSaveHudRetryDelays[kSaveHudMaxAutoRetries] = {0.5, 1.0, 2.0};

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

// Locale Key lookup (docs/localization.md M3-B): thin wrapper around
// jf::tr() using this file's own `gLanguage` state, so Locale.cpp never
// needs to know about main.cpp's language-switching global.
std::string tr(const std::string& key) { return jf::tr(key, gLanguage == Language::Japanese); }
std::string tr(const std::string& key, const std::unordered_map<std::string, std::string>& args) {
    return jf::tr(key, gLanguage == Language::Japanese, args);
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

std::string classNameFor(const jf::GameData& data, jf::UnitClass unitClass);
std::string classRoleFor(const jf::GameData& data, jf::UnitClass unitClass);
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
    // Locale Keys' Japanese Values (data/locales/ja.json) are covered here
    // automatically instead of by name, so migrating more strings to Keys
    // (docs/localization.md M3-B stages 3+) never requires a manual charset
    // edit like the kJa* constants below still do.
    charsetSource += jf::allJapaneseGlyphText();
    charsetSource += kJaJapaneseNative +
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
    // classNameFor()/classRoleFor()'s Japanese text is already covered by
    // allJapaneseGlyphText() above (both are plain tr() wrappers over
    // Locale Keys already in the loaded table) - no manual collection
    // needed here, unlike the still-hardcoded helpers below.
    const Language previousLanguage = gLanguage;
    gLanguage = Language::Japanese;
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

// docs/implementation_roadmap.md M1-E slice5: looks up ClassDefinition's
// data-driven nameKey (data/classes.json) instead of a UnitClass switch, so
// a new class only needs a classes.json row, never a new case here.
std::string classNameFor(const jf::GameData& data, jf::UnitClass unitClass) {
    auto it = data.classesById.find(unitClass);
    if (it == data.classesById.end()) return tr("class.unknown");
    return tr(it->second.nameKey);
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
        badges.push_back({tr("status.poison.glyph"), tr("status.poison.label"),
                          tr("status.poison.label") + " (" + tr("status.left") + " " +
                              std::to_string(unit.poisonRemainingProcs) + ")",
                          Color{150, 95, 205, 255}});
    }
    if (unit.burnRemainingProcs > 0) {
        badges.push_back({tr("status.burn.glyph"), tr("status.burn.label"),
                          tr("status.burn.label") + " (" + tr("status.left") + " " +
                              std::to_string(unit.burnRemainingProcs) + ")",
                          Color{235, 120, 60, 255}});
    }
    if (unit.moveDownActive) {
        badges.push_back({tr("status.move_down.glyph"), tr("status.move_down.label"),
                          tr("status.move_down.label") + " (" + tr("status.until_phase_end") + ")",
                          Color{90, 150, 210, 255}});
    }
    if (unit.defenseDownActive) {
        badges.push_back({tr("status.def_down.glyph"), tr("status.def_down.label"),
                          tr("status.def_down.label") + " (" + tr("status.until_phase_end") + ")",
                          Color{210, 90, 90, 255}});
    }
    if (unit.staggerActive) {
        badges.push_back({tr("status.stagger.glyph"), tr("status.stagger.label"),
                          tr("status.stagger.label") + " (" + tr("status.next_action") + ")",
                          Color{205, 195, 90, 255}});
    }
    return badges;
}

std::string terrainNameFor(jf::TerrainType terrain) {
    switch (terrain) {
        case jf::TerrainType::Floor: return tr("terrain.floor");
        case jf::TerrainType::Ash: return tr("terrain.ash");
        case jf::TerrainType::Rubble: return tr("terrain.rubble");
        case jf::TerrainType::Barrier: return tr("terrain.barrier");
        case jf::TerrainType::WatchPost: return tr("terrain.watch_post");
        case jf::TerrainType::Brush: return tr("terrain.brush");
        case jf::TerrainType::HerbPatch: return tr("terrain.herb_patch");
        case jf::TerrainType::Shallows: return tr("terrain.shallows");
    }
    return jf::toString(terrain);
}

std::string battleObjectNameForKind(jf::BattleObjectKind kind) {
    switch (kind) {
        case jf::BattleObjectKind::Barrier: return tr("battle_object.barrier");
        case jf::BattleObjectKind::Marker: return tr("battle_object.marker");
        case jf::BattleObjectKind::SpawnPoint: return tr("battle_object.spawn_point");
        case jf::BattleObjectKind::ExitPoint: return tr("battle_object.exit_point");
        case jf::BattleObjectKind::Device: return tr("battle_object.device");
        case jf::BattleObjectKind::Container: return tr("battle_object.container");
    }
    return "";
}

std::string battleObjectNameFor(const jf::BattleObjectDefinition& definition) {
    std::string name = battleObjectNameForKind(definition.kind);
    return name.empty() ? definition.definitionId : name;
}

// Unit/enemy names come from data/units.json (or a stage-specific rename
// like the "Former Captain" boss) as plain English strings - the data
// layer has no concept of display language. Translate by looking the
// current name up in a small dictionary here; anything not in it (a name
// we haven't localized yet) just falls back to the English original.
std::string unitDisplayNameFor(const std::string& englishName) {
    static const std::unordered_map<std::string, std::string> table = {
        {"Leon", "character.leon"},
        {"Gareth", "character.gareth"},
        {"Erin", "character.erin"},
        {"Mira", "character.mira"},
        {"Nessa", "character.nessa"},
        {"Rowan", "character.rowan"},
        {"Frontier Scout", "class.frontier_scout"},
        {"March Spearman", "character.march_spearman"},
        {"Raider", "character.raider"},
        {"Watch Archer", "class.watch_archer"},
        {"Deserter Spearman", "character.deserter_spearman"},
        {"Former Captain", "character.former_captain"},
        {"Wolf", "class.wolf"},
        {"Ashenhorn Boar", "class.ashenhorn_boar"},
    };
    auto it = table.find(englishName);
    return it != table.end() ? tr(it->second) : englishName;
}

std::string materialNameFor(const std::string& id) {
    static const std::unordered_set<std::string> known = {
        "wood", "hide", "herb", "gate_tools", "ash_road_map", "field_medicine", "watch_ledger", "signal_lens",
        "captains_seal", jf::kAshveilFangMaterial, jf::kAshenhornFangMaterial, "quality_herb", "ashenhorn_fragment",
    };
    return known.count(id) ? tr("material." + id) : id;
}

std::string itemFullNameFor(jf::ItemType type) {
    switch (type) {
        case jf::ItemType::FirstAidKit: return tr("item.first_aid_kit.name");
        case jf::ItemType::FieldTreatmentKit: return tr("item.field_treatment_kit.name");
        case jf::ItemType::RescuePack: return tr("item.rescue_pack.name");
        case jf::ItemType::CampRations: return tr("item.camp_rations.name");
        case jf::ItemType::ProtectiveBoard: return tr("item.protective_board.name");
        case jf::ItemType::ReturnFlare: return tr("item.return_flare.name");
    }
    return "";
}

// Weapon names come from data/weapons.json as plain English strings, same as
// unit/enemy names - look up the Locale Key by the stable weapon id here.
std::string weaponNameFor(const std::string& weaponId, const std::string& englishName) {
    static const std::unordered_set<std::string> known = {
        "iron_sword", "iron_lance", "iron_axe",  "watch_bow",   "scout_blade", "dawn_staff",
        "iron_spear", "long_spear", "heavy_spear", "guard_spear", "wolf_bite",   "boar_tusks",
    };
    return known.count(weaponId) ? tr("weapon." + weaponId) : englishName;
}

// One-line role summary for a unit class, matching its actual mechanical
// trait (see jf::hasBrace/hasZoneOfControl/etc. in UnitClass.cpp) - shown in
// the roster hover tooltip. Same data-driven lookup as classNameFor().
std::string classRoleFor(const jf::GameData& data, jf::UnitClass unitClass) {
    auto it = data.classesById.find(unitClass);
    if (it == data.classesById.end()) return "";
    return tr(it->second.roleKey);
}

// Locale counterpart to jf::kItemCatalog's (English-only) descriptions.
std::string itemDescriptionFor(jf::ItemType type) {
    switch (type) {
        case jf::ItemType::FirstAidKit: return tr("item.first_aid_kit.description");
        case jf::ItemType::FieldTreatmentKit: return tr("item.field_treatment_kit.description");
        case jf::ItemType::RescuePack: return tr("item.rescue_pack.description");
        case jf::ItemType::CampRations: return tr("item.camp_rations.description");
        case jf::ItemType::ProtectiveBoard: return tr("item.protective_board.description");
        case jf::ItemType::ReturnFlare: return tr("item.return_flare.description");
    }
    return "";
}

std::string outpostStageNameFor(jf::OutpostStage stage) {
    switch (stage) {
        case jf::OutpostStage::Encampment: return tr("outpost_stage.encampment");
        case jf::OutpostStage::PioneerOutpost: return tr("outpost_stage.pioneer_outpost");
        case jf::OutpostStage::FrontierSettlement: return tr("outpost_stage.frontier_settlement");
        case jf::OutpostStage::PioneerCity: return tr("outpost_stage.pioneer_city");
    }
    return "";
}

// Short form used only in the Facilities screen's cramped node-row action
// zone (~130px) - the full name is always available in the node's hover
// tooltip, so nothing is actually lost by abbreviating here.
std::string outpostStageShortNameFor(jf::OutpostStage stage) {
    switch (stage) {
        case jf::OutpostStage::Encampment: return tr("outpost_stage.encampment.short");
        case jf::OutpostStage::PioneerOutpost: return tr("outpost_stage.pioneer_outpost.short");
        case jf::OutpostStage::FrontierSettlement: return tr("outpost_stage.frontier_settlement.short");
        case jf::OutpostStage::PioneerCity: return tr("outpost_stage.pioneer_city.short");
    }
    return "";
}

// Discovery IDs come straight from BaseState's data-layer constants; this is
// purely the display-language lookup, same pattern as itemFullNameFor.
std::string discoveryNameFor(const jf::DiscoveryId& id) {
    if (id == jf::kCinderwatchReconDiscovery) return tr("discovery.cinderwatch_recon");
    if (id == jf::kFieldMedicineDiscovery) return tr("discovery.field_medicine");
    if (id == jf::kReturnSignalDiscovery) return tr("discovery.return_signal");
    if (id == jf::kHerbThicketDiscovery) return tr("discovery.herb_thicket");
    if (id == jf::kAshboughForestSurveyCompleteDiscovery) return tr("discovery.ashbough_forest_survey_complete");
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

// Locale Key overload (docs/localization.md M3-B): for labels already
// resolved via tr(), where there's no separate en/ja pair to pick between.
bool button(Rectangle rect, const std::string& label, Vector2 mouse, bool mousePressed) {
    return button(rect, label, label, mouse, mousePressed);
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

// Per-wave last-seen state, so a Scheduled -> Announced transition fires the
// banner exactly once (ReinforcementAnnouncedEvent itself isn't readable from
// here - BattleMissionState::consumedEventIds is a write-only dedup set with
// no stored payload, so this polls BattleState::reinforcementWaves() instead,
// the same technique lowHpWarnedUnits() above uses for attack events).
std::unordered_map<std::string, jf::ReinforcementState>& reinforcementUiStates() {
    static std::unordered_map<std::string, jf::ReinforcementState> states;
    return states;
}

std::string reinforcementAnnouncedMessageText(int spawnRound) {
    return tr("battle.reinforcement_announced", {{"round", std::to_string(spawnRound)}});
}

// Same technique as reinforcementUiStates() above, keyed by Boss Unit id -
// BossTelegraphChangedEvent has the same write-only consumedEventIds
// limitation, so this polls Unit::bossRuntime.telegraph.state instead.
std::unordered_map<std::string, jf::TelegraphState>& bossTelegraphUiStates() {
    static std::unordered_map<std::string, jf::TelegraphState> states;
    return states;
}

std::string bossChargeTelegraphedMessageText() {
    return tr("battle.boss_charge_telegraphed");
}

// Same technique as bossTelegraphUiStates() above, keyed by Boss Unit id -
// BossStageChangedEvent (fired the instant Unit::bossEnraged flips) has the
// same write-only consumedEventIds limitation, so this polls the field
// directly instead.
std::unordered_map<std::string, bool>& bossEnrageUiStates() {
    static std::unordered_map<std::string, bool> states;
    return states;
}

std::string bossEnragedMessageText() {
    return tr("battle.boss_enraged");
}

std::string hitMessageText(const std::string& attackerName, const std::string& targetName, int damage) {
    return tr("battle.hit_message",
             {{"attacker", attackerName}, {"target", targetName}, {"damage", std::to_string(damage)}});
}

std::string missMessageText(const std::string& attackerName, const std::string& targetName) {
    return tr("battle.miss_message", {{"attacker", attackerName}, {"target", targetName}});
}

std::string lowHpMessageText(const std::string& targetName) {
    return tr("battle.low_hp_message", {{"target", targetName}});
}

std::string fallenMessageText(const std::string& targetName) {
    return tr("battle.fallen_message", {{"target", targetName}});
}

std::string objectHitMessageText(const std::string& attackerName, const std::string& objectName, int damage) {
    return tr("battle.object_hit_message",
             {{"attacker", attackerName}, {"object", objectName}, {"damage", std::to_string(damage)}});
}

std::string objectDestroyedMessageText(const std::string& objectName) {
    return tr("battle.object_destroyed_message", {{"object", objectName}});
}

// Notices any battle-state transition that should surface as a one-shot
// banner/animation this frame (a resolved attack, a reinforcement wave's
// Scheduled->Announced, a Boss telegraph's None->Announced, a Boss crossing
// its enrage threshold) and returns the currently-pending Boss telegraph's
// danger-zone tiles for drawBoardTiles() to highlight. Split out of
// drawGrid() purely to separate "notice state changes" from "draw the
// board" - no behavior change.
std::vector<jf::GridPos> detectAndAnnounceBattleEvents(const jf::BattleController& controller) {
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

    // Detect a wave's Scheduled -> Announced transition and surface it as a
    // one-shot banner (see reinforcementUiStates() above for why polling,
    // not the event itself, is the read path).
    auto& waveStates = reinforcementUiStates();
    for (const jf::ReinforcementWave& wave : battle.reinforcementWaves()) {
        auto [it, inserted] = waveStates.try_emplace(wave.id, jf::ReinforcementState::Scheduled);
        if (it->second != jf::ReinforcementState::Announced && wave.state == jf::ReinforcementState::Announced) {
            pushBattleMessage(reinforcementAnnouncedMessageText(wave.spawnRound), Color{235, 190, 120, 255});
        }
        it->second = wave.state;
    }

    // Same idea for a Boss's telegraphed charge (docs/boss_common_rules.md's
    // "予告"): a None/Executed/Cancelled -> Announced transition fires the
    // banner once, and while pending() the telegraphed danger-zone tiles
    // (BossTelegraph::lockedTiles, computed at telegraph time in
    // EnemyAI.cpp's computeBoarChargeTiles()) get highlighted below.
    auto& telegraphStates = bossTelegraphUiStates();
    std::vector<jf::GridPos> telegraphDangerTiles;
    for (const jf::Unit& unit : battle.units()) {
        const jf::BossTelegraph& telegraph = unit.bossRuntime.telegraph;
        auto [it, inserted] = telegraphStates.try_emplace(unit.id, jf::TelegraphState::None);
        if (it->second != jf::TelegraphState::Announced && telegraph.state == jf::TelegraphState::Announced) {
            pushBattleMessage(bossChargeTelegraphedMessageText(), Color{235, 150, 60, 255});
        }
        it->second = telegraph.state;
        if (telegraph.pending())
            telegraphDangerTiles.insert(telegraphDangerTiles.end(), telegraph.lockedTiles.begin(),
                                       telegraph.lockedTiles.end());
    }

    // Same idea for a Boss crossing its enrage HP threshold
    // (docs/regions/ashbough_forest.md「HPが半分以下になると激昂し」): a
    // false -> true transition on Unit::bossEnraged fires a one-shot banner.
    // Unlike the telegraph above, this is a permanent stat change with no
    // tile of its own, so no highlight is drawn for it.
    auto& enrageStates = bossEnrageUiStates();
    for (const jf::Unit& unit : battle.units()) {
        auto [it, inserted] = enrageStates.try_emplace(unit.id, false);
        if (!it->second && unit.bossEnraged) {
            pushBattleMessage(bossEnragedMessageText(), Color{225, 90, 90, 255});
        }
        it->second = unit.bossEnraged;
    }

    return telegraphDangerTiles;
}

// The 3x8 board itself: terrain, Battle Objects, and every Tile-highlight
// overlay (movement/attack/heal/item/board/skill/interact ranges, the
// pending Boss charge danger zone). Split out of drawGrid(); no behavior
// change.
void drawBoardTiles(const jf::BattleController& controller, const std::vector<jf::GridPos>& telegraphDangerTiles) {
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

            // Battle Objects are separate from Terrain. In particular, a
            // fallen log sits on a passable Floor tile but blocks movement;
            // it must remain visibly distinct so the board never presents a
            // flat-looking tile that rejects movement.
            if (const jf::BattleObjectState* object = battle.objectAt(pos);
                object && object->state != jf::BattleObjectStateKind::Destroyed) {
                const jf::BattleObjectDefinition* definition = battle.objectDefinition(object->definitionId);
                if (definition && definition->kind == jf::BattleObjectKind::Barrier) {
                    const float y = rect.y + rect.height * 0.42f;
                    DrawLineEx({rect.x + 16.0f, y + 4.0f}, {rect.x + rect.width - 16.0f, y - 4.0f},
                               13.0f, Color{82, 55, 38, 255});
                    DrawLineEx({rect.x + 19.0f, y + 1.0f}, {rect.x + rect.width - 19.0f, y - 7.0f},
                               4.0f, Color{151, 105, 63, 255});
                    DrawCircle(static_cast<int>(rect.x + 16.0f), static_cast<int>(y + 4.0f), 7.0f,
                               Color{56, 37, 28, 255});
                    DrawCircle(static_cast<int>(rect.x + rect.width - 16.0f), static_cast<int>(y - 4.0f), 7.0f,
                               Color{56, 37, 28, 255});
                } else if (definition && definition->kind == jf::BattleObjectKind::Marker) {
                    DrawCircleLines(static_cast<int>(rect.x + rect.width * 0.5f),
                                    static_cast<int>(rect.y + rect.height * 0.42f), 13.0f,
                                    Color{244, 199, 92, 230});
                } else if (definition && definition->kind == jf::BattleObjectKind::SpawnPoint) {
                    DrawTriangle({rect.x + rect.width * 0.5f, rect.y + 8.0f},
                                 {rect.x + rect.width * 0.35f, rect.y + rect.height - 14.0f},
                                 {rect.x + rect.width * 0.65f, rect.y + rect.height - 14.0f},
                                 Color{205, 82, 82, 190});
                }
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
            if (containsTile(controller.objectTargetableTiles(), pos) ||
                (controller.pendingObjectTarget() && controller.pendingObjectTarget()->position == pos))
                DrawRectangleRec(rect, Color{230, 28, 38, 185});
            if (containsTile(controller.healableTiles(), pos))
                DrawRectangleRec(rect, Color{55, 205, 115, 155});
            if (containsTile(controller.itemTargetTiles(), pos))
                DrawRectangleRec(rect, Color{70, 210, 145, 175});
            if (containsTile(controller.boardTargetTiles(), pos))
                DrawRectangleRec(rect, Color{220, 185, 70, 150});
            if (containsTile(controller.skillTargetTiles(), pos))
                DrawRectangleRec(rect, Color{90, 200, 235, 165});
            if (containsTile(controller.objectInteractableTiles(), pos))
                DrawRectangleRec(rect, Color{190, 150, 235, 165});
            if (containsTile(telegraphDangerTiles, pos))
                DrawRectangleRec(rect, Color{235, 150, 60, 140});
        }
    }
}

// Every living Unit's sprite: shadow, selection ring, body, HP bar, status
// badges, and name tag. Split out of drawGrid(); no behavior change.
void drawUnitSprites(const jf::BattleController& controller, float dt) {
    const jf::BattleState& battle = controller.battle();

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
}

void drawGrid(const jf::BattleController& controller, float dt) {
    std::vector<jf::GridPos> telegraphDangerTiles = detectAndAnnounceBattleEvents(controller);
    drawBoardTiles(controller, telegraphDangerTiles);
    drawUnitSprites(controller, dt);
    drawBattleMessages(dt);
}

void drawPhaseBanner(const jf::BattleController& controller) {
    bool isPlayerPhase = controller.battle().phase() == jf::Phase::PlayerPhase;
    std::string label = tr(isPlayerPhase ? "ui.phase.player" : "ui.phase.enemy");
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
    std::string label = tr(isPlayerPhase ? "ui.phase.player" : "ui.phase.enemy");
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

    drawText(tr("ui.combat.preview_title"), tx, ty, 20, kColorAccentGold);
    ty += 34;
    drawText(attackerName + "  ->  " + targetName, tx, ty, 22, kColorTextPrimary);
    ty += 30;
    drawText(weaponNameFor(preview.weaponId, preview.weaponName), tx, ty, 15, kColorTextMuted);
    ty += 28;
    drawText(tr("ui.combat.damage") + ": " + std::to_string(preview.damage), tx, ty, 19,
             Color{255, 140, 120, 255});
    ty += 30;
    drawText(tr("ui.combat.hit") + ": " + std::to_string(preview.hitChance) + "%", tx + 235, ty - 30, 17,
             preview.hitChance < 100 ? kColorAccentGold : kColorTextMuted);
    drawText(targetName + " HP: " + std::to_string(preview.targetHpBefore) + " -> " +
                 std::to_string(preview.targetHpAfter),
             tx, ty, 19, Color{235, 90, 90, 255});
}

// Battle Object統合: drawCombatPreviewPopup()のObject版。Objectには命中率・
// 反撃・状態異常が無いためDamageと耐久のみを表示する簡略版。
void drawObjectAttackPreviewPopup(const jf::ObjectAttackPreview& preview) {
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
    std::string objectName = battleObjectNameForKind(preview.objectKind);

    drawText(tr("ui.combat.preview_title"), tx, ty, 20, kColorAccentGold);
    ty += 34;
    drawText(attackerName + "  ->  " + objectName, tx, ty, 22, kColorTextPrimary);
    ty += 44;
    drawText(tr("ui.combat.damage") + ": " + std::to_string(preview.damage), tx, ty, 19,
             Color{255, 140, 120, 255});
    ty += 30;
    drawText(objectName + " HP: " + std::to_string(preview.durabilityBefore) + " -> " +
                 std::to_string(preview.durabilityAfter),
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
void drawHoverInfo(const jf::GameData& data, const jf::BattleController& controller, Vector2 mouse) {
    jf::GridPos pos;
    if (!tileFromScreen(mouse, pos)) return;

    const jf::BattleState& battle = controller.battle();
    std::vector<TooltipLine> lines;

    if (const jf::Unit* unit = battle.unitAt(pos)) {
        Color nameColor = teamColor(unit->team);
        lines.push_back({unitDisplayNameFor(unit->name) + "  " + classNameFor(data, unit->unitClass), nameColor, 17});
        lines.push_back({tr("ui.unit.hp_label") + " " + std::to_string(unit->currentHp) + " / " +
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
                effect += "  " + tr("ui.combat.evasion") + " +" + std::to_string(jf::evasionBonus(terrain)) + "%";
            if (terrain == jf::TerrainType::HerbPatch) effect += "  HP +5";
            lines.push_back({effect, Color{175, 208, 184, 255}, 12});
        }
    } else {
        jf::TerrainType terrain = battle.terrainAt(pos);
        lines.push_back({terrainNameFor(terrain), RAYWHITE, 16});
        if (const jf::BattleObjectState* object = battle.objectAt(pos);
            object && object->state != jf::BattleObjectStateKind::Destroyed) {
            if (const jf::BattleObjectDefinition* definition = battle.objectDefinition(object->definitionId)) {
                lines.push_back({battleObjectNameFor(*definition), Color{244, 199, 92, 255}, 16});
                if (definition->blocksMovement || definition->blocksStopping)
                    lines.push_back({tr("ui.tile.blocks_movement"),
                                     Color{225, 120, 120, 255}, 13});
            }
        }
        if (!jf::isPassable(terrain)) {
            lines.push_back({tr("ui.tile.impassable"), Color{225, 120, 120, 255}, 13});
        } else {
            lines.push_back({tr("ui.tile.move_cost") + " " + std::to_string(jf::movementCost(terrain)),
                              Color{190, 190, 205, 255}, 13});
            int def = jf::defenseBonus(terrain);
            if (def > 0) lines.push_back({"DEF +" + std::to_string(def), Color{175, 208, 184, 255}, 13});
            int evasion = jf::evasionBonus(terrain);
            if (evasion > 0)
                lines.push_back({tr("ui.combat.evasion") + " +" + std::to_string(evasion) + "%",
                                 Color{175, 208, 184, 255}, 13});
            if (terrain == jf::TerrainType::HerbPatch)
                lines.push_back({tr("ui.tile.heal_5_hp"),
                                 Color{130, 210, 145, 255}, 13});
        }
    }

    drawTooltipBox(mouse, lines);
}

// The fixed 5-slot action button row (plus the conditional 6th "Interact"
// slot - see its own comment below) at the bottom of the Battle HUD, and
// every inputState()-specific variant of it: item/skill sub-menus,
// Confirm/Cancel pairs, and the plain "Cancel" shown while selecting a
// target. Split out of drawBattleHud() itself (which also draws the
// mission name / selected-unit info / step label above this row) purely to
// keep each piece under a screenful - no behavior change.
void drawBattleActionButtons(jf::GameApp& app, jf::BattleController& controller, Vector2 mouse, bool clicked,
                             int hudTop, int buttonWidth, int buttonHeight, int buttonGap, int firstActionX,
                             int secondActionX, int thirdActionX, int fourthActionX, int fifthActionX,
                             int sixthActionX) {
    switch (controller.inputState()) {
        case jf::BattleInputState::SelectUnit:
            if (!gBattleItemMenuOpen) {
                if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.items"), mouse, clicked)) {
                    gBattleItemMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.end_turn"), mouse, clicked)) {
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
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                               en + " " + std::to_string(count), ja + " " + std::to_string(count), mouse, clicked) &&
                        app.chooseNeutralBattleHealingItem(type)) {
                        gBattleItemMenuOpen = false;
                    }
                    itemX += buttonWidth + buttonGap;
                    ++usableItemCount;
                };
                drawNeutralHealingItem(jf::ItemType::FieldTreatmentKit, tr("item.field_treatment_kit.short_name"),
                                        tr("item.field_treatment_kit.short_name"));
                if (usableItemCount == 0) {
                    drawText(tr("ui.battle.no_usable_items"), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.back"), mouse, clicked)) {
                    gBattleItemMenuOpen = false;
                }
            }
            break;
        case jf::BattleInputState::SelectAction:
            if (!gBattleItemMenuOpen && !gBattleSkillMenuOpen) {
                if (controller.canInteract() &&
                    button(Rectangle{static_cast<float>(sixthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.interact"), mouse, clicked)) {
                    controller.chooseInteract();
                }
                if (button(Rectangle{static_cast<float>(firstActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.attack"), mouse, clicked)) {
                    controller.chooseAttack();
                }
                if (button(Rectangle{static_cast<float>(secondActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.skills"), mouse, clicked)) {
                    gBattleSkillMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.items"), mouse, clicked)) {
                    gBattleItemMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.wait"), mouse, clicked)) {
                    controller.chooseWait();
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.back"), mouse, clicked)) {
                    controller.returnToMoveSelection();
                }
                break;
            }

            if (gBattleSkillMenuOpen) {
                jf::Unit* selected = controller.selectedUnit();
                bool anySkillShown = false;
                if (selected && jf::canHeal(selected->unitClass)) {
                    anySkillShown = true;
                    if (button(Rectangle{static_cast<float>(firstActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                               tr("ui.button.heal"), mouse, clicked)) {
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
                        Rectangle rect{static_cast<float>(slotX[i]), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)};
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
                    drawText(tr("ui.battle.no_usable_skills"), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.back"), mouse, clicked)) {
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
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                               en + " " + std::to_string(count), ja + " " + std::to_string(count), mouse, clicked) &&
                        app.useBattleHealingItem(type)) {
                        gBattleItemMenuOpen = false;
                    }
                    itemX += buttonWidth + buttonGap;
                    ++usableItemCount;
                };
                drawHealingItem(jf::ItemType::FieldTreatmentKit, tr("item.field_treatment_kit.short_name"),
                                tr("item.field_treatment_kit.short_name"));

                const int boardCount = app.expedition().count(jf::ItemType::ProtectiveBoard);
                if (boardCount > 0) {
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                               tr("item.protective_board.short_name") + " " + std::to_string(boardCount),
                               tr("item.protective_board.short_name") + " " + std::to_string(boardCount), mouse,
                               clicked) &&
                        app.chooseProtectiveBoard()) {
                        gBattleItemMenuOpen = false;
                    }
                    ++usableItemCount;
                }
                if (usableItemCount == 0) {
                    drawText(tr("ui.battle.no_usable_items"), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.back"), mouse, clicked)) {
                    gBattleItemMenuOpen = false;
                }
            }
            break;
        case jf::BattleInputState::ConfirmAttack:
            // The readable preview is drawn as its own popup (see
            // drawCombatPreviewPopup); the HUD here just keeps the buttons.
            if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.confirm"), mouse, clicked))
                controller.confirmAttack();
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.cancel"), mouse, clicked))
                controller.cancelAttackSelection();
            break;
        case jf::BattleInputState::ConfirmSkillAttack:
            // M4 item 3: same Confirm/Cancel pair as ConfirmAttack, for the
            // 3 attack-shape skills' pre-resolution preview.
            if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.confirm"), mouse, clicked))
                controller.confirmSkillAttack();
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.cancel"), mouse, clicked))
                controller.cancelAttackSelection();
            break;
        case jf::BattleInputState::ConfirmObjectAttack:
            // Battle Object統合: 同じConfirm/Cancelペア。Previewは
            // drawCombatPreviewPopup相当の別ポップアップ側で描画。
            if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.confirm"), mouse, clicked)) {
                // attackEventId()方式(lastAttacker_/lastAttackTarget_)はUnit
                // 専用のためObjectには使えない - ここでConfirm直前にPreviewを
                // 読み、確定後のバナー文言をその場で組み立てる
                auto preview = controller.pendingObjectPreview();
                controller.confirmObjectAttack();
                if (preview) {
                    std::string objectName = battleObjectNameForKind(preview->objectKind);
                    pushBattleMessage(
                        objectHitMessageText(unitDisplayNameFor(preview->attackerName), objectName, preview->damage),
                        Color{255, 205, 120, 255});
                    if (preview->durabilityAfter <= 0)
                        pushBattleMessage(objectDestroyedMessageText(objectName), Color{225, 90, 90, 255});
                }
            }
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.cancel"), mouse, clicked))
                controller.cancelAttackSelection();
            break;
        case jf::BattleInputState::SelectMove:
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.cancel"), mouse, clicked))
                controller.cancelToUnitSelect();
            break;
        case jf::BattleInputState::SelectTarget:
        case jf::BattleInputState::SelectHealTarget:
        case jf::BattleInputState::SelectItemTarget:
        case jf::BattleInputState::SelectBoardTarget:
        case jf::BattleInputState::SelectSkillTarget:
        case jf::BattleInputState::SelectInteractTarget:
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.back"), mouse, clicked))
                controller.cancelAttackSelection();
            break;
        default:
            break;
    }
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
    // Battle Object統合(Interact配線): 5固定Slotの外側に置く6個目。他4Actionと
    // 違い「対象があるSelectAction中だけ現れる」条件付きButtonのため、既存Slotを
    // ずらさず追加した(現状出荷済みコンテンツにInteract可能なObjectが無いため
    // 通常プレイでは表示されない)。
    int sixthActionX = firstActionX - buttonWidth - buttonGap;

    DrawRectangleGradientV(0, hudTop, kScreenWidth, kScreenHeight - hudTop, Color{24, 28, 39, 255},
                           Color{15, 17, 24, 255});
    DrawLine(0, hudTop, kScreenWidth, hudTop, withAlpha(kColorBorder, 200));
    DrawRectangle(0, hudTop - 44, kScreenWidth, 44, Color{16, 20, 29, 245});
    DrawLine(0, hudTop - 44, kScreenWidth, hudTop - 44, withAlpha(kColorBorderSoft, 180));
    std::string missionName = pick(app.currentMissionName(), app.currentMissionNameJa());
    drawText(tr("ui.battle.location_prefix", {{"mission", missionName}}),
             18, hudTop - 36, 17, kColorAccentGold);

    if (jf::Unit* selected = controller.selectedUnit()) {
        drawText(unitDisplayNameFor(selected->name) + "  " + classNameFor(app.gameData(), selected->unitClass), 18, hudTop + 12, 19,
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
        drawText(tr("ui.battle.select_unit_prompt"), 18, hudTop + 33, 15, kColorTextMuted);
    }

    std::string stepLabel;
    switch (controller.inputState()) {
        case jf::BattleInputState::SelectMove:
            stepLabel = tr("ui.battle.choose_move");
            break;
        case jf::BattleInputState::SelectAction:
            if (gBattleItemMenuOpen) stepLabel = tr("ui.battle.choose_item");
            else if (gBattleSkillMenuOpen) stepLabel = tr("ui.battle.choose_skill");
            else stepLabel = tr("ui.battle.choose_action");
            break;
        case jf::BattleInputState::SelectItemTarget:
            stepLabel = tr("ui.battle.choose_item_target");
            break;
        case jf::BattleInputState::SelectTarget:
            stepLabel = tr("ui.battle.choose_target");
            break;
        case jf::BattleInputState::SelectHealTarget:
            stepLabel = tr("ui.button.heal");
            break;
        case jf::BattleInputState::SelectBoardTarget:
            stepLabel = tr("ui.button.place_barrier");
            break;
        case jf::BattleInputState::SelectSkillTarget:
            stepLabel = tr("ui.battle.choose_skill_target");
            break;
        case jf::BattleInputState::SelectInteractTarget:
            stepLabel = tr("ui.button.interact");
            break;
        default:
            break;
    }
    if (!stepLabel.empty()) drawText(stepLabel, 420, hudTop + 12, 15, kColorAccentGold);
    drawBattleActionButtons(app, controller, mouse, clicked, hudTop, buttonWidth, buttonHeight, buttonGap,
                           firstActionX, secondActionX, thirdActionX, fourthActionX, fifthActionX, sixthActionX);
}

void drawVictoryOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 190});
    drawCard(Rectangle{static_cast<float>(kScreenWidth) / 2.0f - 230.0f, 220.0f, 460.0f, 200.0f}, kColorCard,
            withAlpha(kColorAccentGold, 220), 0.1f);
    std::string title = tr("ui.result.victory");
    drawText(title, kScreenWidth / 2 - textWidth(title, 44) / 2, 260, 44, kColorAccentGold);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 345, 260, 50};
    if (button(rect, tr("ui.button.proceed_to_camp"), mouse, clicked)) {
        app.proceedToCamp();
    }
}

void drawDefeatOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 190});
    drawCard(Rectangle{static_cast<float>(kScreenWidth) / 2.0f - 230.0f, 220.0f, 460.0f, 230.0f}, kColorCard,
            withAlpha(Color{200, 70, 70, 255}, 220), 0.1f);
    std::string title = tr("ui.result.defeat_title");
    drawText(title, kScreenWidth / 2 - textWidth(title, 38) / 2, 260, 38, Color{225, 90, 90, 255});
    std::string line = tr("ui.defeat.loot_lost_line");
    drawText(line, kScreenWidth / 2 - textWidth(line, 16) / 2, 312, 16, kColorTextPrimary);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 380, 260, 50};
    if (button(rect, tr("ui.button.return_to_base"), mouse, clicked)) {
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

// Party HP roster, the "next field" preview card, pending loot/Discoveries,
// battles-won counter, and the carried bag summary. Split out of
// drawCampScreen() - purely informational, no buttons, so it doesn't need
// mouse/clicked. No behavior change.
void drawCampSummary(jf::GameApp& app) {
    int y = 90;
    drawSectionHeading(tr("ui.camp.party_hp"), 54, y, 20);
    y += 30;
    for (const jf::Unit& unit : app.battle().battle().units()) {
        if (unit.team != jf::Team::Player) continue;
        std::string line = unitDisplayNameFor(unit.name) + " (" + classNameFor(app.gameData(), unit.unitClass) + "): " +
                            std::to_string(unit.currentHp) + " / " + std::to_string(unit.stats.maxHp) +
                            (unit.isAlive() ? "" : " - " + tr("ui.unit.down"));
        drawText(line, 60, y, 16, unit.isAlive() ? kColorTextPrimary : Color{225, 100, 100, 255});
        y += 24;
    }

    Rectangle nextFieldBox{650, 88, 580, 210};
    drawCard(nextFieldBox, Color{22, 27, 38, 255}, withAlpha(kColorAccentGold, 210), 0.05f);
    drawText(tr("ui.camp.next_field_title"), 674, 108, 20, kColorAccentGold);
    if (app.expeditionComplete()) {
        drawText(tr("ui.camp.route_complete"), 674, 150, 20, kColorTextPrimary);
        drawText(tr("ui.camp.return_to_secure_loot"),
                 674, 190, 15, kColorTextMuted);
    } else if (app.expedition().regionId == jf::RegionId::AshboughForest) {
        drawText(app.nextMissionNameJa().value_or("次地点"), 674, 146, 22, kColorTextPrimary);
        drawText(tr("ui.camp.next_site_exploration"),
                 674, 184, 15, kColorTextMuted);
        drawText(tr("ui.camp.carry_over_note"),
                 674, 216, 14, kColorTextMuted);
        // Same "Scout Network" gate as the Exploration screen's enemy
        // preview (docs/campaign_balance.md "敵種...の事前公開" is meant to be
        // earned progression, not a free Camp-screen giveaway).
        if (app.scoutNetworkUnlocked()) {
            const auto roster = app.nextSiteEnemyRosterNames();
            if (roster && !roster->empty()) {
                drawText(tr("ui.deployment.enemy_forces") + " (Scout Network)", 674, 244, 14, kColorAccentGold);
                int enemyX = 674;
                for (const std::string& name : *roster) {
                    std::string label = unitDisplayNameFor(name);
                    drawText(label, enemyX, 264, 13, kColorTextFaint);
                    enemyX += textWidth(label, 13) + 20;
                }
            }
        }
    } else if (app.expedition().stageIndex == 0) {
        drawText(tr("cinderwatch.ironwatch_stores"), 674, 146, 22, kColorTextPrimary);
        drawText(tr("cinderwatch.field_ash_road"), 674, 184, 15, kColorTextMuted);
        drawText(tr("cinderwatch.ash_road_note"), 674, 216, 14, kColorTextMuted);
        drawText(tr("cinderwatch.enemy_force_4"), 674, 246, 14, kColorTextFaint);
    } else {
        drawText(tr("cinderwatch.last_signal"), 674, 146, 22, kColorTextPrimary);
        drawText(tr("cinderwatch.field_signal_tower"), 674, 184, 15, kColorTextMuted);
        drawText(tr("cinderwatch.signal_tower_note"), 674, 216, 14, kColorTextMuted);
        drawText(tr("cinderwatch.enemy_force_4_captain"),
                 674, 246, 14, kColorTextFaint);
    }

    y += 20;
    drawSectionHeading(tr("ui.camp.pending_loot"), 54, y, 20);
    y += 30;
    if (app.expedition().pendingLoot.empty()) {
        drawText("(" + tr("ui.camp.none_yet") + ")", 60, y, 16, kColorTextMuted);
        y += 24;
    }
    for (const jf::LootStack& item : app.expedition().pendingLoot) {
        drawText("- " + materialNameFor(item.id) + " x" + std::to_string(item.quantity), 60, y, 16,
                 kColorTextPrimary);
        y += 22;
    }
    for (const jf::DiscoveryId& discovery : app.expedition().pendingDiscoveries) {
        drawText("- " + tr("cinderwatch.discovery_recon_records"), 360, y - 22, 16,
                 kColorAccentGold);
        (void)discovery;
    }

    y += 10;
    std::string won = tr("ui.camp.battles_won") + ": " + std::to_string(app.expedition().battlesWon);
    drawText(won, 40, y, 16, kColorTextPrimary);
    drawText(tr("ui.label.bag") + ": " + std::to_string(app.expedition().bag.size()) + " / 6", 260, y,
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
}

// The Continue Expedition / Return to Base / Items command row at the
// bottom of the Camp screen. Split out of drawCampScreen(); no behavior
// change.
void drawCampCommandButtons(jf::GameApp& app, Vector2 mouse, bool clicked) {
    constexpr float commandY = 724.0f;
    DrawLine(40, commandY - 14, kScreenWidth - 40, commandY - 14, withAlpha(kColorBorderSoft, 200));
    Rectangle continueRect{40, commandY, 360, 50};
    Rectangle returnRect{420, commandY, 360, 50};
    Rectangle itemsRect{800, commandY, 360, 50};
    if (app.expeditionComplete()) {
        disabledButton(continueRect, tr("ui.camp.route_complete_disabled"));
    } else {
        if (button(continueRect, tr("ui.button.continue_expedition"), mouse, clicked)) {
            app.continueExpedition();
        }
    }
    if (button(returnRect, tr("ui.button.return_to_base"), mouse, clicked)) {
        gCampItemMenuOpen = false;
        gCampSelectedItem.reset();
        // docs/inventory_overflow.md「帰還処理」: a false return means the
        // 200-Stack pending ceiling would be exceeded - route to cleanup
        // instead of silently doing nothing.
        if (!app.returnToBase()) gWarehouseCleanupOpen = true;
    }
    if (button(itemsRect, tr("ui.button.items"), mouse, clicked)) {
        gCampItemMenuOpen = true;
        gCampSelectedItem.reset();
    }
}

// The Items sub-panel (usable-item list, then unit-target list once one's
// picked). Split out of drawCampScreen(); no behavior change.
void drawCampItemMenu(jf::GameApp& app, Vector2 mouse, bool clicked) {
    Rectangle itemPanel{620, 300, 610, 390};
    DrawRectangle(0, 70, kScreenWidth, kScreenHeight - 70, Color{0, 0, 0, 105});
    drawCard(itemPanel, Color{20, 25, 36, 255}, withAlpha(kColorAccentGold, 230), 0.05f);
    drawText(gCampSelectedItem ? tr("ui.camp.choose_target")
                               : tr("ui.camp.usable_items"),
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
            drawText(tr("ui.battle.no_usable_items"), 646, 390, 16, kColorTextMuted);
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

    if (button(Rectangle{1090, 626, 116, 44}, tr("ui.button.back"), mouse, clicked)) {
        if (gCampSelectedItem) gCampSelectedItem.reset();
        else gCampItemMenuOpen = false;
    }
}

void drawCampScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    DrawRectangleGradientV(0, 0, kScreenWidth, 70, Color{40, 46, 62, 255}, Color{24, 27, 38, 255});
    DrawLine(0, 70, kScreenWidth, 70, withAlpha(kColorBorder, 160));
    drawText(tr("ui.camp.title"), 40, 30, 32, kColorTextPrimary);

    if (app.justSecuredLoot()) {
        drawSectionHeading(tr("ui.camp.loot_secured"), 54, 90, 24);
        int y = 140;
        for (const std::string& item : app.lastSecuredLoot()) {
            drawText("- " + materialNameFor(item), 60, y, 18, kColorTextPrimary);
            y += 26;
        }
        if (app.lastSecuredLoot().empty()) {
            drawText("(" + tr("ui.camp.no_loot") + ")", 60, y, 16,
                     kColorTextMuted);
            y += 26;
        }
        Rectangle rect{40, static_cast<float>(y + 30), 260, 50};
        if (button(rect, tr("ui.button.continue"), mouse, clicked)) {
            app.acknowledgeLootSecured();
        }
        return;
    }

    drawCampSummary(app);
    if (!gCampItemMenuOpen) drawCampCommandButtons(app, mouse, clicked);
    if (gCampItemMenuOpen) drawCampItemMenu(app, mouse, clicked);
}

// Top bar: screen title plus the Facilities/Warehouse buttons. Split out of
// drawBaseScreen(); no behavior change.
void drawBaseTopBar(jf::GameApp& app, Vector2 mouse, bool clicked) {
    drawText(tr("ui.prep.title"), 38, 30, 30, kColorTextPrimary);
    Rectangle facilitiesRect{static_cast<float>(kScreenWidth) - 218.0f, 4.0f, 110.0f, 32.0f};
    if (button(facilitiesRect, tr("ui.facilities.title"), mouse, clicked)) {
        gVisitedFacility.reset();
        gForgeCraftClass.reset();
        gShowFacilities = true;
    }
    Rectangle warehouseRect{static_cast<float>(kScreenWidth) - 336.0f, 4.0f, 110.0f, 32.0f};
    if (button(warehouseRect, tr("ui.warehouse.open_button"), mouse, clicked)) gWarehouseCleanupOpen = true;
}

// The 4-of-roster party picker column. Split out of drawBaseScreen();
// `hoverLines` is the single shared tooltip buffer every column of this
// screen can write into (drawn once, at the very end) - no behavior change.
void drawBasePartyRoster(jf::GameApp& app, Vector2 mouse, bool clicked, std::vector<TooltipLine>& hoverLines) {
    drawSectionHeading(tr("ui.prep.party_choose4"), 52, 92, 20);
    int y = 125;
    for (const auto& unit : app.roster()) {
        bool selected = std::find(app.selectedPartyIds().begin(), app.selectedPartyIds().end(), unit.id) != app.selectedPartyIds().end();
        std::string label = std::string(selected ? "[✓] " : "[ ] ") + unitDisplayNameFor(unit.name) + " - " +
                            classNameFor(app.gameData(), unit.classId);
        Rectangle rowRect{40, static_cast<float>(y), 300, 40};
        Rectangle detailRect{348, static_cast<float>(y), 82, 40};
        if (button(rowRect, label, "", mouse, clicked)) app.togglePartyMember(unit.id);
        if (button(detailRect, "Details", "詳細", mouse, clicked)) gViewedUnitId = unit.id;
        if (CheckCollisionPointRec(mouse, rowRect)) {
            const jf::Stats& stats = app.gameData().classDefinition(unit.classId).baseStats;
            hoverLines = {
                {unitDisplayNameFor(unit.name) + "  " + classNameFor(app.gameData(), unit.classId), kColorAccentGold, 17},
                {classRoleFor(app.gameData(), unit.classId), kColorTextMuted, 13},
                {"HP " + std::to_string(stats.maxHp) + "  STR " + std::to_string(stats.strength) + "  MAG " +
                     std::to_string(stats.magic) + "  DEF " + std::to_string(stats.defense) + "  RES " +
                     std::to_string(stats.resistance) + "  MOV " + std::to_string(stats.move),
                 Color{170, 180, 195, 255}, 13},
            };
        }
        y += 45;
    }
}

// The consumable-supplies craft/add column. Split out of drawBaseScreen();
// no behavior change.
void drawBaseSupplies(jf::GameApp& app, Vector2 mouse, bool clicked, std::vector<TooltipLine>& hoverLines) {
    drawSectionHeading(tr("ui.prep.supplies"), 492, 92, 20);
    int y = 125;
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
            if (button(craftRect, tr("ui.button.craft"), mouse, clicked)) app.craftItem(item.type);
        } else {
            disabledButton(craftRect, tr("ui.button.craft"));
        }

        Rectangle addRect{719, static_cast<float>(y), 61, 40};
        if (owned > 0) {
            if (button(addRect, tr("ui.button.add"), mouse, clicked)) app.addPreparedItem(item.type);
        } else {
            disabledButton(addRect, tr("ui.button.add"));
        }

        if (CheckCollisionPointRec(mouse, nameRect) || CheckCollisionPointRec(mouse, craftRect)) {
            hoverLines = {
                {itemFullNameFor(item.type), kColorAccentGold, 16},
                {itemDescriptionFor(item.type), kColorTextMuted, 13},
                {tr("ui.materials_label") + ": " + costLabel, kColorTextMuted, 13},
            };
        } else if (CheckCollisionPointRec(mouse, addRect)) {
            hoverLines = {
                {itemFullNameFor(item.type), kColorAccentGold, 16},
                {itemDescriptionFor(item.type), kColorTextMuted, 13},
            };
        }
        y += 45;
    }
}

// The unlocked/locked region picker list. Split out of drawBaseScreen(); no
// behavior change.
void drawBaseRegionList(jf::GameApp& app, Vector2 mouse, bool clicked, std::vector<TooltipLine>& hoverLines) {
    drawSectionHeading(tr("exploration.region_section"), 480, 405, 18);
    int y = 433;
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
                    {tr("exploration.region_locked_ashbough_forest"), kColorTextMuted, 13},
                };
            }
        }
        y += 45;
    }
}

// The prepared-bag slot list plus the Begin Expedition button. Split out of
// drawBaseScreen(); no behavior change.
void drawBaseBagAndExpedition(jf::GameApp& app, Vector2 mouse, bool clicked, std::vector<TooltipLine>& hoverLines) {
    drawSectionHeading(tr("ui.prep.bag_slots"), 842, 92, 20);
    int y = 125;
    for (std::size_t i = 0; i < jf::ExpeditionState::kBagCapacity; ++i) {
        Rectangle slot{830, static_cast<float>(y), 370, 40};
        if (i < app.preparedBag().size()) {
            std::string label = itemFullNameFor(app.preparedBag()[i]) + "  (" + tr("ui.button.remove") + ")";
            if (button(slot, label, "", mouse, clicked)) app.removePreparedItem(i);
            if (CheckCollisionPointRec(mouse, slot)) {
                hoverLines = {
                    {itemFullNameFor(app.preparedBag()[i]), kColorAccentGold, 16},
                    {itemDescriptionFor(app.preparedBag()[i]), kColorTextMuted, 13},
                };
            }
        } else disabledButton(slot, tr("ui.prep.empty_slot"));
        y += 45;
    }
    Rectangle start{830, 430, 370, 58};
    if (app.selectedPartyIds().size() == 4) {
        if (button(start, tr("ui.button.begin_expedition"), mouse, clicked)) {
            jf::RegionId toStart = app.isRegionUnlocked(gSelectedRegionId) ? gSelectedRegionId : jf::RegionId::AshboughForest;
            app.startExpedition(toStart);
        }
    } else disabledButton(start, tr("ui.validation.select_exactly_4"));
}

// Outpost stage name/advance button and the Discovery registry list. Split
// out of drawBaseScreen(); no behavior change.
void drawBaseOutpostInfo(jf::GameApp& app, Vector2 mouse, bool clicked) {
    const jf::BaseState& base = app.baseState();
    drawSectionHeading(tr("ui.outpost.title"), 40, 520, 20);
    drawText(outpostStageNameFor(base.outpostStage), 40, 552, 22, kColorTextPrimary);
    Rectangle advanceRect{40, 588, 390, 46};
    if (base.outpostStage == jf::OutpostStage::Encampment &&
        jf::eligibleForOutpostStage(base, jf::OutpostStage::PioneerOutpost)) {
        if (button(advanceRect, tr("ui.outpost.advance"), mouse, clicked)) app.advanceOutpostStage();
    } else if (base.outpostStage == jf::OutpostStage::Encampment) {
        disabledButton(advanceRect, tr("ui.outpost.advance_locked"));
    }

    drawSectionHeading(tr("ui.outpost.discoveries"), 492, 520, 20);
    if (base.discoveryRegistry.empty()) {
        drawText(tr("ui.outpost.no_discoveries_yet"), 492, 552, 18, kColorTextMuted);
    } else {
        int discoveryY = 552;
        for (const jf::DiscoveryId& discovery : base.discoveryRegistry) {
            drawText(discoveryNameFor(discovery), 492, discoveryY, 18, kColorTextPrimary);
            discoveryY += 28;
        }
    }
}

void drawBaseScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    drawBaseTopBar(app, mouse, clicked);
    std::vector<TooltipLine> hoverLines;
    drawBasePartyRoster(app, mouse, clicked, hoverLines);
    drawBaseSupplies(app, mouse, clicked, hoverLines);
    drawBaseRegionList(app, mouse, clicked, hoverLines);
    drawBaseBagAndExpedition(app, mouse, clicked, hoverLines);
    drawBaseOutpostInfo(app, mouse, clicked);
    drawTooltipBox(mouse, hoverLines);
}

// docs/exploration_system.md "周回と地域経路の開拓" Japanese labels for
// SiteAccessState - purely a display concern, so it lives here rather than
// on the enum itself.
std::string siteAccessLabel(jf::SiteAccessState state) {
    switch (state) {
        case jf::SiteAccessState::Unknown: return tr("ui.site.unknown");
        case jf::SiteAccessState::Surveyed: return tr("ui.site.surveyed");
        case jf::SiteAccessState::Secured: return tr("ui.site.secured");
    }
    return tr("ui.site.unknown");
}

void drawExplorationScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    drawText(tr("ui.exploration.title"), 42, 30, 28, kColorAccentGold);
    drawText(pick(app.currentMissionName(), app.currentMissionNameJa()), 42, 78, 34, kColorTextPrimary);

    const bool isAshbough = app.expedition().regionId == jf::RegionId::AshboughForest;
    drawText(isAshbough && app.currentMissionNameJa() == "灰枝の林縁"
                 ? tr("exploration.ashbough_verge_situation")
                 : isAshbough
                       ? tr("exploration.next_forest_site")
                        : tr("exploration.cinderwatch_situation"),
             42, 135, 18, kColorTextMuted);
    {
        std::string statusText = tr("ui.site.status_prefix") + siteAccessLabel(app.currentSiteAccess());
        int statusWidth = textWidth(statusText, 16);
        drawText(statusText, kScreenWidth - statusWidth - 42, 34, 16, kColorAccentGold);
    }

    if (!app.currentSiteContentImplemented()) {
        Rectangle pendingBox{160, 260, 960, 180};
        drawCard(pendingBox, Color{22, 27, 38, 255}, withAlpha(kColorAccentGold, 180), 0.04f);
        drawText(tr("ui.exploration.site_reached"), 194, 294, 20, kColorAccentGold);
        drawText(tr("ui.exploration.pending_content"),
                 194, 344, 18, kColorTextPrimary);
        drawText(tr("ui.exploration.pending_checkpoint"),
                 194, 386, 16, kColorTextMuted);
        return;
    }

    if (app.currentSiteAccess() == jf::SiteAccessState::Secured) {
        // docs/exploration_system.md "確保済み地点の通過": no battle, no
        // exploration choice, no reward for the safe route; a fresh battle
        // for ordinary-material-only rewards for reconnaissance.
        Rectangle safeRect{160, 260, 960, 120};
        if (button(safeRect, tr("ui.button.safe_passage"), mouse, clicked)) app.chooseSafePassage();
        drawText(tr("exploration.safe_passage_effect"), 182, 345, 16,
                 kColorTextMuted);

        Rectangle reconRect{160, 410, 960, 120};
        if (button(reconRect, tr("exploration.reconnaissance"), mouse, clicked))
            app.chooseReconnaissance();
        drawText(tr("exploration.reconnaissance_effect"),
                 182, 495, 16, kColorTextMuted);
        return;
    }

    // Command Post "Scout Network" node effect: reveal what's waiting ahead
    // regardless of which route gets picked.
    if (app.scoutNetworkUnlocked()) {
        drawText(tr("ui.deployment.enemy_forces") + " (Scout Network)", 42, 168, 16, kColorAccentGold);
        int enemyX = 42;
        for (const jf::Unit& enemy : app.explorationEnemyPreview()) {
            drawText(unitDisplayNameFor(enemy.name), enemyX, 192, 15, kColorTextMuted);
            enemyX += textWidth(unitDisplayNameFor(enemy.name), 15) + 26;
        }
    }

    Rectangle frontal{70, 225, 520, 120};
    Rectangle sidePath{650, 225, 520, 120};
    if (button(frontal, isAshbough ? tr("exploration.ashbough_frontal") : "A. " + tr("exploration.frontal_advance"),
              mouse, clicked))
        app.chooseExplorationRoute(jf::ExplorationChoice::FrontalAdvance);
    drawText(isAshbough ? tr("exploration.ashbough_frontal_effect") : tr("exploration.frontal_effect"),
             92, 310, 15, kColorTextMuted);
    if (button(sidePath, isAshbough ? tr("exploration.ashbough_side_path") : "B. " + tr("exploration.side_path"),
              mouse, clicked))
        app.chooseExplorationRoute(jf::ExplorationChoice::CollapsedSidePath);
    drawText(isAshbough ? tr("exploration.ashbough_side_path_effect") : tr("exploration.side_path_effect"),
             672, 310, 15, kColorTextMuted);

    Rectangle scoutRect{360, 400, 560, 90};
    std::string scoutLabel =
        "C. " + (isAshbough ? tr("exploration.ashbough_scout_route") : tr("exploration.scout_route"));
    if (app.partyHasFrontierScout()) {
        if (button(scoutRect, scoutLabel, mouse, clicked))
            app.chooseExplorationRoute(jf::ExplorationChoice::ScoutRoute);
        drawText(isAshbough ? tr("exploration.ashbough_scout_route_effect") : tr("exploration.scout_route_effect"),
                 382, 470, 15, kColorTextMuted);
    } else {
        disabledButton(scoutRect, scoutLabel);
        drawText(tr("exploration.scout_route_locked"), 382, 470, 15,
                 Color{200, 110, 110, 255});
    }
}

// Which of the 4 party slots free-placement clicks currently apply to.
// Purely UI state - GameApp only knows about confirmed placements.
int gDeploymentSelectedSlot = 0;

// Board tiles, enemy preview, placed allies, and the placement click
// handler. Split out of drawPreBattleDeploymentScreen(); no behavior
// change.
void drawDeploymentBoard(jf::GameApp& app, Vector2 mouse, bool clicked, const std::vector<jf::Unit>& players,
                          int maxCol) {
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
}

// HUD bar: per-slot placement buttons, the "deploy all" hint, and the
// Back/Begin Battle buttons. Split out of drawPreBattleDeploymentScreen();
// no behavior change.
void drawDeploymentHud(jf::GameApp& app, Vector2 mouse, bool clicked, const std::vector<jf::Unit>& players) {
    int hudTop = static_cast<int>(kHudY);
    DrawRectangleGradientV(0, hudTop, kScreenWidth, kScreenHeight - hudTop, Color{24, 28, 39, 255},
                           Color{15, 17, 24, 255});
    DrawLine(0, hudTop, kScreenWidth, hudTop, withAlpha(kColorBorder, 200));

    constexpr float kSlotWidth = 300.0f;
    for (std::size_t i = 0; i < players.size(); ++i) {
        bool placed = app.isDeploymentUnitPlaced(i);
        std::string status = tr(placed ? "ui.deployment.placed" : "ui.deployment.unplaced");
        std::string label = unitDisplayNameFor(players[i].name) + " (" + classNameFor(app.gameData(), players[i].unitClass) +
                            ")  [" + status + "]";
        Rectangle slotRect{18.0f + static_cast<float>(i) * (kSlotWidth + 8.0f), hudTop + 8.0f, kSlotWidth, 34.0f};
        if (button(slotRect, label, "", mouse, clicked)) gDeploymentSelectedSlot = static_cast<int>(i);
        if (static_cast<int>(i) == gDeploymentSelectedSlot)
            DrawRectangleRoundedLinesEx(slotRect, 0.28f, 8, 3.0f, withAlpha(kColorAccentGold, 255));
    }

    if (!app.allDeploymentUnitsPlaced()) {
        drawText(tr("ui.deployment.deploy_all_to_start"), 18, hudTop + 54, 15, kColorTextMuted);
    }

    Rectangle backRect{static_cast<float>(kScreenWidth) - 460.0f, hudTop + 48.0f, 220.0f, 40.0f};
    Rectangle beginRect{static_cast<float>(kScreenWidth) - 230.0f, hudTop + 48.0f, 212.0f, 40.0f};
    if (button(backRect, tr("ui.button.back"), mouse, clicked)) {
        app.cancelDeployment();
        gDeploymentSelectedSlot = 0;
    }
    if (app.allDeploymentUnitsPlaced()) {
        if (button(beginRect, tr("ui.button.begin_battle"), mouse, clicked)) {
            app.confirmDeployment();
            gDeploymentSelectedSlot = 0;
        }
    } else {
        disabledButton(beginRect, tr("ui.button.begin_battle"));
    }
}

void drawPreBattleDeploymentScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{16, 18, 26, 255});
    DrawRectangleGradientV(0, 0, kScreenWidth, 70, Color{40, 46, 62, 255}, Color{24, 27, 38, 255});
    DrawLine(0, 70, kScreenWidth, 70, withAlpha(kColorBorder, 160));
    drawText(tr("ui.deployment.title"), 40, 20, 26, kColorTextPrimary);
    drawText(tr("ui.deployment.instructions"), 40,
             50, 15, kColorTextMuted);
    drawText(tr("ui.deployment.enemy_forces") + " (Preview)", kScreenWidth - 300, 50, 15, kColorTextFaint);

    const std::vector<jf::Unit>& players = app.deploymentPlayers();
    int maxCol = app.deploymentMaxColumn();

    drawDeploymentBoard(app, mouse, clicked, players, maxCol);
    drawDeploymentHud(app, mouse, clicked, players);
}

std::string facilityIdNameFor(jf::FacilityId id) {
    switch (id) {
        case jf::FacilityId::CommandPost: return tr("facility.command_post.name");
        case jf::FacilityId::TrainingGround: return tr("facility.training_ground.name");
        case jf::FacilityId::Forge: return tr("facility.forge.name");
        case jf::FacilityId::Infirmary: return tr("facility.infirmary.name");
        case jf::FacilityId::Workshop: return tr("facility.workshop.name");
        case jf::FacilityId::Barracks: return tr("facility.barracks.name");
    }
    return "";
}

std::string facilityRoleFor(jf::FacilityId id) {
    switch (id) {
        case jf::FacilityId::CommandPost: return tr("facility.command_post.role");
        case jf::FacilityId::TrainingGround: return tr("facility.training_ground.role");
        case jf::FacilityId::Forge: return tr("facility.forge.role");
        case jf::FacilityId::Infirmary: return tr("facility.infirmary.role");
        case jf::FacilityId::Workshop: return tr("facility.workshop.role");
        case jf::FacilityId::Barracks: return tr("facility.barracks.role");
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

// docs/base_development.md: facilities have no active/inactive toggle state -
// a node is either constructed (permanently) or not. This reports the latter,
// never an operational status.
bool facilityIsConstructed(const jf::BaseState& base, jf::FacilityId facility) {
    bool hasConstructibleRoot = false;
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.facility != facility || !node.occupiesFacilitySlot) continue;
        hasConstructibleRoot = true;
        if (base.constructedFacilityIds.count(node.id)) return true;
    }
    if (hasConstructibleRoot) return false;
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
    const std::string title = facilityIdNameFor(facility) + "  " +
                              tr("ui.facilities.branches_unlocked", {{"count", std::to_string(facilityLevel(base, facility))}});
    drawText(title, static_cast<int>(x + 22), static_cast<int>(y + 18), 18, kColorAccentGold);
    const bool constructed = facilityIsConstructed(base, facility);
    drawText(constructed ? tr("ui.facility_node.constructed") : tr("ui.facility_node.not_constructed"),
             static_cast<int>(x + 22), static_cast<int>(y + 62), 12,
             constructed ? Color{105, 205, 145, 255} : Color{180, 125, 125, 255});
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
    drawText(tr("ui.materials_label"), static_cast<int>(x + 22),
             static_cast<int>(rowY), 12, kColorTextFaint);
    rowY += 34;
    if (node.materialCosts.empty()) {
        drawText(tr("ui.no_material_cost"), static_cast<int>(x + 30), static_cast<int>(rowY), 13,
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
        return tr("ui.facilities.needs_stage_prefix") +
               (compact ? outpostStageShortNameFor(node.requiredStage) : outpostStageNameFor(node.requiredStage));
    }
    for (const jf::DiscoveryId& discovery : node.requiredDiscoveries)
        if (!base.discoveryRegistry.count(discovery)) return tr("ui.facilities.needs_discovery");
    for (const std::string& prereqId : node.prerequisiteNodeIds) {
        const jf::FacilityNode* prereq = jf::findFacilityNode(prereqId);
        bool satisfied = prereq && prereq->occupiesFacilitySlot ? base.constructedFacilityIds.count(prereqId) > 0
                                                                 : base.unlockedNodeIds.count(prereqId) > 0;
        if (!satisfied) return tr("ui.facilities.needs_facility_built");
    }
    for (const jf::LootStack& cost : node.materialCosts) {
        if (base.storageCount(cost.id) < cost.quantity)
            return tr("ui.facilities.needs_material_prefix") + materialNameFor(cost.id);
    }
    return "";
}

// docs/base_development.md: no facility-slot cap, no dismantle, no rebuild -
// a node is either unlocked (and, for the 4 optional facilities, therefore
// permanently built) or it isn't. Only 3 row states exist: unlocked, eligible
// (shows a Build/Unlock button), or blocked (shows why).
void drawFacilityNodeRow(jf::GameApp& app, const jf::FacilityNode& node, float x, float y, float width,
                         Vector2 mouse, bool clicked) {
    const jf::BaseState& base = app.baseState();
    bool unlocked = base.unlockedNodeIds.count(node.id) > 0;

    constexpr float kActionZoneWidth = 132;
    constexpr float kNameLeftPad = 14;
    Rectangle actionRect{x + width - kActionZoneWidth, y, kActionZoneWidth, 24};
    // Leaves a visible gap before the action zone so long names/reasons can
    // never visually run into the button/label on the right.
    const int nameMaxWidth = static_cast<int>(actionRect.x - (x + kNameLeftPad) - 10);

    DrawCircle(static_cast<int>(x + 5), static_cast<int>(y + 12), 3.0f,
               unlocked ? Color{95, 205, 140, 255} : kColorTextFaint);
    drawText(clipTextToWidth(pick(node.nameEn, node.nameJa), 14, nameMaxWidth), static_cast<int>(x + kNameLeftPad),
             static_cast<int>(y) + 5, 14, unlocked ? kColorTextPrimary : kColorTextMuted);

    if (unlocked) {
        drawText(tr("ui.facilities.unlocked"), static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13,
                 kColorTextFaint);
    } else if (jf::facilityNodeEligible(base, node)) {
        std::string label = node.occupiesFacilitySlot ? tr("ui.facilities.build") : tr("ui.facilities.unlock");
        if (button(actionRect, label, mouse, clicked)) app.unlockFacilityNode(node.id);
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
    drawSectionHeading(tr("ui.forge.equipment_heading"), static_cast<int>(x),
                       static_cast<int>(y), 18);
    const jf::BaseState& base = app.baseState();
    auto overrideIt = app.weaponOverrides().find(unit.id);
    std::string current = overrideIt != app.weaponOverrides().end() ? overrideIt->second : "iron_spear";
    drawText(tr("ui.facilities.current_weapon_prefix") + weaponNameFor(current, weaponEnglishName(current)),
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
        if (button(traitRect, tr(traitEquipped ? "ui.facilities.unequip_trait" : "ui.facilities.equip_trait"), mouse,
                  clicked)) {
            app.equipTuningTraitForUnit(unit.id,
                                        traitEquipped ? jf::TuningTraitId::None : jf::TuningTraitId::HideWrappedGrip);
        }
    } else {
        disabledButton(traitRect, tr("ui.facilities.trait_locked"));
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
    drawText(tr("ui.unit_screen.title"), 38, 24, 28, kColorTextPrimary);
    Rectangle backRect{static_cast<float>(kScreenWidth) - 258.0f, 4.0f, 150.0f, 32.0f};
    if (button(backRect, "Party List", "編成一覧へ", mouse, clicked)) {
        gViewedUnitId.reset();
        return;
    }

    Rectangle identity{42, 104, 470, 500};
    drawCard(identity, kColorCard, kColorBorderSoft, 0.04f);
    drawText(unitDisplayNameFor(unit->name), 72, 132, 28, kColorAccentGold);
    drawText(classNameFor(app.gameData(), unit->classId), 72, 184, 20, kColorTextPrimary);
    const std::string role = wrapTextToWidth(classRoleFor(app.gameData(), unit->classId), 14, 400);
    drawText(role, 72, 230, 14, kColorTextMuted);
    const jf::Stats& stats = app.gameData().classDefinition(unit->classId).baseStats;
    drawSectionHeading(tr("ui.unit_screen.stats"), 72, 330, 18);
    drawText("HP " + std::to_string(stats.maxHp) + "    STR " + std::to_string(stats.strength) +
                 "    MAG " + std::to_string(stats.magic), 72, 374, 16, kColorTextPrimary);
    drawText("DEF " + std::to_string(stats.defense) + "    RES " + std::to_string(stats.resistance) +
                 "    MOV " + std::to_string(stats.move), 72, 418, 16, kColorTextPrimary);

    Rectangle equipment{548, 104, 690, 500};
    drawCard(equipment, kColorCard, kColorBorderSoft, 0.04f);
    if (unit->classId == jf::UnitClass::Spearman) {
        drawForgeEquipmentPanel(app, *unit, 580, 136, 626, mouse, clicked);
        if (!app.baseState().constructedFacilityIds.count("simple_forge"))
            drawText(tr("ui.unit_screen.needs_forge"),
                     580, 390, 14, Color{205, 135, 135, 255});
    } else {
        drawSectionHeading(tr("ui.unit_screen.equipment_heading"), 580, 136, 18);
        const std::string weaponId = app.gameData().classDefinition(unit->classId).weaponId;
        drawText(tr("ui.unit_screen.current_weapon_prefix") + weaponNameFor(weaponId, weaponEnglishName(weaponId)),
                 580, 188, 16, kColorTextPrimary);
        drawText(tr("ui.unit_screen.no_alt_equipment"),
                 580, 246, 14, kColorTextMuted);
    }
}

// The facility card grid (list view, shown when no facility is visited
// yet). Split out of drawFacilitiesScreen(); no behavior change.
void drawFacilitiesList(jf::GameApp& app, Vector2 mouse, bool clicked, const jf::BaseState& base,
                         const Rectangle& backRect) {
    drawText(tr("ui.facilities.title"), 38, 24, 28, kColorTextPrimary);
    drawText(outpostStageNameFor(base.outpostStage), 38, 60, 16, kColorTextMuted);
    if (button(backRect, tr("ui.button.back"), mouse, clicked)) {
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
        const bool constructed = facilityIsConstructed(base, facility);
        drawText(tr("ui.facilities.branches_unlocked", {{"count", std::to_string(facilityLevel(base, facility))}}),
                 static_cast<int>(card.x + 22), static_cast<int>(card.y + 59), 14, kColorAccentGold);
        drawText(constructed ? tr("ui.facility_node.constructed") : tr("ui.facility_node.not_constructed"),
                 static_cast<int>(card.x + 112), static_cast<int>(card.y + 59), 14,
                 constructed ? Color{105, 205, 145, 255} : Color{180, 125, 125, 255});
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
}

// The visited-facility detail page: upgrade/recipe node list plus the info
// panel (including the Forge's class-recipe picker). Split out of
// drawFacilitiesScreen(); no behavior change.
void drawFacilityDetail(jf::GameApp& app, Vector2 mouse, bool clicked, const jf::BaseState& base,
                         const Rectangle& backRect) {
    const jf::FacilityId facility = *gVisitedFacility;
    const bool forgeCraftPage = facility == jf::FacilityId::Forge && gForgeCraftClass.has_value();
    const std::string pageTitle = forgeCraftPage
                                      ? tr("ui.forge.craft_prefix") + classNameFor(app.gameData(), *gForgeCraftClass)
                                      : facilityIdNameFor(facility);
    drawText(pageTitle, 38, 24, 28, kColorTextPrimary);
    drawText(tr("ui.facilities.branches_unlocked", {{"count", std::to_string(facilityLevel(base, facility))}}),
             38, 64, 16, kColorAccentGold);
    drawText(facilityRoleFor(facility), 138, 58, 14, kColorTextMuted);
    if (forgeCraftPage) {
        if (button(backRect, tr("ui.forge.back_to_forge"), mouse, clicked)) gForgeCraftClass.reset();
    } else if (button(backRect, tr("ui.facilities.back_to_list"), mouse, clicked)) {
        gVisitedFacility.reset();
    }

    drawSectionHeading(forgeCraftPage ? tr("ui.forge.recipes") : tr("ui.forge.upgrades"),
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
    drawSectionHeading(forgeCraftPage ? tr("ui.forge.class_recipes")
                                      : tr("ui.forge.facility_details"),
                       794, 150, 18);
    if (facility == jf::FacilityId::Forge && !forgeCraftPage) {
        drawText(tr("ui.forge.choose_class"),
                 794, 192, 14, kColorTextPrimary);
        const jf::UnitClass craftClasses[] = {
            jf::UnitClass::MarchCaptain, jf::UnitClass::VeteranGuard, jf::UnitClass::WatchArcher,
            jf::UnitClass::FrontierScout, jf::UnitClass::Spearman, jf::UnitClass::DawnChirurgeon,
        };
        for (int index = 0; index < 6; ++index) {
            Rectangle craftRect{794.0f, 232.0f + index * 58.0f, 410.0f, 46.0f};
            const std::string label = tr("ui.forge.craft_prefix") + classNameFor(app.gameData(), craftClasses[index]);
            if (craftClasses[index] == jf::UnitClass::Spearman) {
                if (button(craftRect, label, mouse, clicked)) gForgeCraftClass = craftClasses[index];
            } else {
                disabledButton(craftRect, label + tr("ui.forge.craft_planned_suffix"));
            }
        }
    } else {
        drawText(forgeCraftPage ? tr("ui.forge.crafted_weapons_note")
                                : facilityRoleFor(facility),
                 794, 192, 14, kColorTextPrimary);
        drawText(tr("ui.forge.hover_hint"),
                 794, 292, 14, kColorTextMuted);
    }
    if (hoveredNode) drawFacilityNodeTooltip(*hoveredNode, base, mouse);
}

void drawFacilitiesScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    const jf::BaseState& base = app.baseState();
    Rectangle backRect{static_cast<float>(kScreenWidth) - 258.0f, 4.0f, 150.0f, 32.0f};

    if (!gVisitedFacility) {
        drawFacilitiesList(app, mouse, clicked, base, backRect);
        return;
    }

    drawFacilityDetail(app, mouse, clicked, base, backRect);
}

// The discard-confirmation sub-panel (shown when a discard button was just
// clicked). Split out of drawWarehouseCleanupOverlay(); advances `y` past
// itself when open, exactly as the original inline block did. No behavior
// change.
void drawWarehouseDiscardConfirm(jf::GameApp& app, Vector2 mouse, bool clicked, const Rectangle& panel, float& y) {
    if (!gWarehouseDiscardConfirm) return;
    const WarehouseDiscardTarget& target = *gWarehouseDiscardConfirm;
    Rectangle confirmPanel{panel.x + 26, y, panel.width - 52, 100};
    drawCard(confirmPanel, kColorCardAlt, withAlpha(Color{225, 120, 120, 255}, 220), 0.1f);
    std::string message = jf::tr("ui.warehouse.discard_confirm", gLanguage == Language::Japanese,
                                 {{"name", target.displayName}, {"quantity", std::to_string(target.quantity)}});
    drawText(message, static_cast<int>(confirmPanel.x + 14), static_cast<int>(confirmPanel.y + 14), 14,
             kColorTextPrimary);
    Rectangle confirmBtn{confirmPanel.x + 14, confirmPanel.y + 50, 150, 40};
    Rectangle cancelBtn{confirmPanel.x + 14 + 150 + 16, confirmPanel.y + 50, 150, 40};
    if (button(confirmBtn, tr("ui.button.confirm"), mouse, clicked)) {
        bool discarded = false;
        switch (target.kind) {
            case WarehouseDiscardKind::Material:
                discarded = app.discardStorage(target.materialId, target.quantity);
                break;
            case WarehouseDiscardKind::Item:
                discarded = app.discardItemStorage(target.itemType, target.quantity);
                break;
            case WarehouseDiscardKind::Overflow:
                discarded = app.discardOverflowStack(target.overflowIndex, target.quantity);
                break;
        }
        (void)discarded;  // Success or failure, the confirm panel closes either way;
                          // the list itself only reflects an entry's removal when
                          // the underlying discard actually reduced its quantity.
        gWarehouseDiscardConfirm.reset();
    }
    if (button(cancelBtn, tr("ui.button.cancel"), mouse, clicked)) gWarehouseDiscardConfirm.reset();
    y = confirmPanel.y + confirmPanel.height + 16.0f;
}

// The storage/consumables/pending-overflow discard lists. Split out of
// drawWarehouseCleanupOverlay(); the three sections share the local drawRow
// lambda and the running `y` cursor so they stay together in one function.
// No behavior change.
void drawWarehouseItemLists(jf::GameApp& app, Vector2 mouse, bool clicked, const Rectangle& panel, float& y) {
    auto drawRow = [&](const std::string& name, int quantity, WarehouseDiscardTarget target) {
        drawText(name + "  x" + std::to_string(quantity), static_cast<int>(panel.x + 26), static_cast<int>(y) + 6, 14,
                 kColorTextPrimary);
        Rectangle discardBtn{panel.x + panel.width - 26 - 110, y, 110, 32};
        target.displayName = name;
        if (button(discardBtn, tr("ui.button.discard"), mouse, clicked)) gWarehouseDiscardConfirm = target;
        y += 38.0f;
    };

    drawText(tr("ui.warehouse.storage_section"), static_cast<int>(panel.x + 26), static_cast<int>(y), 16,
             kColorAccentGold);
    y += 26.0f;
    bool anyStorage = false;
    for (const jf::LootStack& stack : app.baseState().storage) {
        if (app.baseState().materialStorageCap(stack.id) == 1) continue;  // key materials: never discardable
        anyStorage = true;
        WarehouseDiscardTarget target;
        target.kind = WarehouseDiscardKind::Material;
        target.materialId = stack.id;
        target.quantity = stack.quantity;
        drawRow(materialNameFor(stack.id), stack.quantity, target);
    }
    if (!anyStorage) {
        drawText(tr("ui.warehouse.empty"), static_cast<int>(panel.x + 26), static_cast<int>(y), 13, kColorTextMuted);
        y += 26.0f;
    }

    y += 12.0f;
    drawText(tr("ui.warehouse.consumables_section"), static_cast<int>(panel.x + 26), static_cast<int>(y), 16,
             kColorAccentGold);
    y += 26.0f;
    bool anyItems = false;
    for (const jf::ItemDefinition& item : jf::kItemCatalog) {
        const int owned = app.baseState().ownedItemCount(item.type);
        if (owned <= 0) continue;
        anyItems = true;
        WarehouseDiscardTarget target;
        target.kind = WarehouseDiscardKind::Item;
        target.itemType = item.type;
        target.quantity = owned;
        drawRow(itemFullNameFor(item.type), owned, target);
    }
    if (!anyItems) {
        drawText(tr("ui.warehouse.empty"), static_cast<int>(panel.x + 26), static_cast<int>(y), 13, kColorTextMuted);
        y += 26.0f;
    }

    y += 12.0f;
    drawText(tr("ui.warehouse.pending_section"), static_cast<int>(panel.x + 26), static_cast<int>(y), 16,
             kColorAccentGold);
    y += 26.0f;
    const auto& overflowStacks = app.rewardOverflow().stacks;
    if (overflowStacks.empty()) {
        drawText(tr("ui.warehouse.empty"), static_cast<int>(panel.x + 26), static_cast<int>(y), 13, kColorTextMuted);
        y += 26.0f;
    }
    for (std::size_t i = 0; i < overflowStacks.size(); ++i) {
        const jf::OverflowStack& stack = overflowStacks[i];
        // "item:<int>" prefix (see OverflowStack::itemId's doc comment in
        // BaseState.hpp) distinguishes a consumable overflow entry from a
        // material one sharing the same display path.
        std::string name;
        if (stack.itemId.rfind("item:", 0) == 0) {
            const int rawType = std::stoi(stack.itemId.substr(5));
            name = itemFullNameFor(static_cast<jf::ItemType>(rawType));
        } else {
            name = materialNameFor(stack.itemId);
        }
        WarehouseDiscardTarget target;
        target.kind = WarehouseDiscardKind::Overflow;
        target.overflowIndex = i;
        target.quantity = stack.quantity;
        drawRow(name, stack.quantity, target);
    }
}

// docs/inventory_overflow.md「倉庫整理画面」: lists storage/consumables/
// pending-overflow and lets the player discard consumables and ordinary
// materials (never equipped gear, key materials, or Discoveries - key
// materials are refused by GameApp::discardStorage() itself). A single
// explicit Confirm button per docs (no long-press, no two-step beyond the
// one confirmation panel); the target list only drops an entry once the
// discard call actually reports success.
void drawWarehouseCleanupOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    if (!gWarehouseCleanupOpen) return;

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 170});
    Rectangle panel{static_cast<float>(kScreenWidth) / 2.0f - 280.0f, 60.0f, 560.0f, 680.0f};
    drawCard(panel, kColorCard, withAlpha(kColorAccentGold, 230), 0.08f);

    drawText(tr("ui.warehouse.title"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 20), 24,
             kColorTextPrimary);

    const bool blockedByOverflow = app.rewardOverflow().stacks.size() >= jf::RewardOverflowState::kMaxStacks;
    float y = panel.y + 60.0f;
    if (blockedByOverflow) {
        drawText(tr("ui.warehouse.overflow_blocked"), static_cast<int>(panel.x + 26), static_cast<int>(y), 13,
                 Color{225, 120, 120, 255});
        y += 28.0f;
    }

    drawWarehouseDiscardConfirm(app, mouse, clicked, panel, y);
    drawWarehouseItemLists(app, mouse, clicked, panel, y);

    Rectangle closeBtn{panel.x + panel.width - 26 - 120, panel.y + panel.height - 50, 120, 38};
    if (button(closeBtn, tr("ui.button.close"), mouse, clicked)) {
        gWarehouseCleanupOpen = false;
        gWarehouseDiscardConfirm.reset();
    }

    Rectangle retryBtn{panel.x + 26, panel.y + panel.height - 50, 240, 38};
    if (button(retryBtn, tr("ui.warehouse.try_return_again"), mouse, clicked)) {
        if (app.returnToBase()) gWarehouseCleanupOpen = false;
    }
}

// The language picker (English/Japanese) row. Split out of
// drawSettingsOverlay(); no behavior change.
void drawSettingsLanguageSection(Vector2 mouse, bool clicked, const Rectangle& panel) {
    drawText(tr("ui.settings.language"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 66), 15,
             kColorTextMuted);

    Rectangle enBtn{panel.x + 26, panel.y + 96, 150, 48};
    Rectangle jaBtn{panel.x + 26 + 150 + 16, panel.y + 96, 150, 48};

    if (button(enBtn, "English", "", mouse, clicked)) gLanguage = Language::English;
    if (button(jaBtn, kJaJapaneseNative, "", mouse, clicked)) gLanguage = Language::Japanese;

    Rectangle activeHighlight = gLanguage == Language::English ? enBtn : jaBtn;
    DrawRectangleRoundedLinesEx(activeHighlight, 0.28f, 8, 3.0f, withAlpha(kColorAccentGold, 255));
}

// The window maximize/restore row. Split out of drawSettingsOverlay(); no
// behavior change.
void drawSettingsWindowSection(Vector2 mouse, bool clicked, const Rectangle& panel) {
    drawText(tr("ui.settings.window"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 158), 15,
             kColorTextMuted);
    bool maximized = IsWindowState(FLAG_WINDOW_MAXIMIZED);
    Rectangle windowBtn{panel.x + 26, panel.y + 188, 328, 46};
    if (button(windowBtn, tr(maximized ? "ui.settings.restore_window" : "ui.settings.maximize_window"), mouse,
              clicked)) {
        if (maximized) RestoreWindow();
        else MaximizeWindow();
    }
    drawText(tr("ui.settings.maximize_shortcut"), static_cast<int>(panel.x + 26),
             static_cast<int>(panel.y + 238), 12, kColorTextFaint);
}

// The retire-expedition row. Split out of drawSettingsOverlay(); no behavior
// change.
void drawSettingsExpeditionSection(jf::GameApp& app, Vector2 mouse, bool clicked, const Rectangle& panel) {
    drawText(tr("ui.settings.expedition_section"), static_cast<int>(panel.x + 26),
             static_cast<int>(panel.y + 266), 15, kColorTextMuted);
    Rectangle retireBtn{panel.x + 26, panel.y + 296, 328, 46};
    bool canRetire = app.screen() != jf::Screen::Base;
    if (canRetire) {
        if (button(retireBtn, tr("ui.settings.retire_expedition"), mouse, clicked)) {
            if (app.retireExpedition()) gSettingsOpen = false;
        }
    } else {
        disabledButton(retireBtn, tr("ui.settings.retire_expedition"));
    }
    drawText(tr("ui.settings.retire_expedition_note"),
             static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 346), 12, kColorTextFaint);
}

// The save-data export/import section, plus the transient save-status
// message. Split out of drawSettingsOverlay(); no behavior change.
void drawSettingsSaveDataSection(jf::GameApp& app, Vector2 mouse, bool clicked, const Rectangle& panel) {
    drawText(tr("ui.settings.save_data_section"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 380),
              15, kColorTextMuted);

    Rectangle exportBtn{panel.x + 26, panel.y + 408, 150, 46};
    Rectangle importBtn{panel.x + 26 + 150 + 16, panel.y + 408, 150, 46};

    if (button(exportBtn, tr("ui.settings.export"), mouse, clicked)) {
        const std::string language = gLanguage == Language::Japanese ? "ja" : "en";
        std::string exportError;
        std::string exportedPath = jf::exportSaveData(app.createSaveData(language), &exportError);
        if (!exportedPath.empty())
            setSaveStatus(jf::tr("ui.settings.export_ok_prefix", false) + exportedPath,
                          jf::tr("ui.settings.export_ok_prefix", true) + exportedPath);
        else
            setSaveStatus(jf::tr("ui.settings.export_failed_prefix", false) + exportError,
                          jf::tr("ui.settings.export_failed_prefix", true) + exportError);
    }

    const bool canImport = app.screen() == jf::Screen::Base;
    if (gPendingImport) {
        drawText(clipTextToWidth(gPendingImportFilename + " - " + tr("ui.settings.import_confirm"), 13,
                                  328),
                 static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 462), 13, kColorTextPrimary);
        Rectangle applyBtn{panel.x + 26, panel.y + 488, 150, 44};
        Rectangle cancelBtn{panel.x + 26 + 150 + 16, panel.y + 488, 150, 44};
        if (button(applyBtn, tr("ui.settings.import_apply"), mouse, clicked)) {
            std::string importError;
            if (gSaveStore && gSaveStore->importFrom(*gPendingImport, &importError) && app.applySaveData(*gPendingImport)) {
                gLanguage = gPendingImport->language == "ja" ? Language::Japanese : Language::English;
                gAutoSaveEnabled = true;
                setSaveStatus(jf::tr("ui.settings.import_applied", false), jf::tr("ui.settings.import_applied", true));
            } else {
                setSaveStatus(jf::tr("ui.settings.import_failed_prefix", false) + importError,
                              jf::tr("ui.settings.import_failed_prefix", true) + importError);
            }
            gPendingImport.reset();
            gPendingImportFilename.clear();
        }
        if (button(cancelBtn, tr("ui.button.cancel"), mouse, clicked)) {
            gPendingImport.reset();
            gPendingImportFilename.clear();
        }
    } else if (canImport) {
        if (button(importBtn, tr("ui.settings.import"), mouse, clicked)) {
            auto candidates = jf::listImportCandidates();
            if (candidates.empty()) {
                setSaveStatus(jf::tr("ui.settings.import_no_file", false), jf::tr("ui.settings.import_no_file", true));
            } else {
                std::string loadError;
                if (auto data = jf::loadImportCandidate(candidates.front().path, &loadError)) {
                    gPendingImport = data;
                    gPendingImportFilename = candidates.front().filename;
                } else {
                    setSaveStatus(jf::tr("ui.settings.import_failed_prefix", false) + loadError,
                                  jf::tr("ui.settings.import_failed_prefix", true) + loadError);
                }
            }
        }
    } else {
        disabledButton(importBtn, tr("ui.settings.import"));
        drawText(tr("ui.settings.import_base_only"), static_cast<int>(panel.x + 26),
                 static_cast<int>(panel.y + 462), 12, kColorTextFaint);
    }

    if (!gSaveStatusMessage.empty() && GetTime() < gSaveStatusExpiresAt) {
        drawText(clipTextToWidth(pick(gSaveStatusMessage, gSaveStatusMessageJa), 12, 328),
                 static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 544), 12, kColorAccentGold);
    }
}

// Small always-on-top corner button + modal for switching the display
// language. Purely a rendering/UI concern (see the Language enum above),
// so it lives entirely in this file and never touches GameApp/BattleState.
// Draw this last on every screen so it sits above any other overlay.
void drawSettingsOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    Rectangle cornerBtn{static_cast<float>(kScreenWidth) - 100.0f, 4.0f, 92.0f, 32.0f};
    if (button(cornerBtn, tr("ui.settings.title"), mouse, gSettingsOpen ? false : clicked)) {
        gSettingsOpen = !gSettingsOpen;
    }

    if (!gSettingsOpen) return;

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 150});

    Rectangle panel{static_cast<float>(kScreenWidth) / 2.0f - 190.0f, static_cast<float>(kScreenHeight) / 2.0f - 320.0f,
                    380.0f, 640.0f};
    drawCard(panel, kColorCard, withAlpha(kColorAccentGold, 230), 0.1f);

    drawText(tr("ui.settings.title"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 22), 24,
             kColorTextPrimary);

    drawSettingsLanguageSection(mouse, clicked, panel);
    drawSettingsWindowSection(mouse, clicked, panel);
    drawSettingsExpeditionSection(app, mouse, clicked, panel);
    drawSettingsSaveDataSection(app, mouse, clicked, panel);

    Rectangle closeBtn{panel.x + 26, panel.y + 580, 328, 40};
    if (button(closeBtn, tr("ui.button.close"), mouse, clicked)) gSettingsOpen = false;
}

// docs/save_system.md「保存状態HUD」: performs one save attempt and advances
// the Idle/Saving/Saved/Failed state machine. Desktop I/O is synchronous, so
// callers see the outcome (Saved or Failed) immediately - `Saving` never
// actually gets drawn on its own frame here, but the state still exists so
// the transition shape matches the spec (and so a future async Web backend
// slots in without changing this function's contract).
void attemptAutoSave(jf::GameApp& app, std::uint64_t& savedRevision, Language& savedLanguage,
                     std::uint64_t& savedExpeditionRevision) {
    const std::string language = gLanguage == Language::Japanese ? "ja" : "en";
    std::string saveError;
    if (gSaveStore->save(app.createSaveData(language), &saveError)) {
        savedRevision = app.persistentRevision();
        savedLanguage = gLanguage;
        savedExpeditionRevision = app.expeditionRevision();
        gSaveHudState = SaveHudState::Saved;
        gSaveHudSavedUntil = GetTime() + 1.5;
        gSaveHudRetryCount = 0;
    } else {
        gSaveHudFailReason = saveError;
        gSaveHudState = SaveHudState::Failed;
        if (gSaveHudRetryCount < kSaveHudMaxAutoRetries) {
            gSaveHudNextRetryAt = GetTime() + kSaveHudRetryDelays[gSaveHudRetryCount];
            ++gSaveHudRetryCount;
        }
    }
}

// Small fixed-corner status readout (docs/save_system.md「保存状態HUD」),
// placed clear of the battle command HUD / Facilities / Settings buttons.
// Manual "Retry" stays available even after the 3 automatic retries are
// exhausted (spec: "手動の「再試行」は回数制限後も利用可能にする").
void drawSaveStatusHud(jf::GameApp& app, Vector2 mouse, bool clicked, std::uint64_t& savedRevision,
                      Language& savedLanguage, std::uint64_t& savedExpeditionRevision) {
    if (gSaveHudState == SaveHudState::Idle) return;
    if (gSaveHudState == SaveHudState::Saved && GetTime() >= gSaveHudSavedUntil) {
        gSaveHudState = SaveHudState::Idle;
        return;
    }
    constexpr float kHudW = 220.0f;
    constexpr float kHudH = 40.0f;
    Rectangle hud{static_cast<float>(kScreenWidth) - kHudW - 12.0f, static_cast<float>(kScreenHeight) - kHudH - 12.0f,
                 kHudW, kHudH};
    Color border = gSaveHudState == SaveHudState::Failed ? Color{225, 120, 120, 255} : kColorAccentGold;
    drawCard(hud, kColorCard, withAlpha(border, 220), 0.2f);
    std::string label = gSaveHudState == SaveHudState::Saving   ? tr("ui.save_hud.saving")
                        : gSaveHudState == SaveHudState::Saved  ? tr("ui.save_hud.saved")
                                                                 : tr("ui.save_hud.failed");
    drawText(label, static_cast<int>(hud.x + 12), static_cast<int>(hud.y + 10), 14,
             gSaveHudState == SaveHudState::Failed ? Color{225, 120, 120, 255} : kColorTextPrimary);
    if (gSaveHudState == SaveHudState::Failed) {
        Rectangle retryBtn{hud.x + kHudW - 84.0f, hud.y + 4.0f, 76.0f, kHudH - 8.0f};
        if (button(retryBtn, tr("ui.save_hud.retry"), mouse, clicked))
            attemptAutoSave(app, savedRevision, savedLanguage, savedExpeditionRevision);
    }
}

// docs/save_system.md「破損復旧画面」: shown instead of the normal screen
// dispatch when startup couldn't read any usable save (see main()). All 3
// choices end by clearing gSaveRecoveryOpen and re-enabling autosave, since
// each one leaves the app in a definite, savable state (recovered data,
// imported data, or a deliberate fresh start).
void drawSaveRecoveryScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    drawText(tr("ui.recovery.title"), 40, 40, 28, kColorAccentGold);
    drawText(gSaveRecoveryMessage.empty() ? tr("ui.recovery.message") : gSaveRecoveryMessage, 40, 90, 16,
             kColorTextPrimary);

    if (gSaveRecoveryStartNewConfirm) {
        drawText(tr("ui.recovery.start_new_confirm"), 40, 140, 15, Color{225, 120, 120, 255});
        Rectangle confirmBtn{40, 180, 200, 44};
        Rectangle cancelBtn{256, 180, 200, 44};
        if (button(confirmBtn, tr("ui.button.confirm"), mouse, clicked)) {
            gSaveStore->quarantineCorruptSave();
            gSaveRecoveryOpen = false;
            gSaveRecoveryStartNewConfirm = false;
            gAutoSaveEnabled = true;
        }
        if (button(cancelBtn, tr("ui.button.cancel"), mouse, clicked)) gSaveRecoveryStartNewConfirm = false;
        return;
    }

    Rectangle restoreBtn{40, 140, 260, 48};
    Rectangle importBtn{40, 200, 260, 48};
    Rectangle startNewBtn{40, 260, 260, 48};
    if (button(restoreBtn, tr("ui.recovery.restore_backup"), mouse, clicked)) {
        std::string restoreError;
        if (gSaveStore->restoreFromBackup(&restoreError)) {
            if (auto restored = gSaveStore->load()) {
                if (app.applySaveData(*restored))
                    gLanguage = restored->language == "ja" ? Language::Japanese : Language::English;
            }
            gSaveRecoveryOpen = false;
            gAutoSaveEnabled = true;
        } else {
            gSaveRecoveryMessage = tr("ui.recovery.restore_failed");
        }
    }
    if (button(importBtn, tr("ui.recovery.import_save"), mouse, clicked)) {
        gSaveRecoveryOpen = false;
        gAutoSaveEnabled = true;
        gSettingsOpen = true;  // Base screen's Import flow (already unrestricted
                               // at startup, since the app always begins at
                               // Screen::Base) takes it from here.
    }
    if (button(startNewBtn, tr("ui.recovery.start_new"), mouse, clicked)) gSaveRecoveryStartNewConfirm = true;
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
        case jf::BattleInputState::SelectInteractTarget:
            controller.selectInteractTarget(pos);
            break;
        default:
            break;
    }
}

// Locales must load before loadAppFont() (it reads jf::allJapaneseGlyphText()
// for the Glyph Atlas) and before any tr() call. If even this fails, there's
// no Locale table to draw a translated message from, so this one error
// screen stays plain English rather than routing through tr(). Returns false
// (having already run the blocking error screen to WindowShouldClose()) iff
// startup should abort here.
bool loadLocalesAndFontOrShowError() {
    bool localesLoaded = jf::loadLocales("data");
    if (!localesLoaded) localesLoaded = jf::loadLocales("../data");
    if (!localesLoaded) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Failed to load locale files from data/locales/.", 20, 20, 18, RED);
            EndDrawing();
        }
        return false;
    }
    loadAppFont();
    return true;
}

// Same "blocking error screen, empty return means abort" shape as
// loadLocalesAndFontOrShowError() above, for the data/ load that follows it.
std::optional<jf::GameData> loadGameDataOrShowError() {
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
            drawText(tr("ui.error.data_load_failed"), 20, 20, 18, RED);
            EndDrawing();
        }
    }
    return gameData;
}

// One frame's worth of screen dispatch: which top-level screen owns the
// frame, plus the overlays (Settings, Save HUD) every screen draws on top.
// `sceneClicked` is `clicked` gated on the Settings modal not owning input;
// see main()'s own comment on why the two are different.
void drawCurrentScreen(jf::GameApp& app, Vector2 mouse, float dt, bool clicked, bool sceneClicked,
                       std::uint64_t savedRevision, Language savedLanguage, std::uint64_t savedExpeditionRevision) {
    if (gSaveRecoveryOpen) {
        beginLogicalFrame();
        drawSaveRecoveryScreen(app, mouse, clicked);
        endLogicalFrame();
    } else if (app.screen() == jf::Screen::Battle) {
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
            controller.inputState() != jf::BattleInputState::ConfirmSkillAttack &&
            controller.inputState() != jf::BattleInputState::ConfirmObjectAttack &&
            controller.inputState() != jf::BattleInputState::Victory &&
            controller.inputState() != jf::BattleInputState::Defeat) {
            drawHoverInfo(app.gameData(), controller, mouse);
        }

        if (controller.inputState() == jf::BattleInputState::ConfirmAttack) {
            if (auto preview = controller.pendingPreview()) {
                drawCombatPreviewPopup(*preview);
            }
        } else if (controller.inputState() == jf::BattleInputState::ConfirmSkillAttack) {
            // M4 item 3: same popup, fed by the attack-shape skill's own
            // preview (already folds in any flat bonus damage).
            if (auto preview = controller.pendingSkillPreview()) {
                drawCombatPreviewPopup(*preview);
            }
        } else if (controller.inputState() == jf::BattleInputState::ConfirmObjectAttack) {
            if (auto preview = controller.pendingObjectPreview()) {
                drawObjectAttackPreviewPopup(*preview);
            }
        }

        if (controller.inputState() == jf::BattleInputState::Victory) {
            drawVictoryOverlay(app, mouse, sceneClicked);
        } else if (controller.inputState() == jf::BattleInputState::Defeat) {
            drawDefeatOverlay(app, mouse, sceneClicked);
        }
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    } else if (app.screen() == jf::Screen::Camp) {
        beginLogicalFrame();
        drawCampScreen(app, mouse, sceneClicked);
        drawWarehouseCleanupOverlay(app, mouse, sceneClicked);
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    } else if (app.screen() == jf::Screen::Exploration) {
        beginLogicalFrame();
        drawExplorationScreen(app, mouse, sceneClicked);
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    } else if (app.screen() == jf::Screen::PreBattleDeployment) {
        beginLogicalFrame();
        drawPreBattleDeploymentScreen(app, mouse, sceneClicked);
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    } else {
        beginLogicalFrame();
        if (gShowFacilities) drawFacilitiesScreen(app, mouse, sceneClicked);
        else if (gViewedUnitId) drawUnitScreen(app, mouse, sceneClicked);
        else drawBaseScreen(app, mouse, sceneClicked);
        drawWarehouseCleanupOverlay(app, mouse, sceneClicked);
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    }
}

// docs/save_system.md「保存状態HUD」: autosaves once persistent/expedition
// state or language actually changed since the last save, plus up to 3
// automatic retries (at 0.5/1/2s) after a failed attempt.
void runAutoSaveTick(jf::GameApp& app, std::uint64_t& savedRevision, Language& savedLanguage,
                     std::uint64_t& savedExpeditionRevision) {
    if (!gAutoSaveEnabled) return;
    const bool isDirty = savedRevision != app.persistentRevision() || savedLanguage != gLanguage ||
                        savedExpeditionRevision != app.expeditionRevision();
    if (isDirty) {
        attemptAutoSave(app, savedRevision, savedLanguage, savedExpeditionRevision);
    } else if (gSaveHudState == SaveHudState::Failed && gSaveHudRetryCount < kSaveHudMaxAutoRetries &&
              GetTime() >= gSaveHudNextRetryAt) {
        // docs/save_system.md「保存状態HUD」: up to 3 automatic retries at
        // 0.5/1/2s: attemptAutoSave() re-serializes current state each
        // time, so this always saves the latest data, not a stale snapshot
        // from the original failed attempt.
        attemptAutoSave(app, savedRevision, savedLanguage, savedExpeditionRevision);
    }
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(kScreenWidth, kScreenHeight, "JOJIFrontier");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    if (!loadLocalesAndFontOrShowError()) {
        CloseWindow();
        return 1;
    }

    auto gameData = loadGameDataOrShowError();
    if (!gameData) {
        CloseWindow();
        return 1;
    }

    jf::GameApp app(*gameData);
    gSaveStore.emplace(jf::defaultSavePath());
    std::string saveLoadError;
    auto loadedSave = gSaveStore->load(&saveLoadError);
    if (loadedSave) {
        if (app.applySaveData(*loadedSave))
            gLanguage = loadedSave->language == "ja" ? Language::Japanese : Language::English;
    }
    // A corrupt or unsupported save is never overwritten automatically.
    // `saveLoadError` is non-empty here only when SaveStore::load() truly
    // found nothing usable (its own primary-then-".bak" fallback already
    // failed both) - see load()'s doc comment. That's the docs/save_system.md
    // 「破損復旧画面」trigger; a fresh install with no save file at all
    // leaves saveLoadError empty and proceeds normally.
    gSaveRecoveryOpen = !loadedSave && !saveLoadError.empty();
    gAutoSaveEnabled = !gSaveRecoveryOpen;
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
        drawCurrentScreen(app, mouse, dt, clicked, sceneClicked, savedRevision, savedLanguage,
                          savedExpeditionRevision);
        runAutoSaveTick(app, savedRevision, savedLanguage, savedExpeditionRevision);
    }

    if (gFontReady) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
