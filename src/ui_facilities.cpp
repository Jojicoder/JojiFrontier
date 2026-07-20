// Facilities and Unit screens: facility card grid, facility detail (node
// list, upgrades, Forge class-recipe picker), and the Unit equipment
// screen. Split out of main.cpp; no behavior change.
#include <raylib.h>

#include <algorithm>
#include <string>
#include <vector>

#include "jf/core/GameApp.hpp"
#include "jf/data/GameData.hpp"
#include "ui_facilities.hpp"
#include "ui_shared.hpp"

namespace jfui {

std::string facilityIdNameFor(jf::FacilityId id) {
    switch (id) {
        case jf::FacilityId::CommandPost: return tr("facility.command_post.name");
        case jf::FacilityId::TrainingGround: return tr("facility.training_ground.name");
        case jf::FacilityId::Forge: return tr("facility.forge.name");
        case jf::FacilityId::Infirmary: return tr("facility.infirmary.name");
        case jf::FacilityId::Workshop: return tr("facility.workshop.name");
        case jf::FacilityId::Barracks: return tr("facility.barracks.name");
    }
    return "";
}

std::string facilityRoleFor(jf::FacilityId id) {
    switch (id) {
        case jf::FacilityId::CommandPost: return tr("facility.command_post.role");
        case jf::FacilityId::TrainingGround: return tr("facility.training_ground.role");
        case jf::FacilityId::Forge: return tr("facility.forge.role");
        case jf::FacilityId::Infirmary: return tr("facility.infirmary.role");
        case jf::FacilityId::Workshop: return tr("facility.workshop.role");
        case jf::FacilityId::Barracks: return tr("facility.barracks.role");
    }
    return "";
}

int facilityLevel(const jf::BaseState& base, jf::FacilityId facility) {
    int level = 0;
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.facility == facility && base.unlockedNodeIds.count(node.id)) ++level;
    }
    return level;
}

// docs/base_development.md: facilities have no active/inactive toggle state -
// a node is either constructed (permanently) or not. This reports the latter,
// never an operational status.
bool facilityIsConstructed(const jf::BaseState& base, jf::FacilityId facility) {
    bool hasConstructibleRoot = false;
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.facility != facility || !node.occupiesFacilitySlot) continue;
        hasConstructibleRoot = true;
        if (base.constructedFacilityIds.count(node.id)) return true;
    }
    if (hasConstructibleRoot) return false;
    return facilityLevel(base, facility) > 0;
}

void drawFacilityTooltip(jf::FacilityId facility, const jf::BaseState& base, Vector2 mouse) {
    constexpr float width = 560.0f;
    const std::string role = wrapTextToWidth(facilityRoleFor(facility), 13, static_cast<int>(width - 44.0f));
    const int roleLines = static_cast<int>(textLines(role).size());
    const float height = 132.0f + roleLines * textLineHeight(13) + 18.0f;
    float x = std::min(mouse.x + 16.0f, kScreenWidth - width - 12.0f);
    float y = std::min(mouse.y + 16.0f, kScreenHeight - height - 12.0f);
    Rectangle rect{x, y, width, height};
    drawCard(rect, Color{19, 23, 33, 255}, kColorAccentGold, 0.06f);
    const std::string title = facilityIdNameFor(facility) + "  " +
                              tr("ui.facilities.branches_unlocked", {{"count", std::to_string(facilityLevel(base, facility))}});
    drawText(title, static_cast<int>(x + 22), static_cast<int>(y + 18), 18, kColorAccentGold);
    const bool constructed = facilityIsConstructed(base, facility);
    drawText(constructed ? tr("ui.facility_node.constructed") : tr("ui.facility_node.not_constructed"),
             static_cast<int>(x + 22), static_cast<int>(y + 62), 12,
             constructed ? Color{105, 205, 145, 255} : Color{180, 125, 125, 255});
    DrawLineEx(Vector2{x + 22, y + 100}, Vector2{x + width - 22, y + 100}, 1.5f, kColorBorderSoft);
    drawText(role, static_cast<int>(x + 22), static_cast<int>(y + 116), 13, kColorTextPrimary);
}

// Forward declaration: defined below, but the tooltip (drawn earlier in the
// file) needs the full, non-abbreviated reason text too.
std::string facilityNodeBlockedReason(const jf::BaseState& base, const jf::FacilityNode& node, bool compact);

// Hover tooltip for a single facility node: its effect summary plus every
// required material with a live have/need count (green once satisfied, red
// while short), so the player can see exactly what a node does and what it
// still needs without clicking Unlock/Build first. If the node is currently
// locked, the full (untruncated) reason is shown too - the node-row action
// zone is only ~130px wide and has to abbreviate, but the tooltip never does.
void drawFacilityNodeTooltip(const jf::FacilityNode& node, const jf::BaseState& base, Vector2 mouse) {
    constexpr float width = 700.0f;
    const std::string effect = wrapTextToWidth(pick(node.effectEn, node.effectJa), 13,
                                               static_cast<int>(width - 44.0f));
    const int effectLineCount = 1 + static_cast<int>(std::count(effect.begin(), effect.end(), '\n'));
    const int materialLineCount = node.materialCosts.empty() ? 1 : static_cast<int>(node.materialCosts.size());
    const bool unlocked = base.unlockedNodeIds.count(node.id) > 0;
    const std::string reason = unlocked ? "" : wrapTextToWidth(facilityNodeBlockedReason(base, node, false), 13,
                                                               static_cast<int>(width - 44.0f));
    const int reasonLineCount = reason.empty() ? 0 : static_cast<int>(textLines(reason).size());
    const float effectHeight = effectLineCount * textLineHeight(13);
    const float reasonHeight = reasonLineCount * textLineHeight(13);
    const float height = 72.0f + effectHeight + 42.0f + materialLineCount * 32.0f + 18.0f + reasonHeight;
    float x = std::min(mouse.x + 16.0f, kScreenWidth - width - 12.0f);
    float y = std::min(mouse.y + 16.0f, kScreenHeight - height - 12.0f);
    Rectangle rect{x, y, width, height};
    drawCard(rect, Color{19, 23, 33, 255}, kColorAccentGold, 0.06f);

    drawText(pick(node.nameEn, node.nameJa), static_cast<int>(x + 22), static_cast<int>(y + 16), 17,
             kColorAccentGold);
    drawText(effect, static_cast<int>(x + 22), static_cast<int>(y + 56), 13, kColorTextPrimary);

    float rowY = y + 56 + effectHeight + 14.0f;
    DrawLineEx(Vector2{x + 22, rowY - 8}, Vector2{x + width - 22, rowY - 8}, 1.5f, kColorBorderSoft);
    drawText(tr("ui.materials_label"), static_cast<int>(x + 22),
             static_cast<int>(rowY), 12, kColorTextFaint);
    rowY += 34;
    if (node.materialCosts.empty()) {
        drawText(tr("ui.no_material_cost"), static_cast<int>(x + 30), static_cast<int>(rowY), 13,
                 kColorTextMuted);
        rowY += 32;
    } else {
        for (const jf::LootStack& cost : node.materialCosts) {
            int have = base.storageCount(cost.id);
            bool enough = have >= cost.quantity;
            std::string line = materialNameFor(cost.id) + "  " + std::to_string(have) + " / " +
                               std::to_string(cost.quantity);
            drawText(line, static_cast<int>(x + 30), static_cast<int>(rowY), 13,
                     enough ? Color{140, 210, 150, 255} : Color{215, 130, 130, 255});
            rowY += 32;
        }
    }
    if (!reason.empty()) {
        rowY += 8;
        DrawLineEx(Vector2{x + 22, rowY - 8}, Vector2{x + width - 22, rowY - 8}, 1.5f, kColorBorderSoft);
        drawText(reason, static_cast<int>(x + 22), static_cast<int>(rowY), 13, Color{215, 130, 130, 255});
    }
}

// Mirrors facilityNodeEligible()'s checks in order, and reports which one
// is currently the blocker - keeps the UI text data-driven rather than a
// second hardcoded copy of the unlock rules.
// `compact` picks the abbreviated stage name for the ~130px-wide node-row
// action zone; the hover tooltip calls this with compact=false so the full,
// untruncated requirement is always available on hover.
std::string facilityNodeBlockedReason(const jf::BaseState& base, const jf::FacilityNode& node, bool compact = true) {
    if (static_cast<int>(base.outpostStage) < static_cast<int>(node.requiredStage)) {
        return tr("ui.facilities.needs_stage_prefix") +
               (compact ? outpostStageShortNameFor(node.requiredStage) : outpostStageNameFor(node.requiredStage));
    }
    for (const jf::DiscoveryId& discovery : node.requiredDiscoveries)
        if (!base.discoveryRegistry.count(discovery)) return tr("ui.facilities.needs_discovery");
    for (const std::string& prereqId : node.prerequisiteNodeIds) {
        const jf::FacilityNode* prereq = jf::findFacilityNode(prereqId);
        bool satisfied = prereq && prereq->occupiesFacilitySlot ? base.constructedFacilityIds.count(prereqId) > 0
                                                                 : base.unlockedNodeIds.count(prereqId) > 0;
        if (!satisfied) return tr("ui.facilities.needs_facility_built");
    }
    for (const jf::LootStack& cost : node.materialCosts) {
        if (base.storageCount(cost.id) < cost.quantity)
            return tr("ui.facilities.needs_material_prefix") + materialNameFor(cost.id);
    }
    return "";
}

// docs/base_development.md: no facility-slot cap, no dismantle, no rebuild -
// a node is either unlocked (and, for the 4 optional facilities, therefore
// permanently built) or it isn't. Only 3 row states exist: unlocked, eligible
// (shows a Build/Unlock button), or blocked (shows why).
void drawFacilityNodeRow(jf::GameApp& app, const jf::FacilityNode& node, float x, float y, float width,
                         Vector2 mouse, bool clicked) {
    const jf::BaseState& base = app.baseState();
    bool unlocked = base.unlockedNodeIds.count(node.id) > 0;

    constexpr float kActionZoneWidth = 132;
    constexpr float kNameLeftPad = 14;
    Rectangle actionRect{x + width - kActionZoneWidth, y, kActionZoneWidth, 24};
    // Leaves a visible gap before the action zone so long names/reasons can
    // never visually run into the button/label on the right.
    const int nameMaxWidth = static_cast<int>(actionRect.x - (x + kNameLeftPad) - 10);

    DrawCircle(static_cast<int>(x + 5), static_cast<int>(y + 12), 3.0f,
               unlocked ? Color{95, 205, 140, 255} : kColorTextFaint);
    drawText(clipTextToWidth(pick(node.nameEn, node.nameJa), 14, nameMaxWidth), static_cast<int>(x + kNameLeftPad),
             static_cast<int>(y) + 5, 14, unlocked ? kColorTextPrimary : kColorTextMuted);

    if (unlocked) {
        drawText(tr("ui.facilities.unlocked"), static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13,
                 kColorTextFaint);
    } else if (jf::facilityNodeEligible(base, node)) {
        std::string label = node.occupiesFacilitySlot ? tr("ui.facilities.build") : tr("ui.facilities.unlock");
        if (button(actionRect, label, mouse, clicked)) app.unlockFacilityNode(node.id);
    } else {
        std::string reason = clipTextToWidth(facilityNodeBlockedReason(base, node), 13,
                                             static_cast<int>(actionRect.width - 4));
        drawText(reason, static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13,
                 Color{205, 135, 135, 255});
    }
}

// English half of a Forge weapon's name, used both as the display fallback
// and as the lookup key fed into weaponNameFor() for the Japanese half.
std::string weaponEnglishName(const std::string& weaponId) {
    if (weaponId == "iron_spear") return "Iron Spear";
    if (weaponId == "long_spear") return "Long Spear";
    if (weaponId == "heavy_spear") return "Heavy Spear";
    if (weaponId == "guard_spear") return "Guard Spear";
    return weaponId;
}

// Only Spearman currently has Forge branch weapons/traits to manage
// (docs/base_development.md's Iron Spear branches + Hide-Wrapped Grip).
void drawForgeEquipmentPanel(jf::GameApp& app, const jf::UnitTemplate& unit, float x, float y, float width,
                             Vector2 mouse, bool clicked) {
    drawSectionHeading(tr("ui.forge.equipment_heading"), static_cast<int>(x),
                       static_cast<int>(y), 18);
    const jf::BaseState& base = app.baseState();
    auto overrideIt = app.weaponOverrides().find(unit.id);
    std::string current = overrideIt != app.weaponOverrides().end() ? overrideIt->second : "iron_spear";
    drawText(tr("ui.facilities.current_weapon_prefix") + weaponNameFor(current, weaponEnglishName(current)),
             static_cast<int>(x), static_cast<int>(y) + 30, 14, kColorTextMuted);

    struct Candidate { const char* id; const char* nodeId; };
    static const Candidate kCandidates[] = {
        {"iron_spear", nullptr},
        {"long_spear", "craft_long_spear"},
        {"heavy_spear", "craft_heavy_spear"},
        {"guard_spear", "craft_guard_spear"},
    };
    float by = y + 58;
    const float candidateWidth = (width - 12.0f) / 2.0f;
    for (int index = 0; index < 4; ++index) {
        const Candidate& candidate = kCandidates[index];
        bool available = candidate.nodeId == nullptr || base.unlockedNodeIds.count(candidate.nodeId) > 0;
        Rectangle rect{x + (index % 2) * (candidateWidth + 12.0f), by + (index / 2) * 46.0f,
                       candidateWidth, 34};
        std::string labelEn = weaponEnglishName(candidate.id);
        std::string labelJa = weaponNameFor(candidate.id, weaponEnglishName(candidate.id));
        if (available) {
            if (button(rect, labelEn, labelJa, mouse, clicked))
                app.equipWeaponForUnit(unit.id, candidate.id);
        } else {
            disabledButton(rect, pick(labelEn, labelJa));
        }
    }

    by += 100;
    bool traitUnlocked = base.unlockedNodeIds.count("trait_hide_wrapped_grip") > 0;
    bool traitEquipped = app.equippedTraits().count(unit.id) > 0;
    Rectangle traitRect{x, by, 280, 34};
    if (traitUnlocked) {
        if (button(traitRect, tr(traitEquipped ? "ui.facilities.unequip_trait" : "ui.facilities.equip_trait"), mouse,
                  clicked)) {
            app.equipTuningTraitForUnit(unit.id,
                                        traitEquipped ? jf::TuningTraitId::None : jf::TuningTraitId::HideWrappedGrip);
        }
    } else {
        disabledButton(traitRect, tr("ui.facilities.trait_locked"));
    }
}

void drawUnitScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    auto unit = std::find_if(app.roster().begin(), app.roster().end(), [&](const jf::UnitTemplate& candidate) {
        return gBaseScreen.viewedUnitId && candidate.id == *gBaseScreen.viewedUnitId;
    });
    if (unit == app.roster().end()) {
        gBaseScreen.viewedUnitId.reset();
        return;
    }

    ClearBackground(Color{18, 21, 30, 255});
    drawText(tr("ui.unit_screen.title"), 38, 24, 28, kColorTextPrimary);
    Rectangle backRect{static_cast<float>(kScreenWidth) - 258.0f, 4.0f, 150.0f, 32.0f};
    if (button(backRect, "Party List", "編成一覧へ", mouse, clicked)) {
        gBaseScreen.viewedUnitId.reset();
        return;
    }

    Rectangle identity{42, 104, 470, 500};
    drawCard(identity, kColorCard, kColorBorderSoft, 0.04f);
    drawText(unitDisplayNameFor(unit->name), 72, 132, 28, kColorAccentGold);
    drawText(classNameFor(app.gameData(), unit->classId), 72, 184, 20, kColorTextPrimary);
    const std::string role = wrapTextToWidth(classRoleFor(app.gameData(), unit->classId), 14, 400);
    drawText(role, 72, 230, 14, kColorTextMuted);
    const jf::Stats& stats = app.gameData().classDefinition(unit->classId).baseStats;
    drawSectionHeading(tr("ui.unit_screen.stats"), 72, 330, 18);
    drawText("HP " + std::to_string(stats.maxHp) + "    STR " + std::to_string(stats.strength) +
                 "    MAG " + std::to_string(stats.magic), 72, 374, 16, kColorTextPrimary);
    drawText("DEF " + std::to_string(stats.defense) + "    RES " + std::to_string(stats.resistance) +
                 "    MOV " + std::to_string(stats.move), 72, 418, 16, kColorTextPrimary);

    Rectangle equipment{548, 104, 690, 500};
    drawCard(equipment, kColorCard, kColorBorderSoft, 0.04f);
    if (unit->classId == jf::UnitClass::Spearman) {
        drawForgeEquipmentPanel(app, *unit, 580, 136, 626, mouse, clicked);
        if (!app.baseState().constructedFacilityIds.count("simple_forge"))
            drawText(tr("ui.unit_screen.needs_forge"),
                     580, 390, 14, Color{205, 135, 135, 255});
    } else {
        drawSectionHeading(tr("ui.unit_screen.equipment_heading"), 580, 136, 18);
        const std::string weaponId = app.gameData().classDefinition(unit->classId).weaponId;
        drawText(tr("ui.unit_screen.current_weapon_prefix") + weaponNameFor(weaponId, weaponEnglishName(weaponId)),
                 580, 188, 16, kColorTextPrimary);
        drawText(tr("ui.unit_screen.no_alt_equipment"),
                 580, 246, 14, kColorTextMuted);
    }
}

// The facility card grid (list view, shown when no facility is visited
// yet). Split out of drawFacilitiesScreen(); no behavior change.
void drawFacilitiesList(jf::GameApp& app, Vector2 mouse, bool clicked, const jf::BaseState& base,
                         const Rectangle& backRect) {
    drawText(tr("ui.facilities.title"), 38, 24, 28, kColorTextPrimary);
    drawText(outpostStageNameFor(base.outpostStage), 38, 60, 16, kColorTextMuted);
    if (button(backRect, tr("ui.button.back"), mouse, clicked)) {
        gBaseScreen.visitedFacility.reset();
        gBaseScreen.forgeCraftClass.reset();
        gBaseScreen.showFacilities = false;
    }

    const jf::FacilityId facilities[] = {
        jf::FacilityId::CommandPost, jf::FacilityId::TrainingGround, jf::FacilityId::Forge,
        jf::FacilityId::Infirmary, jf::FacilityId::Workshop, jf::FacilityId::Barracks,
    };
    bool hasHoveredFacility = false;
    jf::FacilityId hoveredFacility = jf::FacilityId::CommandPost;
    for (int index = 0; index < 6; ++index) {
        const jf::FacilityId facility = facilities[index];
        const int col = index % 2;
        const int row = index / 2;
        Rectangle card{42.0f + col * 610.0f, 112.0f + row * 166.0f, 574.0f, 138.0f};
        drawCard(card, kColorCard, kColorBorderSoft, 0.04f);
        drawText(facilityIdNameFor(facility), static_cast<int>(card.x + 22), static_cast<int>(card.y + 18),
                 21, kColorTextPrimary);
        const bool constructed = facilityIsConstructed(base, facility);
        drawText(tr("ui.facilities.branches_unlocked", {{"count", std::to_string(facilityLevel(base, facility))}}),
                 static_cast<int>(card.x + 22), static_cast<int>(card.y + 59), 14, kColorAccentGold);
        drawText(constructed ? tr("ui.facility_node.constructed") : tr("ui.facility_node.not_constructed"),
                 static_cast<int>(card.x + 112), static_cast<int>(card.y + 59), 14,
                 constructed ? Color{105, 205, 145, 255} : Color{180, 125, 125, 255});
        Rectangle visitRect{card.x + card.width - 174.0f, card.y + 82.0f, 150.0f, 40.0f};
        if (button(visitRect, "Visit", "訪れる", mouse, clicked)) {
            gBaseScreen.visitedFacility = facility;
            gBaseScreen.forgeCraftClass.reset();
        }
        if (CheckCollisionPointRec(mouse, card) && !CheckCollisionPointRec(mouse, visitRect)) {
            hasHoveredFacility = true;
            hoveredFacility = facility;
        }
    }
    if (hasHoveredFacility) drawFacilityTooltip(hoveredFacility, base, mouse);
}

// The visited-facility detail page: upgrade/recipe node list plus the info
// panel (including the Forge's class-recipe picker). Split out of
// drawFacilitiesScreen(); no behavior change.
void drawFacilityDetail(jf::GameApp& app, Vector2 mouse, bool clicked, const jf::BaseState& base,
                         const Rectangle& backRect) {
    const jf::FacilityId facility = *gBaseScreen.visitedFacility;
    const bool forgeCraftPage = facility == jf::FacilityId::Forge && gBaseScreen.forgeCraftClass.has_value();
    const std::string pageTitle = forgeCraftPage
                                      ? tr("ui.forge.craft_prefix") + classNameFor(app.gameData(), *gBaseScreen.forgeCraftClass)
                                      : facilityIdNameFor(facility);
    drawText(pageTitle, 38, 24, 28, kColorTextPrimary);
    drawText(tr("ui.facilities.branches_unlocked", {{"count", std::to_string(facilityLevel(base, facility))}}),
             38, 64, 16, kColorAccentGold);
    drawText(facilityRoleFor(facility), 138, 58, 14, kColorTextMuted);
    if (forgeCraftPage) {
        if (button(backRect, tr("ui.forge.back_to_forge"), mouse, clicked)) gBaseScreen.forgeCraftClass.reset();
    } else if (button(backRect, tr("ui.facilities.back_to_list"), mouse, clicked)) {
        gBaseScreen.visitedFacility.reset();
    }

    drawSectionHeading(forgeCraftPage ? tr("ui.forge.recipes") : tr("ui.forge.upgrades"),
                       42, 132, 18);
    const jf::FacilityNode* hoveredNode = nullptr;
    float nodeY = 174.0f;
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.facility != facility) continue;
        const bool isWeaponRecipe = node.id.rfind("craft_", 0) == 0;
        if (facility == jf::FacilityId::Forge) {
            if (forgeCraftPage && (!isWeaponRecipe || *gBaseScreen.forgeCraftClass != jf::UnitClass::Spearman)) continue;
            if (!forgeCraftPage && isWeaponRecipe) continue;
        }
        Rectangle rowPanel{36.0f, nodeY - 5.0f, 696.0f, 38.0f};
        DrawRectangleRec(rowPanel, Color{25, 30, 42, 255});
        drawFacilityNodeRow(app, node, 48.0f, nodeY, 672.0f, mouse, clicked);
        if (CheckCollisionPointRec(mouse, rowPanel)) hoveredNode = &node;
        nodeY += 44.0f;
    }

    Rectangle infoPanel{770.0f, 128.0f, 470.0f, 510.0f};
    drawCard(infoPanel, kColorCard, kColorBorderSoft, 0.04f);
    drawSectionHeading(forgeCraftPage ? tr("ui.forge.class_recipes")
                                      : tr("ui.forge.facility_details"),
                       794, 150, 18);
    if (facility == jf::FacilityId::Forge && !forgeCraftPage) {
        drawText(tr("ui.forge.choose_class"),
                 794, 192, 14, kColorTextPrimary);
        const jf::UnitClass craftClasses[] = {
            jf::UnitClass::MarchCaptain, jf::UnitClass::VeteranGuard, jf::UnitClass::WatchArcher,
            jf::UnitClass::FrontierScout, jf::UnitClass::Spearman, jf::UnitClass::DawnChirurgeon,
        };
        for (int index = 0; index < 6; ++index) {
            Rectangle craftRect{794.0f, 232.0f + index * 58.0f, 410.0f, 46.0f};
            const std::string label = tr("ui.forge.craft_prefix") + classNameFor(app.gameData(), craftClasses[index]);
            if (craftClasses[index] == jf::UnitClass::Spearman) {
                if (button(craftRect, label, mouse, clicked)) gBaseScreen.forgeCraftClass = craftClasses[index];
            } else {
                disabledButton(craftRect, label + tr("ui.forge.craft_planned_suffix"));
            }
        }
    } else {
        drawText(forgeCraftPage ? tr("ui.forge.crafted_weapons_note")
                                : facilityRoleFor(facility),
                 794, 192, 14, kColorTextPrimary);
        drawText(tr("ui.forge.hover_hint"),
                 794, 292, 14, kColorTextMuted);
    }
    if (hoveredNode) drawFacilityNodeTooltip(*hoveredNode, base, mouse);
}

void drawFacilitiesScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    const jf::BaseState& base = app.baseState();
    Rectangle backRect{static_cast<float>(kScreenWidth) - 258.0f, 4.0f, 150.0f, 32.0f};

    if (!gBaseScreen.visitedFacility) {
        drawFacilitiesList(app, mouse, clicked, base, backRect);
        return;
    }

    drawFacilityDetail(app, mouse, clicked, base, backRect);
}

}  // namespace jfui
