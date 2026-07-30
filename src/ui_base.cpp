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
// joined candidate. Candidate display/class data comes from GameData's
// recruit definitions, matching GameApp::confirmRecruitJoin().
void drawBaseRecruitBanner(jf::GameApp& app, Vector2 mouse, bool clicked) {
    for (const std::string& candidateId : app.joinReadyCandidateIds()) {
        if (app.joinedRecruitIds().count(candidateId)) continue;
        const jf::UnitTemplate* recruit = app.gameData().recruitDefinition(candidateId);
        if (!recruit) continue;
        std::string name = unitDisplayNameFor(recruit->name);
        std::string className = classNameFor(app.gameData(), recruit->classId);
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
int drawBasePartyRoster(jf::GameApp& app, Vector2 mouse, bool clicked, std::vector<TooltipLine>& hoverLines) {
    drawSectionHeading(tr("ui.prep.party_choose4"), 52, 92, 20);
    int y = 125;
    for (const auto& unit : app.roster()) {
        bool selected = std::find(app.selectedPartyIds().begin(), app.selectedPartyIds().end(), unit.id) != app.selectedPartyIds().end();
        std::string label = std::string(selected ? "[✓] " : "[ ] ") + unitDisplayNameFor(unit.name) + " - " +
                            classNameFor(app.gameData(), unit.classId);
        Rectangle rowRect{40, static_cast<float>(y), 260, 34};
        Rectangle detailRect{308, static_cast<float>(y), 80, 34};
        Rectangle compareRect{392, static_cast<float>(y), 80, 34};
        if (button(rowRect, label, "", mouse, clicked)) app.togglePartyMember(unit.id);
        if (button(detailRect, "Details", "詳細", mouse, clicked)) gBaseScreen.viewedUnitId = unit.id;
        // docs/character_progression.md「ユニットページ」一覧「比較対象を1人固定できる」:
        // toggling the same Unit again clears the pin.
        bool isComparisonTarget = gBaseScreen.comparisonUnitId == unit.id;
        if (button(compareRect, isComparisonTarget ? "[✓] Compare" : "Compare",
                  isComparisonTarget ? "[✓] 比較" : "比較", mouse, clicked)) {
            gBaseScreen.comparisonUnitId = isComparisonTarget ? std::nullopt : std::optional(unit.id);
        }
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
        y += 39;
    }
    return y;
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
        drawText(clipTextToWidth(ownedLabel, 14, static_cast<int>(nameRect.width)),
                 static_cast<int>(nameRect.x), static_cast<int>(nameRect.y), 14, kColorTextPrimary);

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
// behavior change. Returns the Y just below the last drawn row, so callers
// placing content beneath this list (drawBaseOutpostInfo's discoveries
// section) can offset dynamically instead of assuming a fixed row count -
// this list grows with every new region (7 as of M9-Y, more to come), and a
// hardcoded discoveries-section Y previously overlapped it once the list
// grew past that fixed position.
int drawBaseRegionList(jf::GameApp& app, Vector2 mouse, bool clicked, std::vector<TooltipLine>& hoverLines) {
    drawSectionHeading(tr("exploration.region_section"), 480, 405, 18);
    const auto summaries = app.regionSummaries();
    constexpr int kStartY = 433;
    constexpr int kDefaultRowStep = 45;
    constexpr int kRowHeight = 40;
    constexpr int kListBottom = kScreenHeight - 20;
    constexpr float kScrollPixelsPerWheelStep = 55.0f;
    const Rectangle listViewport{480.0f, static_cast<float>(kStartY), 300.0f,
                                 static_cast<float>(kListBottom - kStartY)};
    const float contentHeight = static_cast<float>(summaries.size() * kDefaultRowStep);
    const float maxScroll = std::max(0.0f, contentHeight - listViewport.height);
    if (CheckCollisionPointRec(mouse, listViewport)) {
        gBaseScreen.regionListScroll =
            std::clamp(gBaseScreen.regionListScroll - GetMouseWheelMove() * kScrollPixelsPerWheelStep,
                       0.0f, maxScroll);
    } else {
        gBaseScreen.regionListScroll = std::clamp(gBaseScreen.regionListScroll, 0.0f, maxScroll);
    }

    DrawRectangleRec(listViewport, Color{18, 21, 30, 255});
    int y = kStartY - static_cast<int>(gBaseScreen.regionListScroll);
    for (const auto& summary : summaries) {
        Rectangle rowRect{480, static_cast<float>(y), 300, static_cast<float>(kRowHeight)};
        const bool visible = rowRect.y >= listViewport.y &&
                             rowRect.y + rowRect.height <= listViewport.y + listViewport.height;
        if (!visible) {
            y += kDefaultRowStep;
            continue;
        }
        std::string marker = summary.id == gBaseScreen.selectedRegionId ? "> " : "";
        if (summary.unlocked) {
            if (button(rowRect, marker + summary.displayNameEn, marker + summary.displayNameJa, mouse, clicked))
                gBaseScreen.selectedRegionId = summary.id;
        } else {
            disabledButton(rowRect, pick(summary.displayNameEn, summary.displayNameJa));
            if (CheckCollisionPointRec(mouse, rowRect)) {
                hoverLines = {
                    {pick(summary.displayNameEn, summary.displayNameJa), kColorAccentGold, 16},
                    {tr("exploration.region_locked", {{"region", pick(summary.lockedByDisplayNameEn, summary.lockedByDisplayNameJa)}}),
                     kColorTextMuted, 13},
                };
            }
        }
        y += kDefaultRowStep;
    }
    if (maxScroll > 0.0f) {
        const float thumbHeight = std::max(34.0f, listViewport.height * listViewport.height / contentHeight);
        const float thumbTrack = listViewport.height - thumbHeight;
        const float thumbY = listViewport.y + (maxScroll <= 0.0f ? 0.0f : thumbTrack * (gBaseScreen.regionListScroll / maxScroll));
        DrawRectangleRounded(Rectangle{listViewport.x + listViewport.width + 8.0f, listViewport.y, 5.0f, listViewport.height},
                             0.5f, 4, Color{38, 43, 55, 180});
        DrawRectangleRounded(Rectangle{listViewport.x + listViewport.width + 8.0f, thumbY, 5.0f, thumbHeight},
                             0.5f, 4, kColorBorder);
    }
    return kListBottom;
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
    // docs/character_progression.md「連携作戦」: squad-wide single slot,
    // cycles through {none, every unlocked pair with a battle effect} on
    // click (no dedicated picker screen for just 1 slot / up to 5 choices -
    // `paired_cross_observation` is never offered since it has no battle
    // effect to select for, see Cooperation.hpp's own comment).
    //
    // docs/implementation_status.md「連携作戦UIの情報不足」#1: keep the
    // cooperation control below the heading's rendered line height. Japanese
    // font scaling makes the label taller than its nominal fontSize, so a
    // small fixed offset clips into the first button.
    constexpr int kCooperationHeadingY = 415;
    drawSectionHeading(tr("ui.prep.cooperation_label"), 830, kCooperationHeadingY, 18);
    Rectangle cooperationSlot{830, kCooperationHeadingY + textLineHeight(18) + 12.0f, 370, 40};
    std::vector<std::string> selectableIds = {""};
    for (const jf::CooperationDefinition& def : jf::cooperationDefinitions()) {
        if (def.hasBattleEffect && jf::isCooperationUnlocked(def.id, app.baseState())) selectableIds.push_back(def.id);
    }
    if (button(cooperationSlot, cooperationNameFor(app.equippedCooperationId()), "", mouse, clicked)) {
        auto it = std::find(selectableIds.begin(), selectableIds.end(), app.equippedCooperationId());
        std::size_t nextIndex = (it == selectableIds.end()) ? 0 : (static_cast<std::size_t>(it - selectableIds.begin()) + 1) % selectableIds.size();
        app.equipCooperation(selectableIds[nextIndex]);
    }
    // docs/implementation_status.md「連携作戦UIの情報不足」#2: the slot only
    // ever showed the pair's bare name (clicking cycles blind through every
    // unlocked option) - hovering it now previews what the CURRENTLY
    // equipped pair actually does, same name+description hover pattern the
    // bag slots above already use.
    if (!app.equippedCooperationId().empty() && CheckCollisionPointRec(mouse, cooperationSlot)) {
        hoverLines = {
            {cooperationNameFor(app.equippedCooperationId()), kColorAccentGold, 16},
            {cooperationDescriptionFor(app.equippedCooperationId()), kColorTextMuted, 13},
        };
    }

    Rectangle start{830, cooperationSlot.y + cooperationSlot.height + 24.0f, 370, 58};
    if (app.selectedPartyIds().size() == 4) {
        if (button(start, tr("ui.button.begin_expedition"), mouse, clicked)) {
            jf::RegionId toStart = app.isRegionUnlocked(gBaseScreen.selectedRegionId) ? gBaseScreen.selectedRegionId : jf::RegionId::AshboughForest;
            app.startExpedition(toStart);
        }
    } else disabledButton(start, tr("ui.validation.select_exactly_4"));
}

// Outpost stage name/advance button and the Discovery registry list. Split
// out of drawBaseScreen(); no behavior change. `regionListBottom` is
// drawBaseRegionList()'s return value - the discoveries column (x=492) sits
// directly below the region list (x=480-780), so its heading must never sit
// above a fixed Y that the region list can outgrow (see that function's own
// comment).
void drawBaseOutpostInfo(jf::GameApp& app, Vector2 mouse, bool clicked, int regionListBottom, int partyListBottom) {
    const jf::BaseState& base = app.baseState();
    const int outpostY = std::max(520, partyListBottom + 20);
    const int sectionY = std::max(520, regionListBottom + 20);
    const int contentY = sectionY + 32;
    drawSectionHeading(tr("ui.outpost.title"), 40, outpostY, 20);
    // M10-D (docs/deep_layers.md「拠点発展のLv化」): stage name (derived from
    // outpostLevel) plus the numeric Lv itself, then a Lv-by-Lv climb button
    // driven entirely by jf::activeOutpostLevelCheckpoint()/
    // outpostCheckpointGateMet()/outpostLevelStepCost() - no per-stage UI
    // branches, so this scales to any future checkpoint without new code.
    drawText(outpostStageNameFor(base.outpostStage()) + " (Lv " + std::to_string(base.outpostLevel) + ")", 40, outpostY + 32,
              22, kColorTextPrimary);
    Rectangle advanceRect{40, static_cast<float>(outpostY + 68), 390, 46};
    const jf::OutpostLevelCheckpoint* checkpoint = jf::activeOutpostLevelCheckpoint(base.outpostLevel);
    if (!checkpoint) {
        // At/above kMaxImplementedOutpostLevel - no further checkpoint exists
        // in code yet (Lv10-15/15-20 reserved for deep layers).
    } else if (!jf::outpostCheckpointGateMet(base, *checkpoint)) {
        disabledButton(advanceRect, tr("ui.outpost.advance_locked"));
    } else {
        const std::vector<jf::LootStack> stepCost = jf::outpostLevelStepCost(*checkpoint, base.outpostLevel + 1);
        bool canAfford = true;
        for (const jf::LootStack& stack : stepCost)
            if (base.storageCount(stack.id) < stack.quantity) canAfford = false;
        if (canAfford) {
            if (button(advanceRect, tr("ui.outpost.advance"), mouse, clicked)) app.advanceOutpostLevel();
        } else {
            disabledButton(advanceRect, tr("ui.outpost.advance_locked"));
        }
    }

    drawSectionHeading(tr("ui.outpost.discoveries"), 492, sectionY, 20);
    if (base.discoveryRegistry.empty()) {
        drawText(tr("ui.outpost.no_discoveries_yet"), 492, contentY, 18, kColorTextMuted);
    } else {
        int discoveryY = contentY;
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
    const int partyListBottom = drawBasePartyRoster(app, mouse, clicked, hoverLines);
    drawBaseSupplies(app, mouse, clicked, hoverLines);
    const int regionListBottom = drawBaseRegionList(app, mouse, clicked, hoverLines);
    drawBaseBagAndExpedition(app, mouse, clicked, hoverLines);
    drawBaseOutpostInfo(app, mouse, clicked, regionListBottom, partyListBottom);
    drawTooltipBox(mouse, hoverLines);
}

}  // namespace jfui
