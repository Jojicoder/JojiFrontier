// Pre-Battle Deployment screen: board tiles/enemy preview/placed allies
// and the placement click handler, plus the HUD bar with per-slot
// placement buttons and Back/Begin Battle buttons. Split out of main.cpp;
// no behavior change.
#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

#include "jf/core/GameApp.hpp"
#include "jf/core/Grid.hpp"
#include "ui_deployment.hpp"
#include "ui_shared.hpp"

namespace jfui {

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

}  // namespace jfui
