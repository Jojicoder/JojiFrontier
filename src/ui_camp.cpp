// Camp screen rendering: party HP/next-field summary, the Continue/Return/
// Items command row, and the Items sub-menu. Split out of main.cpp; no
// behavior change.
#include <raylib.h>

#include <algorithm>
#include <optional>
#include <string>

#include "jf/core/GameApp.hpp"
#include "jf/data/GameData.hpp"
#include "ui_camp.hpp"
#include "ui_shared.hpp"

namespace jfui {

struct CampScreenState {
    bool itemMenuOpen = false;
    std::optional<jf::ItemType> selectedItem;
};
CampScreenState gCampScreen;

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
        gCampScreen.itemMenuOpen = false;
        gCampScreen.selectedItem.reset();
        // docs/inventory_overflow.md「帰還処理」: a false return means the
        // 200-Stack pending ceiling would be exceeded - route to cleanup
        // instead of silently doing nothing.
        if (!app.returnToBase()) gWarehouseCleanupOpen = true;
    }
    if (button(itemsRect, tr("ui.button.items"), mouse, clicked)) {
        gCampScreen.itemMenuOpen = true;
        gCampScreen.selectedItem.reset();
    }
}

// The Items sub-panel (usable-item list, then unit-target list once one's
// picked). Split out of drawCampScreen(); no behavior change.
void drawCampItemMenu(jf::GameApp& app, Vector2 mouse, bool clicked) {
    Rectangle itemPanel{620, 300, 610, 390};
    DrawRectangle(0, 70, kScreenWidth, kScreenHeight - 70, Color{0, 0, 0, 105});
    drawCard(itemPanel, Color{20, 25, 36, 255}, withAlpha(kColorAccentGold, 230), 0.05f);
    drawText(gCampScreen.selectedItem ? tr("ui.camp.choose_target")
                               : tr("ui.camp.usable_items"),
             646, 324, 21, kColorAccentGold);

    const auto& units = app.battle().battle().units();
    const bool hasWounded = std::any_of(units.begin(), units.end(), [](const jf::Unit& unit) {
        return unit.team == jf::Team::Player && unit.isAlive() && unit.currentHp < unit.stats.maxHp;
    });
    const bool hasDefeated = std::any_of(units.begin(), units.end(), [](const jf::Unit& unit) {
        return unit.team == jf::Team::Player && !unit.isAlive();
    });

    if (!gCampScreen.selectedItem) {
        int itemY = 372;
        int usableCount = 0;
        const auto itemChoice = [&](jf::ItemType type, bool usable) {
            const int count = app.expedition().count(type);
            if (!usable || count <= 0) return;
            if (button(Rectangle{646, static_cast<float>(itemY), 430, 48},
                       itemFullNameFor(type) + "  x" + std::to_string(count), "", mouse, clicked)) {
                if (type == jf::ItemType::CampRations) {
                    if (app.useCampItem(type)) gCampScreen.itemMenuOpen = false;
                } else if (type == jf::ItemType::ReturnFlare) {
                    if (app.useCampItem(type)) gCampScreen.itemMenuOpen = false;
                } else {
                    gCampScreen.selectedItem = type;
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
            const bool rescue = *gCampScreen.selectedItem == jf::ItemType::RescuePack;
            const bool valid = rescue ? !unit.isAlive() : unit.isAlive() && unit.currentHp < unit.stats.maxHp;
            if (!valid) continue;
            std::string label = unitDisplayNameFor(unit.name) + "  HP " + std::to_string(unit.currentHp) + "/" +
                                std::to_string(unit.stats.maxHp);
            if (button(Rectangle{646, static_cast<float>(targetY), 430, 48}, label, "", mouse, clicked) &&
                app.useCampItem(*gCampScreen.selectedItem, unit.id)) {
                gCampScreen.selectedItem.reset();
                gCampScreen.itemMenuOpen = false;
                break;
            }
            targetY += 58;
        }
    }

    if (button(Rectangle{1090, 626, 116, 44}, tr("ui.button.back"), mouse, clicked)) {
        if (gCampScreen.selectedItem) gCampScreen.selectedItem.reset();
        else gCampScreen.itemMenuOpen = false;
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
    if (!gCampScreen.itemMenuOpen) drawCampCommandButtons(app, mouse, clicked);
    if (gCampScreen.itemMenuOpen) drawCampItemMenu(app, mouse, clicked);
}

}  // namespace jfui
