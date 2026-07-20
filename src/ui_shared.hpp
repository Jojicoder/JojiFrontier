// Declarations for the UI infrastructure defined in main.cpp (layout
// constants, palette, text/button drawing, and Locale Key-backed name
// lookups) that other main.cpp UI translation units (e.g. ui_battle.cpp)
// need. The definitions stay in main.cpp's `jfui` namespace; this header
// exists purely so a second .cpp can link against them. No behavior change.
#pragma once

#include <raylib.h>

#include <string>
#include <unordered_map>
#include <vector>

#include <optional>

#include "jf/core/Grid.hpp"
#include "jf/data/GameData.hpp"

namespace jfui {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 800;

constexpr float kGridOriginX = 32.0f;
constexpr float kTileW = 152.0f;
constexpr float kTileH = 58.0f;
constexpr float kGridOriginY = 220.0f;
constexpr float kRowStep = 198.0f;
constexpr float kUnitRadius = 40.0f;
constexpr float kHudY = 704.0f;

extern const Color kColorCard;
extern const Color kColorCardAlt;
extern const Color kColorBorder;
extern const Color kColorBorderSoft;
extern const Color kColorShadow;
extern const Color kColorAccentGold;
extern const Color kColorTextPrimary;
extern const Color kColorTextMuted;
extern const Color kColorTextFaint;

enum class Language { English, Japanese };
extern Language gLanguage;

// A language picker's native-script label for switching TO Japanese
// ("日本語") is not itself translated (same reasoning as the "English"
// literal beside it), so it stays a plain constant rather than a Locale Key.
extern const std::string kJaJapaneseNative;

// docs/inventory_overflow.md「倉庫整理画面」: opened from multiple screens
// (Camp, Base), so its state lives here rather than with any one screen's
// file-local globals.
extern bool gWarehouseCleanupOpen;

// Base screen state (docs/implementation_roadmap.md "Phase 3.5"), also
// touched by the not-yet-split Facilities/Unit screens and main.cpp's own
// screen dispatcher - kept as an extern struct rather than moved into any
// one screen's file-local globals, since main.cpp's dispatcher and
// ui_facilities.cpp both need direct access and no single screen owns all
// of it yet.
struct BaseScreenState {
    bool showFacilities = false;
    jf::RegionId selectedRegionId = jf::RegionId::AshboughForest;
    std::optional<jf::FacilityId> visitedFacility;
    std::optional<jf::UnitClass> forgeCraftClass;
    std::optional<std::string> viewedUnitId;
};
extern BaseScreenState gBaseScreen;

Rectangle logicalViewport();
Vector2 logicalMousePosition();
void beginLogicalFrame();
void endLogicalFrame();

void loadAppFont();
void unloadAppFont();

Rectangle tileRect(jf::GridPos pos);
Color scaleColor(Color color, float factor);
Color withAlpha(Color color, unsigned char alpha);
void drawCard(Rectangle rect, Color fill, Color border, float roundness = 0.12f);
Color terrainColor(jf::TerrainType terrain);
void drawBattlefieldBackdrop();
void drawTilePanel(Rectangle rect, Color frameColor);

std::string pick(const std::string& en, const std::string& ja);
std::string tr(const std::string& key);
std::string tr(const std::string& key, const std::unordered_map<std::string, std::string>& args);

int displayFontSize(int requestedSize);
void drawText(const std::string& text, int x, int y, int fontSize, Color color);
int textWidth(const std::string& text, int fontSize);
std::string clipTextToWidth(const std::string& text, int fontSize, int maxWidth);
std::string wrapTextToWidth(const std::string& text, int fontSize, int maxWidth);
std::vector<std::string> textLines(const std::string& text);
float textLineHeight(int fontSize);

bool tileFromScreen(Vector2 mouse, jf::GridPos& out);
Color teamColor(jf::Team team);

std::string classNameFor(const jf::GameData& data, jf::UnitClass unitClass);
std::string classRoleFor(const jf::GameData& data, jf::UnitClass unitClass);
std::string itemFullNameFor(jf::ItemType type);
std::string itemDescriptionFor(jf::ItemType type);
std::string materialNameFor(const std::string& id);
std::string weaponNameFor(const std::string& weaponId, const std::string& englishName);
std::string terrainNameFor(jf::TerrainType terrain);
std::string battleObjectNameForKind(jf::BattleObjectKind kind);
std::string battleObjectNameFor(const jf::BattleObjectDefinition& definition);
std::string unitDisplayNameFor(const std::string& englishName);
std::string outpostStageNameFor(jf::OutpostStage stage);
std::string outpostStageShortNameFor(jf::OutpostStage stage);
std::string discoveryNameFor(const jf::DiscoveryId& id);

// Status-effect UI (docs/status_effects.md "UI"): one badge per currently
// active effect, in the doc's fixed display order.
struct StatusBadge {
    std::string glyph;
    std::string label;
    std::string detail;
    Color color;
};
std::vector<StatusBadge> activeStatusBadges(const jf::Unit& unit);

bool button(Rectangle rect, const std::string& labelEn, const std::string& labelJa, Vector2 mouse,
            bool mousePressed);
bool button(Rectangle rect, const std::string& label, Vector2 mouse, bool mousePressed);
void disabledButton(Rectangle rect, const std::string& label);

// Generic hover-tooltip line, shared by every screen's info popups.
struct TooltipLine {
    std::string text;
    Color color;
    int fontSize;
};
void drawTooltipBox(Vector2 mouse, const std::vector<TooltipLine>& lines);

void drawSectionHeading(const std::string& text, int x, int y, int fontSize);

}  // namespace jfui
