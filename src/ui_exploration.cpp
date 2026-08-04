// Exploration screen rendering: mission header, site status, and the
// route-choice buttons (safe passage/reconnaissance once secured, or
// frontal/side/scout routes otherwise). Split out of main.cpp; no
// behavior change.
#include <raylib.h>

#include <optional>
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

        // docs/deep_layers.md: shortcut into the deep-layer expeditions from
        // AshboughForest's own already-Secured site screen, gated the same
        // way the Base region list gates them (isRegionUnlocked()).
        if (isAshbough) {
            // 2026-08-02: only offer this shortcut before the player has
            // ever entered that deep layer at least once (BaseState::
            // deepLayerRegionsEntered) - previously it re-asked on every
            // single visit to this already-Secured site. Once entered
            // (through here or the Base region list), use the Base screen's
            // region list instead.
            const auto& enteredDeepLayers = app.baseState().deepLayerRegionsEntered;
            if (app.isRegionUnlocked(jf::RegionId::AshboughForestDeep) &&
               !enteredDeepLayers.count(jf::RegionId::AshboughForestDeep)) {
                Rectangle deepRect{160, 550, 960, 90};
                if (button(deepRect, tr("exploration.enter_deep_layer"), mouse, clicked))
                    app.startDeepLayerExpeditionFromSecuredSite(jf::RegionId::AshboughForestDeep);
            }
            if (app.isRegionUnlocked(jf::RegionId::AshboughForestDeepest) &&
               !enteredDeepLayers.count(jf::RegionId::AshboughForestDeepest)) {
                Rectangle deepestRect{160, 655, 960, 90};
                if (button(deepestRect, tr("exploration.enter_deepest_layer"), mouse, clicked))
                    app.startDeepLayerExpeditionFromSecuredSite(jf::RegionId::AshboughForestDeepest);
            }
        }
        return;
    }

    // Command Post "Scout Network" node effect: reveal what's waiting ahead
    // regardless of which route gets picked.
    if (app.scoutNetworkUnlocked()) {
        drawText(tr("ui.deployment.enemy_forces") + tr("ui.deployment.scout_network_suffix"), 42, 168, 16,
                 kColorAccentGold);
        int enemyX = 42;
        for (const jf::Unit& enemy : app.explorationEnemyPreview()) {
            drawText(unitDisplayNameFor(enemy.name), enemyX, 192, 15, kColorTextMuted);
            enemyX += textWidth(unitDisplayNameFor(enemy.name), 15) + 26;
        }
    }

    Rectangle frontal{70, 225, 520, 120};
    Rectangle sidePath{650, 225, 520, 120};
    Rectangle scoutRect{360, 400, 560, 90};

    // docs/prompts/exploration_system_improvement_prompt.md(2026-08-02、
    // Phase 2続き)・docs/prompts/exploration_effect_summary_improvement_prompt.md
    // (2026-08-02、項目2): describe what this route actually does (from its
    // real ExplorationOutcome), coloring each effect by its tone (danger/
    // caution/benefit/neutral) instead of one flat muted line. Ashbough
    // Forest keeps its own narrative flavor text, with the colored effect
    // tokens appended in parentheses; every other stage shows the tokens
    // directly.
    auto drawRouteEffect = [](bool ashboughFlavor, const std::string& flavorKey, int x, int y,
                              const jf::ExplorationOutcome& outcome) {
        int cursorX = x;
        if (ashboughFlavor) {
            const std::string flavor = tr(flavorKey) + "(";
            drawText(flavor, cursorX, y, 15, kColorTextMuted);
            cursorX += textWidth(flavor, 15);
        }
        cursorX = drawExplorationEffectTokens(buildExplorationEffectDisplayTokens(outcome), cursorX, y, 15);
        if (ashboughFlavor) drawText(")", cursorX, y, 15, kColorTextMuted);
    };
    // docs/prompts/exploration_randomization_prompt.md(2026-08-02設計):
    // any of the 3 slots may now be gated on a class (previously only slot
    // C/ScoutRoute could be), since a template's rolled candidate for A or B
    // can itself require one. requiredClassForChoice() returns nullopt for
    // the old always-open cases (A/B on a hand-authored or unrolled stage),
    // so this reduces to the pre-randomization behavior there.
    auto drawGatedRoute = [&app, &mouse, clicked, &drawRouteEffect](Rectangle rect, const std::string& label,
                                                                    jf::ExplorationChoice choice, bool ashboughFlavor,
                                                                    const std::string& flavorKey, int textX,
                                                                    int textY) {
        const std::optional<jf::UnitClass> requiredClass = app.requiredClassForChoice(choice);
        if (!requiredClass || app.partyHasClass(*requiredClass)) {
            if (button(rect, label, mouse, clicked)) app.chooseExplorationRoute(choice);
            drawRouteEffect(ashboughFlavor, flavorKey, textX, textY, app.explorationOutcomeForChoice(choice));
        } else {
            disabledButton(rect, label);
            // 2026-08-02(Phase 2): name the actual required class instead of
            // a fixed "requires a Frontier Scout" string.
            drawText(tr("exploration.scout_route_locked", {{"class", classNameFor(app.gameData(), *requiredClass)}}),
                     textX, textY, 15, Color{200, 110, 110, 255});
        }
    };

    const std::string frontalLabel = isAshbough
                                         ? tr("exploration.ashbough_frontal")
                                         : "A. " + tr(app.explorationChoiceLabelKey(jf::ExplorationChoice::FrontalAdvance));
    const std::string sidePathLabel =
        isAshbough ? tr("exploration.ashbough_side_path")
                   : "B. " + tr(app.explorationChoiceLabelKey(jf::ExplorationChoice::CollapsedSidePath));
    const std::string scoutLabel =
        "C. " + (isAshbough ? tr("exploration.ashbough_scout_route")
                            : tr(app.explorationChoiceLabelKey(jf::ExplorationChoice::ScoutRoute)));

    drawGatedRoute(frontal, frontalLabel, jf::ExplorationChoice::FrontalAdvance, isAshbough,
                   "exploration.ashbough_frontal_effect", 92, 310);
    drawGatedRoute(sidePath, sidePathLabel, jf::ExplorationChoice::CollapsedSidePath, isAshbough,
                   "exploration.ashbough_side_path_effect", 672, 310);
    drawGatedRoute(scoutRect, scoutLabel, jf::ExplorationChoice::ScoutRoute, isAshbough,
                   "exploration.ashbough_scout_route_effect", 382, 470);
}

}  // namespace jfui
