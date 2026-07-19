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
#include "ui_base.hpp"
#include "ui_battle.hpp"
#include "ui_camp.hpp"
#include "ui_deployment.hpp"
#include "ui_exploration.hpp"
#include "ui_facilities.hpp"
#include "ui_shared.hpp"

namespace jfui {

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
extern const Color kColorCard{23, 27, 38, 250};
extern const Color kColorCardAlt{28, 33, 46, 250};
extern const Color kColorBorder{108, 122, 145, 210};
extern const Color kColorBorderSoft{72, 82, 100, 150};
extern const Color kColorShadow{0, 0, 0, 90};
extern const Color kColorAccentGold{224, 190, 120, 255};
extern const Color kColorTextPrimary{238, 240, 245, 255};
extern const Color kColorTextMuted{176, 186, 200, 255};
extern const Color kColorTextFaint{140, 150, 165, 255};

constexpr Color kFloorPanelColor{88, 98, 112, 255};

// Rounded card with a soft drop shadow and a hairline border - the base
// look reused by popups, tooltips, and HUD chrome throughout.
void drawCard(Rectangle rect, Color fill, Color border, float roundness) {
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
// active effect, in the doc's fixed display order.
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

// Small gold accent bar preceding a section heading, matching the same
// "gold picks out something the player should notice" language used by
// the battle HUD's step label and combat preview title.
void drawSectionHeading(const std::string& text, int x, int y, int fontSize) {
    DrawRectangleRounded(Rectangle{static_cast<float>(x) - 14.0f, static_cast<float>(y) + 3.0f, 5.0f,
                                   static_cast<float>(fontSize) - 4.0f},
                         0.5f, 4, kColorAccentGold);
    drawText(text, x, y, fontSize, kColorTextPrimary);
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

} // namespace jfui

using namespace jfui;

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
