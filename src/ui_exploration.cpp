// Exploration screen rendering: mission header, site status, and the
// route-choice buttons (safe passage/reconnaissance once secured, or
// frontal/side/scout routes otherwise). Split out of main.cpp; no
// behavior change.
#include <raylib.h>

#include <string>

#include "jf/core/GameApp.hpp"
#include "ui_exploration.hpp"
#include "ui_shared.hpp"

namespace jfui {

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

}  // namespace jfui
