// Shared UI infrastructure - layout constants, palette, the logical-frame
// viewport, the Japanese-capable font, text/button drawing primitives, and
// Locale Key-backed name lookups - used by every screen's own .cpp file via
// ui_shared.hpp. Split out of main.cpp; no behavior change.
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/core/GameApp.hpp"
#include "jf/core/Locale.hpp"
#include "jf/data/GameData.hpp"
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
extern const std::string kJaJapaneseNative{"日本語"};

// --- Language setting -------------------------------------------------
// Purely a display concern (like the font/colors above), not part of
// battle rules, so it lives here rather than in GameApp/BattleController.
Language gLanguage = Language::English;

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
                              jf::UnitClass::HeavyInfantry, jf::UnitClass::FrontierEngineer,
                              jf::UnitClass::MessengerCavalry, jf::UnitClass::FrontierRanger,
                              jf::UnitClass::BannerBearer,
                              jf::UnitClass::Bandit, jf::UnitClass::Wolf,
                              jf::UnitClass::AshenhornBoar}) {
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
                           "watch_ledger", "captains_seal", jf::kAshveilFangMaterial,
                           jf::kAshenhornFangMaterial, "quality_herb", "ashenhorn_fragment", "iron", "stone",
                           "old_gear", "signal_core", "quality_iron"})
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

void unloadAppFont() {
    if (gFontReady) UnloadFont(gFont);
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
        "wood", "hide", "herb", "gate_tools", "ash_road_map", "field_medicine", "watch_ledger",
        "captains_seal", jf::kAshveilFangMaterial, jf::kAshenhornFangMaterial, "quality_herb", "ashenhorn_fragment",
        "iron", "stone", "old_gear", "signal_core", "quality_iron",
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
        "iron_greathammer", "engineer_hammer", "messenger_sword", "hunting_bow", "banner_spear",
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

// Small gold accent bar preceding a section heading, matching the same
// "gold picks out something the player should notice" language used by
// the battle HUD's step label and combat preview title.
void drawSectionHeading(const std::string& text, int x, int y, int fontSize) {
    DrawRectangleRounded(Rectangle{static_cast<float>(x) - 14.0f, static_cast<float>(y) + 3.0f, 5.0f,
                                   static_cast<float>(fontSize) - 4.0f},
                         0.5f, 4, kColorAccentGold);
    drawText(text, x, y, fontSize, kColorTextPrimary);
}

}  // namespace jfui
