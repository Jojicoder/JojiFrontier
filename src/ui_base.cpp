// Base screen rendering: top bar, party roster picker, supplies craft/add
// column, region picker, bag/expedition column, and outpost info. Split
// out of main.cpp; no behavior change.
#include <raylib.h>

#include <algorithm>
#include <string>
#include <vector>

#include "jf/core/GameApp.hpp"
#include "jf/data/GameData.hpp"
#include "ui_base.hpp"
#include "ui_shared.hpp"

namespace jfui {

// Top bar: screen title plus the Facilities/Warehouse buttons. Split out of
// drawBaseScreen(); no behavior change.
void drawBaseTopBar(jf::GameApp& app, Vector2 mouse, bool clicked) {
    drawText(tr("ui.prep.title"), 38, 30, 30, kColorTextPrimary);
    Rectangle facilitiesRect{static_cast<float>(kScreenWidth) - 218.0f, 4.0f, 110.0f, 32.0f};
    if (button(facilitiesRect, tr("ui.facilities.title"), mouse, clicked)) {
        gBaseScreen.visitedFacility.reset();
        gBaseScreen.forgeCraftClass.reset();
        gBaseScreen.showFacilities = true;
    }
    Rectangle warehouseRect{static_cast<float>(kScreenWidth) - 336.0f, 4.0f, 110.0f, 32.0f};
    if (button(warehouseRect, tr("ui.warehouse.open_button"), mouse, clicked)) gWarehouseCleanupOpen = true;
}

// docs/roster_design.md「加入処理の共通ルール」: minimal functional trigger for
// GameApp::confirmRecruitJoin() - the real 集会所 conversation UI (choices,
// read-state, portraits) is a separate, larger scope (docs/gathering_place.md,
// M7項目4) not attempted here. Shows one line per join-ready-but-not-yet-
// joined candidate; only "heavy_recruit" actually resolves to a class today
// (see GameApp::confirmRecruitJoin()'s own comment).
void drawBaseRecruitBanner(jf::GameApp& app, Vector2 mouse, bool clicked) {
    for (const std::string& candidateId : app.joinReadyCandidateIds()) {
        if (app.joinedRecruitIds().count(candidateId)) continue;
        // Only heavy_recruit is wired to a display name/class today (mirrors
        // GameApp::confirmRecruitJoin()'s own single-id gate).
        if (candidateId != "heavy_recruit") continue;
        std::string name = unitDisplayNameFor("Hadric");
        std::string className = classNameFor(app.gameData(), jf::UnitClass::HeavyInfantry);
        bool hasCapacity = static_cast<int>(app.roster().size()) < app.recruitCapacity();
        Rectangle textRect{40, 66, 480, 24};
        Rectangle buttonRect{528, 60, 140, 34};
        if (hasCapacity) {
            drawText(tr("ui.recruit.join_available", {{"name", name}, {"class", className}}), 40, 68, 17,
                     kColorAccentGold);
            if (button(buttonRect, tr("ui.recruit.join_button"), mouse, clicked)) app.confirmRecruitJoin(candidateId);
        } else {
            drawText(tr("ui.recruit.join_capacity_full", {{"name", name}}), 40, 68, 17, kColorTextMuted);
        }
    }
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
        if (button(detailRect, "Details", "詳細", mouse, clicked)) gBaseScreen.viewedUnitId = unit.id;
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
        std::string marker = summary.id == gBaseScreen.selectedRegionId ? "> " : "";
        if (summary.unlocked) {
            if (button(rowRect, marker + summary.displayNameEn, marker + summary.displayNameJa, mouse, clicked))
                gBaseScreen.selectedRegionId = summary.id;
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
            jf::RegionId toStart = app.isRegionUnlocked(gBaseScreen.selectedRegionId) ? gBaseScreen.selectedRegionId : jf::RegionId::AshboughForest;
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
    drawBaseRecruitBanner(app, mouse, clicked);
    std::vector<TooltipLine> hoverLines;
    drawBasePartyRoster(app, mouse, clicked, hoverLines);
    drawBaseSupplies(app, mouse, clicked, hoverLines);
    drawBaseRegionList(app, mouse, clicked, hoverLines);
    drawBaseBagAndExpedition(app, mouse, clicked, hoverLines);
    drawBaseOutpostInfo(app, mouse, clicked);
    drawTooltipBox(mouse, hoverLines);
}

}  // namespace jfui
