#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/core/GameApp.hpp"
#include "jf/core/Grid.hpp"
#include "jf/data/GameData.hpp"

namespace {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 800;

// --- Fixed oblique / pseudo-isometric battlefield layout -----------------
// The camera never moves (per the design doc), but rows are drawn as a
// trapezoid receding away from the player: farther rows (lower index) sit
// higher on screen, shrink, and shear sideways, giving a Mega Man Battle
// Network-style side-facing floor instead of a flat top-down grid. Columns
// stay a simple left-to-right axis within each row.
constexpr float kBaseTileW = 92.0f;  // front-row (nearest) tile width
constexpr float kBaseTileH = 92.0f;  // front-row tile height
constexpr float kRowScale = 0.78f;   // shrink factor applied per row moving back
constexpr float kRowShearX = 36.0f;  // sideways shear applied per row moving back
constexpr float kGridCenterX = 410.0f;
constexpr float kFrontBaselineY = 740.0f; // bottom edge of the nearest row

constexpr float kFrontRowWidth = jf::kGridCols * kBaseTileW;
constexpr float kFrontRowRightX = kGridCenterX + kFrontRowWidth / 2.0f;

constexpr int kPanelX = static_cast<int>(kFrontRowRightX) + 40;
constexpr int kPanelWidth = kScreenWidth - kPanelX - 30;

struct RowLayout {
    float leftX;
    float topY;
    float tileW;
    float tileH;
    float scale;
};

// Row 0 is farthest from camera (top of screen, smallest); the last row is
// nearest (bottom, full size). Computed once and cached since the layout
// never changes at runtime.
const std::array<RowLayout, jf::kGridRows>& rowLayouts() {
    static const std::array<RowLayout, jf::kGridRows> layouts = [] {
        std::array<RowLayout, jf::kGridRows> result{};
        float bottomY = kFrontBaselineY;
        for (int r = jf::kGridRows - 1; r >= 0; --r) {
            int depth = jf::kGridRows - 1 - r; // 0 at front, increases toward the back
            float scale = std::pow(kRowScale, static_cast<float>(depth));
            float tileW = kBaseTileW * scale;
            float tileH = kBaseTileH * scale;
            float topY = bottomY - tileH;
            float shear = -static_cast<float>(depth) * kRowShearX;
            float leftX = kGridCenterX - (jf::kGridCols * tileW) / 2.0f + shear;
            result[r] = RowLayout{leftX, topY, tileW, tileH, scale};
            bottomY = topY;
        }
        return result;
    }();
    return layouts;
}

// --- Bilingual UI strings (English / Japanese) ---------------------------
const std::string kJaPlayerPhase = "プレイヤーフェイズ";
const std::string kJaEnemyPhase = "敵フェイズ";
const std::string kJaSelectUnit = "行動するユニットを選択してください。";
const std::string kJaAttack = "こうげき";
const std::string kJaWait = "待機";
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
    charsetSource += kJaPlayerPhase + kJaEnemyPhase + kJaSelectUnit + kJaAttack + kJaWait + kJaConfirm +
                      kJaCancel + kJaCombatPreview + kJaDamage + kJaVictory + kJaProceedToCamp +
                      kJaDefeatTitle + kJaLootLostLine + kJaReturnToBase + kJaCamp + kJaLootSecured +
                      kJaNoLootThisExpedition + kJaContinue + kJaPartyHp + kJaPendingLoot + kJaNoneYet +
                      kJaBattlesWon + kJaContinueExpedition + kJaDown + kJaHp + kJaWeapon + kJaStr + kJaMag +
                      kJaDef + kJaRes + kJaDataLoadFailed;
    for (jf::UnitClass uc : {jf::UnitClass::Lord, jf::UnitClass::ArmorKnight, jf::UnitClass::Archer,
                              jf::UnitClass::Mage, jf::UnitClass::Bandit, jf::UnitClass::Soldier}) {
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

Rectangle tileRect(jf::GridPos pos) {
    const RowLayout& rl = rowLayouts()[pos.row];
    return Rectangle{
        rl.leftX + pos.col * rl.tileW,
        rl.topY,
        rl.tileW - 3,
        rl.tileH - 3,
    };
}

bool tileFromScreen(Vector2 mouse, jf::GridPos& out) {
    const auto& layouts = rowLayouts();
    for (int row = 0; row < jf::kGridRows; ++row) {
        const RowLayout& rl = layouts[row];
        if (mouse.y < rl.topY || mouse.y >= rl.topY + rl.tileH) continue;
        int col = static_cast<int>((mouse.x - rl.leftX) / rl.tileW);
        if (col < 0 || col >= jf::kGridCols) continue;
        out = jf::GridPos{row, col};
        return true;
    }
    return false;
}

Color teamColor(jf::Team team) {
    return team == jf::Team::Player ? Color{60, 120, 220, 255} : Color{200, 60, 60, 255};
}

bool containsTile(const std::vector<jf::GridPos>& tiles, jf::GridPos pos) {
    for (const auto& t : tiles) {
        if (t == pos) return true;
    }
    return false;
}

bool button(Rectangle rect, const std::string& labelEn, const std::string& labelJa, Vector2 mouse,
            bool mousePressed) {
    bool hovered = CheckCollisionPointRec(mouse, rect);
    Color base = hovered ? Color{90, 90, 110, 255} : Color{60, 60, 75, 255};
    DrawRectangleRec(rect, base);
    DrawRectangleLinesEx(rect, 2, Color{200, 200, 210, 255});

    std::string label = labelJa.empty() ? labelEn : labelJa + " / " + labelEn;
    int fontSize = 16;
    int w = textWidth(label, fontSize);
    drawText(label, static_cast<int>(rect.x + (rect.width - w) / 2),
             static_cast<int>(rect.y + (rect.height - fontSize) / 2), fontSize, RAYWHITE);
    return hovered && mousePressed;
}

// Sky band above the battlefield plus a receding floor trapezoid beneath
// it, so the tile grid reads as an oblique/pseudo-isometric floor rather
// than floating rectangles. Fixed camera - this never rotates or pans.
void drawBattlefieldBackdrop() {
    const auto& layouts = rowLayouts();
    const RowLayout& back = layouts.front();
    const RowLayout& front = layouts.back();

    float backLeft = back.leftX;
    float backRight = back.leftX + jf::kGridCols * back.tileW;
    float backY = back.topY;
    float frontLeft = front.leftX;
    float frontRight = front.leftX + jf::kGridCols * front.tileW;
    float frontY = front.topY + front.tileH;

    DrawRectangle(0, 66, kScreenWidth, static_cast<int>(backY) - 66, Color{22, 26, 40, 255});

    Vector2 bl{backLeft, backY};
    Vector2 br{backRight, backY};
    Vector2 fl{frontLeft, frontY};
    Vector2 fr{frontRight, frontY};
    Color floorColor{30, 46, 40, 255};
    DrawTriangle(bl, fl, fr, floorColor);
    DrawTriangle(bl, fr, br, floorColor);
    DrawLineEx(bl, br, 2.0f, Color{80, 110, 100, 255});
}

void drawGrid(const jf::BattleController& controller) {
    const jf::BattleState& battle = controller.battle();

    drawBattlefieldBackdrop();

    // Farther rows are drawn first and shaded slightly darker to reinforce
    // depth; nearer rows are brighter, as if closer to the (fixed) camera.
    for (int row = 0; row < jf::kGridRows; ++row) {
        const RowLayout& rl = rowLayouts()[row];
        unsigned char shade = static_cast<unsigned char>(35 + rl.scale * 25.0f);
        for (int col = 0; col < jf::kGridCols; ++col) {
            jf::GridPos pos{row, col};
            Rectangle rect = tileRect(pos);
            DrawRectangleRec(rect, Color{shade, static_cast<unsigned char>(shade + 6),
                                          static_cast<unsigned char>(shade + 22), 255});
            DrawRectangleLinesEx(rect, 1, Color{70, 75, 95, 255});

            if (containsTile(controller.reachableTiles(), pos)) {
                DrawRectangleRec(rect, Color{60, 130, 230, 110});
            }
            if (containsTile(controller.targetableTiles(), pos) ||
                (controller.pendingTarget() && controller.pendingTarget()->position == pos)) {
                DrawRectangleRec(rect, Color{230, 70, 70, 130});
            }
        }
    }

    // Draw back-to-front so nearer (larger) units correctly overlap farther ones.
    for (int row = 0; row < jf::kGridRows; ++row) {
        float scale = rowLayouts()[row].scale;
        for (const jf::Unit& unit : battle.units()) {
            if (!unit.isAlive() || unit.position.row != row) continue;

            Rectangle rect = tileRect(unit.position);
            Color c = teamColor(unit.team);
            if (unit.hasActed) c = Fade(c, 0.45f);
            if (controller.selectedUnit() == &unit) {
                DrawRectangleLinesEx(Rectangle{rect.x - 3, rect.y - 3, rect.width + 6, rect.height + 6}, 3, YELLOW);
            }
            DrawRectangleRec(Rectangle{rect.x + 8, rect.y + 8 * scale, rect.width - 16, rect.height - 24 * scale}, c);

            // HP bar
            float hpRatio = static_cast<float>(unit.currentHp) / static_cast<float>(unit.stats.maxHp);
            Rectangle hpBack{rect.x + 6, rect.y + rect.height - 12 * scale, rect.width - 12, 8 * scale};
            DrawRectangleRec(hpBack, Color{20, 20, 20, 255});
            DrawRectangleRec(Rectangle{hpBack.x, hpBack.y, hpBack.width * hpRatio, hpBack.height},
                              hpRatio > 0.5f ? GREEN : (hpRatio > 0.25f ? ORANGE : RED));

            int fontSize = std::max(9, static_cast<int>(12 * scale));
            drawText(unit.name, static_cast<int>(rect.x + 4), static_cast<int>(rect.y + 2), fontSize, RAYWHITE);
        }
    }
}

void drawPhaseBanner(const jf::BattleController& controller) {
    bool isPlayerPhase = controller.battle().phase() == jf::Phase::PlayerPhase;
    std::string label = (isPlayerPhase ? std::string("PLAYER PHASE / ") + kJaPlayerPhase
                                        : std::string("ENEMY PHASE / ") + kJaEnemyPhase);
    Color color = isPlayerPhase ? Color{60, 120, 220, 255} : Color{200, 60, 60, 255};
    DrawRectangle(0, 0, kScreenWidth, 40, color);
    int w = textWidth(label, 22);
    drawText(label, (kScreenWidth - w) / 2, 8, 22, RAYWHITE);
    drawText("JOJIFrontier - Battle Prototype", 12, 44, 18, Color{180, 180, 195, 255});
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
                          ", Rng " + std::to_string(unit.weapon.minRange) + "-" +
                          std::to_string(unit.weapon.maxRange) + ")";
    drawText(weapon, x, y + 92, 14, Color{190, 190, 205, 255});
}

void drawCombatPreview(const jf::CombatPreview& preview, int x, int y) {
    drawText("COMBAT PREVIEW / " + kJaCombatPreview, x, y, 18, YELLOW);
    drawText(preview.attackerName, x, y + 26, 18, RAYWHITE);
    drawText(preview.weaponName, x, y + 46, 14, Color{190, 190, 205, 255});
    std::string dmg = "Damage(" + kJaDamage + "): " + std::to_string(preview.damage);
    drawText(dmg, x, y + 68, 16, RAYWHITE);
    std::string hp = preview.targetName + " HP";
    drawText(hp, x, y + 90, 14, Color{190, 190, 205, 255});
    std::string hpChange = std::to_string(preview.targetHpBefore) + " -> " + std::to_string(preview.targetHpAfter);
    drawText(hpChange, x, y + 108, 18, RED);
}

void drawBattleSidePanel(jf::GameApp& app, Vector2 mouse, bool clicked) {
    jf::BattleController& controller = app.battle();
    int x = kPanelX;
    int y = 100;

    DrawRectangle(kPanelX - 10, 90, kPanelWidth + 20, kScreenHeight - 100, Color{25, 27, 38, 255});

    if (jf::Unit* selected = controller.selectedUnit()) {
        drawUnitInfo(*selected, x, y);
        y += 130;
    } else {
        drawText("Select a unit to act. / " + kJaSelectUnit, x, y, 15, Color{190, 190, 205, 255});
        y += 40;
    }

    switch (controller.inputState()) {
        case jf::BattleInputState::SelectAction: {
            if (button(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 44}, "Attack", kJaAttack, mouse,
                       clicked)) {
                controller.chooseAttack();
            }
            if (button(Rectangle{static_cast<float>(x), static_cast<float>(y + 54), 220, 44}, "Wait", kJaWait, mouse,
                       clicked)) {
                controller.chooseWait();
            }
            break;
        }
        case jf::BattleInputState::ConfirmAttack: {
            if (auto preview = controller.pendingPreview()) {
                drawCombatPreview(*preview, x, y);
                y += 140;
            }
            if (button(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 44}, "Confirm", kJaConfirm,
                       mouse, clicked)) {
                controller.confirmAttack();
            }
            if (button(Rectangle{static_cast<float>(x), static_cast<float>(y + 54), 220, 44}, "Cancel", kJaCancel,
                       mouse, clicked)) {
                controller.cancelToUnitSelect();
            }
            break;
        }
        case jf::BattleInputState::SelectMove:
        case jf::BattleInputState::SelectTarget: {
            if (button(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 44}, "Cancel", kJaCancel, mouse,
                       clicked)) {
                controller.cancelToUnitSelect();
            }
            break;
        }
        default:
            break;
    }
}

void drawVictoryOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 190});
    std::string title = "VICTORY / " + kJaVictory;
    drawText(title, kScreenWidth / 2 - textWidth(title, 44) / 2, 260, 44, GOLD);
    std::string subtitle = "Proceed to Camp / " + kJaProceedToCamp;
    drawText(subtitle, kScreenWidth / 2 - textWidth(subtitle, 18) / 2, 320, 18, RAYWHITE);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 380, 260, 50};
    if (button(rect, "Proceed to Camp", kJaProceedToCamp, mouse, clicked)) {
        app.proceedToCamp();
    }
}

void drawDefeatOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 190});
    std::string title = "EXPEDITION FAILED / " + kJaDefeatTitle;
    drawText(title, kScreenWidth / 2 - textWidth(title, 38) / 2, 260, 38, RED);
    std::string line = "All expedition loot would be lost. / " + kJaLootLostLine;
    drawText(line, kScreenWidth / 2 - textWidth(line, 16) / 2, 312, 16, RAYWHITE);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 380, 260, 50};
    if (button(rect, "Return to Base", kJaReturnToBase, mouse, clicked)) {
        app.acknowledgeDefeat();
    }
}

void drawCampScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{20, 24, 34, 255});
    drawText("CAMP / " + kJaCamp, 40, 30, 32, RAYWHITE);

    if (app.justSecuredLoot()) {
        drawText("Loot Secured / " + kJaLootSecured, 40, 90, 24, GOLD);
        int y = 140;
        for (const std::string& item : app.lastSecuredLoot()) {
            drawText("- " + item, 60, y, 18, RAYWHITE);
            y += 26;
        }
        if (app.lastSecuredLoot().empty()) {
            drawText("(no loot this expedition) / (" + kJaNoLootThisExpedition + ")", 60, y, 16,
                     Color{190, 190, 205, 255});
            y += 26;
        }
        Rectangle rect{40, static_cast<float>(y + 30), 260, 50};
        if (button(rect, "Continue", kJaContinue, mouse, clicked)) {
            app.acknowledgeLootSecured();
        }
        return;
    }

    int y = 90;
    drawText("Party HP / " + kJaPartyHp, 40, y, 20, RAYWHITE);
    y += 30;
    for (const jf::Unit& unit : app.battle().battle().units()) {
        if (unit.team != jf::Team::Player) continue;
        std::string line = unit.name + " (" + jf::toString(unit.unitClass) + "): " +
                            std::to_string(unit.currentHp) + " / " + std::to_string(unit.stats.maxHp) +
                            (unit.isAlive() ? "" : " - Down / " + kJaDown);
        drawText(line, 60, y, 16, unit.isAlive() ? RAYWHITE : Color{160, 60, 60, 255});
        y += 24;
    }

    y += 20;
    drawText("Pending Expedition Loot / " + kJaPendingLoot, 40, y, 20, RAYWHITE);
    y += 30;
    if (app.expedition().pendingLoot.empty()) {
        drawText("(none yet) / (" + kJaNoneYet + ")", 60, y, 16, Color{190, 190, 205, 255});
        y += 24;
    }
    for (const std::string& item : app.expedition().pendingLoot) {
        drawText("- " + item, 60, y, 16, RAYWHITE);
        y += 22;
    }

    y += 10;
    std::string won = "Battles Won / " + kJaBattlesWon + ": " + std::to_string(app.expedition().battlesWon);
    drawText(won, 40, y, 16, RAYWHITE);
    y += 40;

    Rectangle continueRect{40, static_cast<float>(y), 300, 50};
    Rectangle returnRect{360, static_cast<float>(y), 260, 50};
    if (button(continueRect, "Continue Expedition", kJaContinueExpedition, mouse, clicked)) {
        app.continueExpedition();
    }
    if (button(returnRect, "Return to Base", kJaReturnToBase, mouse, clicked)) {
        app.returnToBase();
    }
}

void handleGridClick(jf::BattleController& controller, jf::GridPos pos) {
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
            drawText("Failed to load data/ (classes.json, units.json, weapons.json). / " + kJaDataLoadFailed, 20, 20,
                      18, RED);
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

        app.update(dt);

        if (app.screen() == jf::Screen::Battle) {
            jf::BattleController& controller = app.battle();

            if (clicked) {
                jf::GridPos pos;
                if (tileFromScreen(mouse, pos)) {
                    handleGridClick(controller, pos);
                }
            }

            BeginDrawing();
            ClearBackground(Color{16, 18, 26, 255});
            drawPhaseBanner(controller);
            drawGrid(controller);
            drawBattleSidePanel(app, mouse, clicked);

            if (controller.inputState() == jf::BattleInputState::Victory) {
                drawVictoryOverlay(app, mouse, clicked);
            } else if (controller.inputState() == jf::BattleInputState::Defeat) {
                drawDefeatOverlay(app, mouse, clicked);
            }
            EndDrawing();
        } else {
            BeginDrawing();
            drawCampScreen(app, mouse, clicked);
            EndDrawing();
        }
    }

    if (gFontReady) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
