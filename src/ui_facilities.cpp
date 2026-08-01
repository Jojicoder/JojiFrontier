// Facilities and Unit screens: facility card grid, facility detail (node
// list, upgrades, Forge class-recipe picker), and the Unit equipment
// screen. Split out of main.cpp; no behavior change.
#include <raylib.h>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
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

// True if any `craft_*` weapon-branch recipe is registered for this class
// (docs/implementation_roadmap.md "M7項目3(残り) ...特性・武器分岐の他兵種
// 一般化"). Data-driven so the Forge UI never needs a per-class hardcode.
bool classHasWeaponBranchRecipes(jf::UnitClass unitClass) {
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.id.rfind("craft_", 0) == 0 && node.weaponBranchClass == unitClass) return true;
    }
    return false;
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
    // 「解放済み分岐 N」は施設一覧のカード本体に既に表示されている
    // (drawFacilitiesList()側)ため、このツールチップでは重複表示せず
    // 施設名だけにする - ユーザー指摘「91とか4とかって何？」は、この
    // カウントがクラフト/装飾品ノードも1件ずつ数えた合計で、意味のある
    // 「分岐」数として読めていなかったことによる。
    drawText(facilityIdNameFor(facility), static_cast<int>(x + 22), static_cast<int>(y + 18), 18, kColorAccentGold);
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
    if (static_cast<int>(base.outpostStage()) < static_cast<int>(node.requiredStage)) {
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

    // M10-A (docs/deep_layers.md「Lv制」): once a branch weapon is crafted,
    // its row switches from the static "unlocked" label to a Lv readout plus
    // a strengthen button (when the next Lv's materials are affordable) -
    // ASCII-only text ("Lv N"/"MAX"), so no new JA glyph needs registering in
    // loadAppFont()'s charset (see project memory on JA glyph coverage).
    // M10-B: armor recipes ("craft_armor_*") get their own Lv readout below,
    // checked BEFORE the generic weapon "craft_*" check (every armor id also
    // starts with "craft_") so an armor row never gets misread as a weapon
    // id via node.id.substr(6).
    const bool isArmorRecipe = node.id.rfind("craft_armor_", 0) == 0;
    const bool isWeaponRecipe = !isArmorRecipe && node.id.rfind("craft_", 0) == 0;
    if (unlocked && isArmorRecipe) {
        const std::string armorId = node.id.substr(6); // strip "craft_" only, keep "armor_..." prefix
        const int level = base.armorLevel(armorId);
        const std::string lvLabel = "Lv " + std::to_string(level);
        if (level >= jf::BaseState::kMaxArmorLevel) {
            drawText(lvLabel + " MAX", static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13,
                     kColorTextFaint);
        } else {
            const jf::FacilityNode* recipe = jf::findFacilityNode(node.id);
            std::vector<jf::LootStack> cost =
                recipe && !recipe->materialCosts.empty()
                    ? jf::armorLevelUpCost(armorId, recipe->materialCosts[0].id, level + 1)
                    : std::vector<jf::LootStack>{};
            bool canAfford = !cost.empty();
            for (const jf::LootStack& stack : cost)
                if (base.storageCount(stack.id) < stack.quantity) canAfford = false;
            if (canAfford) {
                if (button(actionRect, lvLabel + " ->", mouse, clicked)) app.strengthenArmor(armorId);
            } else {
                drawText(lvLabel, static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13, kColorTextFaint);
            }
        }
    } else if (unlocked && isWeaponRecipe) {
        const std::string weaponId = node.id.substr(6);
        const int level = base.weaponLevel(weaponId);
        const std::string lvLabel = "Lv " + std::to_string(level);
        if (level >= jf::BaseState::kMaxWeaponLevel) {
            drawText(lvLabel + " MAX", static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13,
                     kColorTextFaint);
        } else {
            std::vector<jf::LootStack> cost = jf::weaponLevelUpCost(weaponId, level + 1);
            bool canAfford = !cost.empty();
            for (const jf::LootStack& stack : cost)
                if (base.storageCount(stack.id) < stack.quantity) canAfford = false;
            if (canAfford) {
                if (button(actionRect, lvLabel + " ->", mouse, clicked)) app.strengthenWeapon(weaponId);
            } else {
                drawText(lvLabel, static_cast<int>(actionRect.x + 4), static_cast<int>(y) + 6, 13, kColorTextFaint);
            }
        }
    } else if (unlocked) {
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
    static const std::unordered_map<std::string, std::string> kNames = {
        {"iron_spear", "Iron Spear"}, {"long_spear", "Long Spear"}, {"heavy_spear", "Heavy Spear"},
        {"guard_spear", "Guard Spear"},
        {"iron_sword", "Iron Sword"}, {"command_sword", "Command Sword"}, {"duel_sword", "Duel Sword"},
        {"guard_sword", "Guard Sword"},
        {"iron_lance", "Iron Lance"}, {"hook_lance", "Hook Lance"}, {"fortress_lance", "Fortress Lance"},
        {"patrol_lance", "Patrol Lance"},
        {"watch_bow", "Watch Bow"}, {"long_watch_bow", "Long Watch Bow"}, {"war_bow", "War Bow"},
        {"pinning_bow", "Pinning Bow"},
        {"scout_blade", "Scout Blade"}, {"trail_blade", "Trail Blade"}, {"ambush_blade", "Ambush Blade"},
        {"withdrawal_blade", "Withdrawal Blade"},
        {"dawn_staff", "Dawn Staff"}, {"mercy_staff", "Mercy Staff"}, {"ward_staff", "Ward Staff"},
        {"march_staff", "March Staff"},
        {"iron_greathammer", "Iron Greathammer"}, {"bulwark_maul", "Bulwark Maul"},
        {"breaker_maul", "Breaker Maul"}, {"driving_maul", "Driving Maul"},
        {"engineer_hammer", "Engineer Hammer"}, {"builder_hammer", "Builder Hammer"},
        {"demolition_hammer", "Demolition Hammer"}, {"repair_hammer", "Repair Hammer"},
        {"messenger_sword", "Courier Sabre"}, {"road_sabre", "Road Sabre"}, {"charge_lance", "Charge Lance"},
        {"escort_blade", "Escort Blade"},
        {"hunting_bow", "Hunting Bow"}, {"snare_bow", "Snare Bow"}, {"quarry_bow", "Quarry Bow"},
        {"driving_bow", "Driving Bow"},
        {"banner_spear", "Standard Spear"}, {"far_standard", "Far Standard"}, {"valor_standard", "Valor Standard"},
        {"warding_standard", "Warding Standard"},
        {"arcane_focus", "Arcane Focus"}, {"resonant_focus", "Resonant Focus"}, {"war_focus", "War Focus"},
        {"ember_focus", "Ember Focus"},
    };
    auto it = kNames.find(weaponId);
    return it != kNames.end() ? it->second : weaponId;
}

// docs/character_progression.md「ユニットページ/詳細」: "武器、特性、スキルを選ぶと、
// 右側へ「現在」「変更後」「変わる戦術」「失うもの」を表示する". Filled in by
// drawForgeEquipmentPanel()/drawSkillEquipmentPanel() whenever the mouse hovers a
// candidate, and consumed by drawUnitScreen() to render the 4-block diff card.
// Traits are out of scope: no trait *selection* UI exists yet (only a single
// binary equip/unequip toggle for "hide_wrapped_grip"/"ward_step" per slot),
// so there is no second candidate to diff against - see
// drawForgeEquipmentPanel()'s trait buttons below.
//
// M10-E (docs/character_progression.md「ユニットページ/詳細」): armor gets the
// same 4-block diff panel weapons/skills already have. A 3-way enum (rather
// than stacking a 2nd bool alongside `isSkill`) keeps "which candidate kind"
// a single, exhaustive switch instead of an isSkill/isArmor cross-product
// with an invalid 4th combination.
enum class EquipmentHoverKind { Weapon, Skill, Armor };

struct EquipmentHover {
    EquipmentHoverKind kind = EquipmentHoverKind::Weapon;
    std::string currentWeaponId;   // valid when kind == Weapon
    std::string hoveredWeaponId;
    std::string currentSkillId;    // valid when kind == Skill (may be empty = no skill equipped)
    std::string hoveredSkillId;
    std::string currentArmorId;    // valid when kind == Armor (may be empty = no armor equipped)
    std::string hoveredArmorId;
};

// Short localized tags for a weapon's qualitative (non-numeric) effects, used
// to derive both "変わる戦術" (gained tags) and "失うもの" (lost tags) - see
// weaponDiffLines() below.
std::vector<std::string> weaponEffectTags(const jf::Weapon& weapon) {
    std::vector<std::string> tags;
    if (weapon.causesKnockback) tags.push_back(tr("ui.unit_screen.diff.knockback_tag"));
    if (weapon.braceBoost) tags.push_back(tr("ui.unit_screen.diff.brace_boost_tag"));
    for (jf::StatusEffectType status : weapon.onHitStatuses) {
        switch (status) {
            case jf::StatusEffectType::Poison: tags.push_back(tr("status.poison.label")); break;
            case jf::StatusEffectType::Burn: tags.push_back(tr("status.burn.label")); break;
            case jf::StatusEffectType::MoveDown: tags.push_back(tr("status.move_down.label")); break;
            case jf::StatusEffectType::DefenseDown: tags.push_back(tr("status.def_down.label")); break;
            case jf::StatusEffectType::Stagger: tags.push_back(tr("status.stagger.label")); break;
        }
    }
    return tags;
}

std::string weaponRangeText(const jf::Weapon& weapon) {
    if (weapon.minRange == weapon.maxRange) return std::to_string(weapon.minRange);
    return std::to_string(weapon.minRange) + "-" + std::to_string(weapon.maxRange);
}

std::string weaponSummaryLine(const jf::Weapon& weapon) {
    return tr("ui.unit_screen.diff.might_prefix") + std::to_string(weapon.might) + "   " +
           tr("ui.unit_screen.diff.range_prefix") + weaponRangeText(weapon);
}

std::string weaponDamageTypeName(jf::DamageType type) {
    return type == jf::DamageType::Physical ? tr("ui.weapon_tooltip.physical") : tr("ui.weapon_tooltip.magical");
}

std::vector<std::string> weaponDetailLines(const jf::Weapon& weapon) {
    std::vector<std::string> details;
    if (weapon.moveModifier != 0)
        details.push_back(tr("ui.weapon_tooltip.move_modifier",
                             {{"value", (weapon.moveModifier > 0 ? "+" : "") + std::to_string(weapon.moveModifier)}}));
    if (weapon.braceBoost) details.push_back(tr("ui.weapon_tooltip.brace_boost"));
    if (weapon.causesKnockback)
        details.push_back(weapon.firstHitOnly ? tr("ui.weapon_tooltip.knockback_first")
                                              : tr("ui.weapon_tooltip.knockback"));
    for (jf::StatusEffectType status : weapon.onHitStatuses) {
        std::string label;
        switch (status) {
            case jf::StatusEffectType::Poison: label = tr("status.poison.label"); break;
            case jf::StatusEffectType::Burn: label = tr("status.burn.label"); break;
            case jf::StatusEffectType::MoveDown: label = tr("status.move_down.label"); break;
            case jf::StatusEffectType::DefenseDown: label = tr("status.def_down.label"); break;
            case jf::StatusEffectType::Stagger: label = tr("status.stagger.label"); break;
        }
        details.push_back(weapon.firstHitOnly ? tr("ui.weapon_tooltip.status_first", {{"status", label}})
                                              : tr("ui.weapon_tooltip.status", {{"status", label}}));
    }
    if (weapon.healAmountOverride > 0)
        details.push_back(tr("ui.weapon_tooltip.heal_amount",
                             {{"value", std::to_string(weapon.healAmountOverride)}}));
    if (weapon.healGrantsResistanceUp) details.push_back(tr("ui.weapon_tooltip.heal_res_up"));
    if (weapon.healGrantsMoveUp) details.push_back(tr("ui.weapon_tooltip.heal_move_up"));
    if (weapon.fieldRepairAmountOverride > 0)
        details.push_back(tr("ui.weapon_tooltip.repair_amount",
                             {{"value", std::to_string(weapon.fieldRepairAmountOverride)}}));
    if (weapon.splashDamage > 0)
        details.push_back(tr("ui.weapon_tooltip.splash", {{"value", std::to_string(weapon.splashDamage)}}));
    if (weapon.pullsAtRangeTwo) details.push_back(tr("ui.weapon_tooltip.pull_range_two"));
    if (weapon.trailblazeOnMove) details.push_back(tr("ui.weapon_tooltip.trailblaze"));
    return details;
}

std::vector<TooltipLine> weaponTooltipLines(const std::string& weaponId, const jf::Weapon& weapon, bool available) {
    std::vector<TooltipLine> lines;
    lines.push_back({weaponNameFor(weaponId, weaponEnglishName(weaponId)), kColorAccentGold, 16});
    lines.push_back({tr("ui.weapon_tooltip.stats",
                        {{"might", std::to_string(weapon.might)},
                         {"range", weaponRangeText(weapon)},
                         {"type", weaponDamageTypeName(weapon.damageType)}}),
                     Color{170, 180, 195, 255}, 12});

    const std::vector<std::string> details = weaponDetailLines(weapon);
    if (details.empty()) {
        lines.push_back({tr("ui.weapon_tooltip.no_special"), kColorTextPrimary, 13});
    } else {
        for (const std::string& detail : details) lines.push_back({detail, kColorTextPrimary, 13});
    }
    if (!available) lines.push_back({tr("ui.weapon_tooltip.locked"), Color{225, 120, 120, 255}, 12});
    return lines;
}

std::string armorTierName(jf::ArmorTier tier) {
    switch (tier) {
        case jf::ArmorTier::Tier1: return tr("ui.armor_tooltip.tier1");
        case jf::ArmorTier::Tier2: return tr("ui.armor_tooltip.tier2");
        case jf::ArmorTier::Tier3: return tr("ui.armor_tooltip.tier3");
    }
    return "";
}

std::vector<TooltipLine> armorTooltipLines(const jf::ArmorDefinition& armor, int level, bool available) {
    std::vector<TooltipLine> lines;
    lines.push_back({pick(armor.nameEn, armor.nameJa), kColorAccentGold, 16});
    lines.push_back({tr("ui.armor_tooltip.stats",
                        {{"level", std::to_string(level)},
                         {"def", std::to_string(jf::armorDefBonusAtLevel(armor, level))},
                         {"res", std::to_string(jf::armorResBonusAtLevel(armor, level))}}),
                     Color{170, 180, 195, 255}, 12});
    lines.push_back({armorTierName(armor.tier), kColorTextPrimary, 13});
    if (armor.tier == jf::ArmorTier::Tier1)
        lines.push_back({tr("ui.armor_tooltip.tier1_detail"), kColorTextPrimary, 13});
    else if (armor.tier == jf::ArmorTier::Tier2)
        lines.push_back({tr("ui.armor_tooltip.tier2_detail"), kColorTextPrimary, 13});
    else
        lines.push_back({tr("ui.armor_tooltip.tier3_detail"), kColorTextPrimary, 13});
    if (!available) lines.push_back({tr("ui.armor_tooltip.locked"), Color{225, 120, 120, 255}, 12});
    return lines;
}

// "変わる戦術": numeric stat deltas (both directions) plus any effect tags the
// hovered weapon adds. "失うもの": effect tags only the current weapon has.
void weaponDiffLines(const jf::Weapon& current, const jf::Weapon& hovered, std::vector<std::string>& tactics,
                     std::vector<std::string>& lost) {
    if (hovered.might != current.might) {
        int delta = hovered.might - current.might;
        tactics.push_back(tr("ui.unit_screen.diff.might_prefix") + (delta > 0 ? "+" : "") + std::to_string(delta));
    }
    if (hovered.minRange != current.minRange || hovered.maxRange != current.maxRange) {
        tactics.push_back(tr("ui.unit_screen.diff.range_prefix") + weaponRangeText(current) + " -> " +
                          weaponRangeText(hovered));
    }
    if (hovered.moveModifier != current.moveModifier) {
        int delta = hovered.moveModifier - current.moveModifier;
        tactics.push_back(tr("ui.unit_screen.diff.move_mod_prefix") + (delta > 0 ? "+" : "") + std::to_string(delta));
    }
    const std::vector<std::string> currentTags = weaponEffectTags(current);
    const std::vector<std::string> hoveredTags = weaponEffectTags(hovered);
    std::vector<std::string> gained;
    for (const std::string& tag : hoveredTags)
        if (std::find(currentTags.begin(), currentTags.end(), tag) == currentTags.end()) gained.push_back(tag);
    if (!gained.empty()) {
        std::string joined = tr("ui.unit_screen.diff.gain_prefix");
        for (std::size_t i = 0; i < gained.size(); ++i) joined += (i ? ", " : "") + gained[i];
        tactics.push_back(joined);
    }
    for (const std::string& tag : currentTags)
        if (std::find(hoveredTags.begin(), hoveredTags.end(), tag) == hoveredTags.end())
            lost.push_back(tr("ui.unit_screen.diff.lose_effect", {{"value", tag}}));
}

// M10-E: armor's DEF/RES summary/diff, mirroring weaponSummaryLine()/
// weaponDiffLines() above. Scope note: like the weapon panel (which diffs
// jf::Weapon::might, the branch's BASE value, not the currently-strengthened
// weapon's Lv-adjusted number), this compares armor's base Tier DEF/RES
// (ArmorDefinition::baseDef/baseRes) rather than each armor's own current
// Lv-adjusted value via armorDefBonusAtLevel()/armorResBonusAtLevel() -
// matching the weapon panel's existing scope rather than introducing a new,
// inconsistent Lv-aware display for only one of the two equipment panels.
std::string armorSummaryLine(const jf::ArmorDefinition& armor) {
    return tr("ui.unit_screen.diff.def_prefix") + std::to_string(armor.baseDef) + "   " +
           tr("ui.unit_screen.diff.res_prefix") + std::to_string(armor.baseRes);
}

// "変わる戦術": DEF/RES deltas (both directions). "失うもの" is always empty for
// armor - unlike weapons, ArmorDefinition carries no qualitative effect tags
// (docs/deep_layers.md「防具システム(新設)」: "武器分岐のような固有戦闘効果
// ...は防具には持たせない"), so there is nothing an armor swap can take away
// beyond the DEF/RES numbers already reflected in tactics.
void armorDiffLines(const jf::ArmorDefinition& current, const jf::ArmorDefinition& hovered,
                    std::vector<std::string>& tactics, std::vector<std::string>& /*lost*/) {
    if (hovered.baseDef != current.baseDef) {
        int delta = hovered.baseDef - current.baseDef;
        tactics.push_back(tr("ui.unit_screen.diff.def_prefix") + (delta > 0 ? "+" : "") + std::to_string(delta));
    }
    if (hovered.baseRes != current.baseRes) {
        int delta = hovered.baseRes - current.baseRes;
        tactics.push_back(tr("ui.unit_screen.diff.res_prefix") + (delta > 0 ? "+" : "") + std::to_string(delta));
    }
}

// Renders the 4-block "現在/変更後/変わる戦術/失うもの" panel
// (docs/character_progression.md「ユニットページ/詳細」) for whichever
// weapon/skill candidate is currently hovered. Long text wraps rather than
// relying on a hover-only tooltip, per the doc's own accessibility note.
void drawEquipmentDiffPanel(jf::GameApp& app, const EquipmentHover& hover, Rectangle panel) {
    drawCard(panel, kColorCard, kColorBorderSoft, 0.04f);
    drawSectionHeading(tr("ui.unit_screen.diff.heading"), static_cast<int>(panel.x + 22),
                       static_cast<int>(panel.y + 16), 18);

    std::string currentSummary;
    std::string afterSummary;
    std::vector<std::string> tactics;
    std::vector<std::string> lost;

    if (hover.kind == EquipmentHoverKind::Weapon) {
        const jf::Weapon& current = app.gameData().weaponsById.at(hover.currentWeaponId);
        const jf::Weapon& hovered = app.gameData().weaponsById.at(hover.hoveredWeaponId);
        currentSummary = weaponNameFor(hover.currentWeaponId, weaponEnglishName(hover.currentWeaponId)) + "\n" +
                         weaponSummaryLine(current);
        afterSummary = weaponNameFor(hover.hoveredWeaponId, weaponEnglishName(hover.hoveredWeaponId)) + "\n" +
                       weaponSummaryLine(hovered);
        weaponDiffLines(current, hovered, tactics, lost);
    } else if (hover.kind == EquipmentHoverKind::Armor) {
        const jf::ArmorDefinition* current =
            hover.currentArmorId.empty() ? nullptr : jf::findArmorDefinition(hover.currentArmorId);
        const jf::ArmorDefinition* hovered = jf::findArmorDefinition(hover.hoveredArmorId);
        currentSummary = current ? pick(current->nameEn, current->nameJa) + "\n" + armorSummaryLine(*current)
                                 : tr("ui.facilities.armor_none");
        if (hovered) {
            afterSummary = pick(hovered->nameEn, hovered->nameJa) + "\n" + armorSummaryLine(*hovered);
            if (current) {
                armorDiffLines(*current, *hovered, tactics, lost);
            } else {
                // No armor equipped yet: nothing to subtract from, so the
                // whole candidate's DEF/RES shows as a straight gain rather
                // than a delta against a nonexistent "current".
                tactics.push_back(armorSummaryLine(*hovered));
            }
        }
    } else {
        const jf::SkillDefinition* current = hover.currentSkillId.empty() ? nullptr : jf::findSkill(hover.currentSkillId);
        const jf::SkillDefinition* hovered = jf::findSkill(hover.hoveredSkillId);
        // docs/implementation_status.md「装備差分/帰還UIの追加文言レビュー」#2:
        // category/usage-limit used to only show up here as a "changed to X"
        // line when it actually differed from the currently-equipped skill,
        // so an empty slot or a same-category swap showed neither - the
        // player had to hover a second time (skillTooltipLines()'s own
        // popup) just to see how many times a skill can be used. Now always
        // shown alongside the effect text for both slots.
        auto skillMeta = [](const jf::SkillDefinition& def) {
            return skillCategoryNameFor(def.category) + "  /  " + skillUsageNameFor(def.usageType);
        };
        currentSummary = current ? pick(current->nameEn, current->nameJa) + "\n" + skillMeta(*current) + "\n" +
                                       tr("ui.unit_screen.diff.skill_effect_prefix") + pick(current->effectEn, current->effectJa)
                                 : tr("ui.unit_screen.skill_slot_empty");
        if (hovered) {
            afterSummary = pick(hovered->nameEn, hovered->nameJa) + "\n" + skillMeta(*hovered) + "\n" +
                           tr("ui.unit_screen.diff.skill_effect_prefix") + pick(hovered->effectEn, hovered->effectJa);
            tactics.push_back(skillMeta(*hovered));
            tactics.push_back(tr("ui.unit_screen.diff.skill_effect_prefix") + pick(hovered->effectEn, hovered->effectJa));
            if (current)
                lost.push_back(tr("ui.unit_screen.diff.skill_lose", {{"value", pick(current->nameEn, current->nameJa)}}));
        }
    }

    // 2x2 grid (現在/変更後 on top, 変わる戦術/失うもの below) so all 4 blocks fit
    // the panel's fixed height without depending on how much text each has -
    // each quadrant wraps and clips independently instead of pushing later
    // blocks off the card.
    const float colWidth = (panel.width - 3 * 22.0f) / 2.0f;
    const float rowHeight = (panel.height - 40.0f - 16.0f) / 2.0f;
    const int maxWidth = static_cast<int>(colWidth - 8.0f);
    auto joinLines = [](const std::vector<std::string>& lines) {
        std::string joined;
        for (std::size_t i = 0; i < lines.size(); ++i) joined += (i ? "\n" : "") + lines[i];
        return joined;
    };
    auto drawBlock = [&](int col, int row, const std::string& label, const std::string& body) {
        const float bx = panel.x + 22.0f + static_cast<float>(col) * (colWidth + 22.0f);
        const float by = panel.y + 40.0f + static_cast<float>(row) * (rowHeight + 8.0f);
        drawText(label, static_cast<int>(bx), static_cast<int>(by), 13, kColorAccentGold);
        std::string wrapped = wrapTextToWidth(body.empty() ? tr("ui.unit_screen.diff.none") : body, 12, maxWidth);
        // Clips to the quadrant's line budget rather than letting an
        // overlong diff spill into the next block below it.
        std::vector<std::string> lines = textLines(wrapped);
        const int maxLines = std::max(1, static_cast<int>((rowHeight - 18.0f) / textLineHeight(12)));
        if (static_cast<int>(lines.size()) > maxLines) {
            lines.resize(static_cast<std::size_t>(maxLines));
            wrapped.clear();
            for (std::size_t i = 0; i < lines.size(); ++i) wrapped += (i ? "\n" : "") + lines[i];
        }
        drawText(wrapped, static_cast<int>(bx), static_cast<int>(by) + 18, 12, kColorTextPrimary);
    };

    drawBlock(0, 0, tr("ui.unit_screen.diff.current"), currentSummary);
    drawBlock(1, 0, tr("ui.unit_screen.diff.after"), afterSummary);
    drawBlock(0, 1, tr("ui.unit_screen.diff.tactics_change"), joinLines(tactics));
    drawBlock(1, 1, tr("ui.unit_screen.diff.lost"), joinLines(lost));
}

// Data-driven for all 12 classes: base weapon comes from the class
// definition, branch candidates come from any `craft_*` node registered for
// this class (docs/implementation_roadmap.md "M7項目3(残り) ...特性・武器
// 分岐の他兵種一般化"). Node id -> weapon id is the "craft_" prefix stripped,
// which matches every entry in facilityNodeRegistry() by construction.
std::optional<EquipmentHover> drawForgeEquipmentPanel(jf::GameApp& app, const jf::UnitTemplate& unit, float x,
                                                       float y, float width, Vector2 mouse, bool clicked,
                                                       std::vector<TooltipLine>& tooltipLines,
                                                       float* outBottomY = nullptr) {
    drawSectionHeading(tr("ui.forge.equipment_heading"), static_cast<int>(x),
                       static_cast<int>(y), 18);
    const jf::BaseState& base = app.baseState();
    const std::string baseWeaponId = app.gameData().classDefinition(unit.classId).weaponId;
    auto overrideIt = app.weaponOverrides().find(unit.id);
    std::string current = overrideIt != app.weaponOverrides().end() ? overrideIt->second : baseWeaponId;
    drawText(tr("ui.facilities.current_weapon_prefix") + weaponNameFor(current, weaponEnglishName(current)),
             static_cast<int>(x), static_cast<int>(y) + 30, 14, kColorTextMuted);

    struct Candidate { std::string id; std::string nodeId; };
    std::vector<Candidate> candidates = {{baseWeaponId, ""}};
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        // M10-B: "craft_armor_*" nodes also set weaponBranchClass (reused for
        // per-class Forge UI routing - see Facilities.hpp's own comment) but
        // must not appear in the weapon candidate list.
        if (node.id.rfind("craft_", 0) == 0 && node.id.rfind("craft_armor_", 0) != 0 &&
            node.weaponBranchClass == unit.classId)
            candidates.push_back({node.id.substr(6), node.id});
    }
    float by = y + 58;
    const float candidateWidth = (width - 12.0f) / 2.0f;
    std::optional<EquipmentHover> hover;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const Candidate& candidate = candidates[index];
        bool available = candidate.nodeId.empty() || base.unlockedNodeIds.count(candidate.nodeId) > 0;
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
        // Diff preview (docs/character_progression.md「ユニットページ/詳細」): any
        // hovered candidate other than the currently-equipped one, available or
        // not, previews what changes - previewing a not-yet-craftable branch is
        // useful too, so hover isn't gated on `available`.
        if (CheckCollisionPointRec(mouse, rect)) {
            if (candidate.id != current)
                hover = EquipmentHover{EquipmentHoverKind::Weapon, current, candidate.id, "", "", "", ""};
            auto weaponIt = app.gameData().weaponsById.find(candidate.id);
            if (weaponIt != app.gameData().weaponsById.end())
                tooltipLines = weaponTooltipLines(candidate.id, weaponIt->second, available);
        }
    }

    by += static_cast<float>((candidates.size() + 1) / 2) * 46.0f + 20.0f;
    // 装飾品(調整特性)候補一覧: ユーザー要望「もっと増やしたい」で単一トグル
    // ボタンから武器/防具候補と同じ「クリックで切替」グリッドに変更。装備中の
    // ものをクリックすると解除、それ以外をクリックすると切替。
    struct TraitCandidate { std::string nodeId; jf::TuningTraitId traitId; const char* nameKey; const char* tipKey; };
    static const std::array<TraitCandidate, 3> kTraitCandidates{
        TraitCandidate{"trait_hide_wrapped_grip", jf::TuningTraitId::HideWrappedGrip,
                       "ui.facilities.trait_name_hide_wrapped_grip", "ui.facilities.trait_tip_hide_wrapped_grip"},
        TraitCandidate{"trait_scorch_guard_wrap", jf::TuningTraitId::ScorchGuardWrap,
                       "ui.facilities.trait_name_scorch_guard_wrap", "ui.facilities.trait_tip_scorch_guard_wrap"},
        TraitCandidate{"trait_charm_of_might", jf::TuningTraitId::CharmOfMight,
                       "ui.facilities.trait_name_charm_of_might", "ui.facilities.trait_tip_charm_of_might"},
    };
    auto equippedTraitIt = app.equippedTraits().find(unit.id);
    jf::TuningTraitId equippedTraitId =
        equippedTraitIt != app.equippedTraits().end() ? equippedTraitIt->second : jf::TuningTraitId::None;
    {
        std::string statusLabel = tr("ui.facilities.armor_none");
        for (const TraitCandidate& c : kTraitCandidates) {
            if (c.traitId == equippedTraitId) {
                statusLabel = tr(c.nameKey);
                break;
            }
        }
        drawText(tr("ui.facilities.current_trait_prefix") + statusLabel, static_cast<int>(x),
                 static_cast<int>(by), 13, kColorTextMuted);
    }
    by += 24.0f;
    // 3列並びで1行に収める(2列だと3個目が2行目に落ち、パネル下のスキル欄と
    // 干渉するため - 見切れバグの再発防止)。
    const float traitCandidateWidth = (width - 16.0f) / 3.0f;
    for (std::size_t index = 0; index < kTraitCandidates.size(); ++index) {
        const TraitCandidate& candidate = kTraitCandidates[index];
        bool candidateUnlocked = base.unlockedNodeIds.count(candidate.nodeId) > 0;
        Rectangle rect{x + static_cast<float>(index) * (traitCandidateWidth + 8.0f), by, traitCandidateWidth, 34};
        std::string label = tr(candidate.nameKey);
        if (candidateUnlocked) {
            if (button(rect, label, mouse, clicked)) {
                app.equipTuningTraitForUnit(
                    unit.id, equippedTraitId == candidate.traitId ? jf::TuningTraitId::None : candidate.traitId);
            }
        } else {
            disabledButton(rect, label);
        }
        if (CheckCollisionPointRec(mouse, rect)) {
            if (const jf::FacilityNode* traitNode = jf::findFacilityNode(candidate.nodeId)) {
                tooltipLines = {{pick(traitNode->nameEn, traitNode->nameJa), kColorAccentGold, 16},
                               {pick(traitNode->effectEn, traitNode->effectJa), kColorTextPrimary, 13}};
                tooltipLines.push_back({tr(candidate.tipKey), kColorTextMuted, 12});
                if (!candidateUnlocked)
                    tooltipLines.push_back({tr("ui.facilities.trait_locked"), Color{225, 120, 120, 255}, 12});
            }
        }
    }
    by += 34.0f + 20.0f;

    // M10-B (docs/deep_layers.md「防具システム(新設)」): armor equip panel,
    // mirroring the weapon panel above but simpler - armor has no "class base
    // item" exemption (an empty armorOverrides() entry means no armor
    // equipped at all, not "wearing a default"), so there's no synthetic
    // "current" candidate the way baseWeaponId is above.
    by += 46.0f;
    drawSectionHeading(tr("ui.facilities.armor_heading"), static_cast<int>(x), static_cast<int>(by), 18);
    auto armorOverrideIt = app.armorOverrides().find(unit.id);
    std::string currentArmorId = armorOverrideIt != app.armorOverrides().end() ? armorOverrideIt->second : "";
    const jf::ArmorDefinition* currentArmor =
        currentArmorId.empty() ? nullptr : jf::findArmorDefinition(currentArmorId);
    std::string currentArmorLabel = currentArmor ? pick(currentArmor->nameEn, currentArmor->nameJa)
                                                 : tr("ui.facilities.armor_none");
    drawText(tr("ui.facilities.current_armor_prefix") + currentArmorLabel, static_cast<int>(x),
             static_cast<int>(by) + 26, 14, kColorTextMuted);

    std::vector<const jf::ArmorDefinition*> armorCandidates;
    for (const jf::ArmorDefinition& armor : jf::armorRegistry())
        if (armor.unitClass == unit.classId) armorCandidates.push_back(&armor);

    by += 54.0f;
    for (std::size_t index = 0; index < armorCandidates.size(); ++index) {
        const jf::ArmorDefinition* armor = armorCandidates[index];
        bool craftedUnlocked = base.unlockedNodeIds.count("craft_" + armor->id) > 0;
        Rectangle rect{x + (index % 2) * (candidateWidth + 12.0f), by + (index / 2) * 46.0f, candidateWidth, 34};
        std::string labelEn = armor->nameEn;
        std::string labelJa = armor->nameJa;
        if (craftedUnlocked) {
            if (button(rect, labelEn, labelJa, mouse, clicked)) app.equipArmorForUnit(unit.id, armor->id);
        } else {
            disabledButton(rect, pick(labelEn, labelJa));
        }
        // M10-E: diff preview, mirroring the weapon panel's hover rule above -
        // any hovered candidate other than the currently-equipped armor
        // previews it, available or not.
        if (CheckCollisionPointRec(mouse, rect)) {
            if (armor->id != currentArmorId)
                hover = EquipmentHover{EquipmentHoverKind::Armor, "", "", "", "", currentArmorId, armor->id};
            tooltipLines = armorTooltipLines(*armor, base.armorLevel(armor->id), craftedUnlocked);
        }
    }
    by += static_cast<float>((armorCandidates.size() + 1) / 2) * 46.0f + 20.0f;

    struct ArmorTraitCandidate {
        std::string nodeId;
        jf::ArmorTuningTraitId traitId;
        const char* nameKey;
        const char* tipKey;
    };
    static const std::array<ArmorTraitCandidate, 3> kArmorTraitCandidates{
        ArmorTraitCandidate{"armor_trait_ward_step", jf::ArmorTuningTraitId::WardStep,
                           "ui.facilities.trait_name_ward_step", "ui.facilities.trait_tip_ward_step"},
        ArmorTraitCandidate{"armor_trait_sturdy_sash", jf::ArmorTuningTraitId::SturdySash,
                           "ui.facilities.trait_name_sturdy_sash", "ui.facilities.trait_tip_sturdy_sash"},
        ArmorTraitCandidate{"armor_trait_vital_charm", jf::ArmorTuningTraitId::VitalCharm,
                           "ui.facilities.trait_name_vital_charm", "ui.facilities.trait_tip_vital_charm"},
    };
    auto equippedArmorTraitIt = app.equippedArmorTraits().find(unit.id);
    jf::ArmorTuningTraitId equippedArmorTraitId = equippedArmorTraitIt != app.equippedArmorTraits().end()
                                                      ? equippedArmorTraitIt->second
                                                      : jf::ArmorTuningTraitId::None;
    {
        std::string statusLabel = tr("ui.facilities.armor_none");
        for (const ArmorTraitCandidate& c : kArmorTraitCandidates) {
            if (c.traitId == equippedArmorTraitId) {
                statusLabel = tr(c.nameKey);
                break;
            }
        }
        drawText(tr("ui.facilities.current_trait_prefix") + statusLabel, static_cast<int>(x),
                 static_cast<int>(by), 13, kColorTextMuted);
    }
    by += 24.0f;
    for (std::size_t index = 0; index < kArmorTraitCandidates.size(); ++index) {
        const ArmorTraitCandidate& candidate = kArmorTraitCandidates[index];
        bool candidateUnlocked = base.unlockedNodeIds.count(candidate.nodeId) > 0;
        Rectangle rect{x + static_cast<float>(index) * (traitCandidateWidth + 8.0f), by, traitCandidateWidth, 34};
        std::string label = tr(candidate.nameKey);
        if (candidateUnlocked) {
            if (button(rect, label, mouse, clicked)) {
                app.equipArmorTraitForUnit(unit.id, equippedArmorTraitId == candidate.traitId
                                                       ? jf::ArmorTuningTraitId::None
                                                       : candidate.traitId);
            }
        } else {
            disabledButton(rect, label);
        }
        if (CheckCollisionPointRec(mouse, rect)) {
            if (const jf::FacilityNode* armorTraitNode = jf::findFacilityNode(candidate.nodeId)) {
                tooltipLines = {{pick(armorTraitNode->nameEn, armorTraitNode->nameJa), kColorAccentGold, 16},
                               {pick(armorTraitNode->effectEn, armorTraitNode->effectJa), kColorTextPrimary, 13}};
                tooltipLines.push_back({tr(candidate.tipKey), kColorTextMuted, 12});
                if (!candidateUnlocked)
                    tooltipLines.push_back({tr("ui.facilities.armor_trait_locked"), Color{225, 120, 120, 255}, 12});
            }
        }
    }
    by += 34.0f;

    if (outBottomY) *outBottomY = by;
    return hover;
}

// docs/character_progression.md「ユニットページ」詳細5「装備スキル2枠」: unlike the
// weapon/trait panel above, this works for any class - GameApp::equipSkillForUnit()
// was already class-generic (only Spearman's *UI* was missing). The diff/preview
// panel, cooperation tactics, and exploration-ability sections from the full spec
// are out of scope for this Slice; only the 2 skill slots are wired here.
std::optional<EquipmentHover> drawSkillEquipmentPanel(jf::GameApp& app, const jf::UnitTemplate& unit, float x,
                                                       float y, float width, Vector2 mouse, bool clicked,
                                                       std::vector<TooltipLine>& tooltipLines) {
    drawSectionHeading(tr("ui.unit_screen.skills_heading"), static_cast<int>(x), static_cast<int>(y), 18);
    const jf::BaseState& base = app.baseState();
    const std::vector<const jf::SkillDefinition*> classSkills = jf::skillsForClass(unit.classId);
    const std::string requiredNode = jf::requiredTrainingNodeIdFor(unit.classId);
    const bool trainingUnlocked = !requiredNode.empty() && base.unlockedNodeIds.count(requiredNode) > 0;

    auto slotIt = app.equippedSkills().find(unit.id);
    std::array<std::string, 2> equipped{"", ""};
    if (slotIt != app.equippedSkills().end()) equipped = slotIt->second.equippedSkillIds;

    std::optional<EquipmentHover> hover;
    // docs/implementation_status.md「日本語UI文言/スキル説明レビュー」#3: the
    // 4-block diff panel below clips long effect text to its quadrant's fixed
    // line budget (e.g. Emergency Treatment's full sentence got cut off
    // mid-condition). A mouse-follow tooltip has no such size limit, so show
    // the full skillTooltipLines() (same component the Battle skill menu
    // uses) alongside the diff panel rather than trying to fit everything
    // into the fixed quadrant.
    const float candidateWidth = (width - 3 * 10.0f) / 4.0f;
    for (int slot = 0; slot < 2; ++slot) {
        float slotY = y + 30 + static_cast<float>(slot) * 70.0f;
        const jf::SkillDefinition* currentDef = equipped[slot].empty() ? nullptr : jf::findSkill(equipped[slot]);
        std::string currentLabel = currentDef ? pick(currentDef->nameEn, currentDef->nameJa) : tr("ui.unit_screen.skill_slot_empty");
        drawText(tr("ui.unit_screen.skill_slot_prefix") + std::to_string(slot + 1) + ": " + currentLabel,
                 static_cast<int>(x), static_cast<int>(slotY), 14, kColorTextMuted);

        for (std::size_t index = 0; index < classSkills.size() && index < 3; ++index) {
            const jf::SkillDefinition* skill = classSkills[index];
            // equipSkillForUnit() requires the training branch for every
            // tier, including Tier1 - only the auto-equip-at-join path
            // (confirmRecruitJoin()/roster init) bypasses it.
            bool available = trainingUnlocked;
            // A skill already equipped in the other slot can't also go here -
            // equipSkillForUnit() only checks per-slot validity, so the UI
            // enforces no-duplicate-across-slots itself (see plan Context).
            bool equippedElsewhere = equipped[1 - slot] == skill->id;
            Rectangle rect{x + static_cast<float>(index) * (candidateWidth + 10.0f), slotY + 24, candidateWidth, 34};
            std::string labelEn = skill->nameEn;
            std::string labelJa = skill->nameJa;
            if (available && !equippedElsewhere) {
                if (button(rect, labelEn, labelJa, mouse, clicked)) app.equipSkillForUnit(unit.id, slot, skill->id);
            } else {
                disabledButton(rect, pick(labelEn, labelJa));
            }
            // Diff preview, mirroring the weapon panel's hover rule above -
            // hovering a not-yet-available candidate (locked training branch,
            // or already equipped in the other slot) still previews it.
            if (skill->id != equipped[slot] && CheckCollisionPointRec(mouse, rect)) {
                hover = EquipmentHover{EquipmentHoverKind::Skill, "", "", equipped[slot], skill->id, "", ""};
                std::string reason = equippedElsewhere  ? tr("ui.unit_screen.skill_equipped_elsewhere")
                                     : !trainingUnlocked ? tr("ui.unit_screen.needs_training")
                                                        : "";
                tooltipLines = skillTooltipLines(*skill, available && !equippedElsewhere, reason, reason);
            }
        }
        Rectangle clearRect{x + 3 * (candidateWidth + 10.0f), slotY + 24, candidateWidth, 34};
        if (equipped[slot].empty()) {
            disabledButton(clearRect, tr("ui.unit_screen.skill_slot_clear"));
        } else if (button(clearRect, tr("ui.unit_screen.skill_slot_clear"), mouse, clicked)) {
            app.equipSkillForUnit(unit.id, slot, "");
        }
    }

    return hover;
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

    const std::string explorationAbility = explorationAbilityFor(unit->classId);
    if (!explorationAbility.empty()) {
        drawSectionHeading(tr("ui.unit_screen.exploration_ability_heading"), 72, 460, 18);
        drawText(wrapTextToWidth(explorationAbility, 14, 400), 72, 494, 14, kColorTextMuted);
    }

    // Equipment panels are drawn ahead of the comparison/diff region below so
    // their hover state (docs/character_progression.md「ユニットページ/詳細」
    // "武器、特性、スキルを選ぶと...") is known before deciding what to show there.
    // Drawing order doesn't affect the two cards' on-screen position (they
    // occupy disjoint rectangles), only which data is ready in time.
    Rectangle equipment{548, 104, 690, 684};
    drawCard(equipment, kColorCard, kColorBorderSoft, 0.04f);
    std::optional<EquipmentHover> hover;
    std::vector<TooltipLine> tooltipLines;
    // 武器/防具の装飾品候補が増えたことで武器・防具パネルの実高さがクラス次第で
    // 伸びるようになった(防具3種持ちのクラス等) - 下の装備スキル欄を固定y=608に
    // 置いたままだと重なって見切れるため、実際の描画下端をdrawForgeEquipmentPanel()
    // から受け取って動的に配置する(見切れバグの再発防止)。
    float skillPanelY = 608.0f;
    if (classHasWeaponBranchRecipes(unit->classId)) {
        float forgeBottomY = 0.0f;
        hover = drawForgeEquipmentPanel(app, *unit, 580, 136, 626, mouse, clicked, tooltipLines, &forgeBottomY);
        skillPanelY = std::max(skillPanelY, forgeBottomY + 20.0f);
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
    std::optional<EquipmentHover> skillHover =
        drawSkillEquipmentPanel(app, *unit, 580, skillPanelY, 626, mouse, clicked, tooltipLines);
    if (!hover) hover = skillHover;

    // docs/character_progression.md「ユニットページ」一覧「比較対象を1人固定できる」:
    // side-by-side numbers only, no green/red-only diff coloring (spec's own
    // "能力値差を緑赤だけで表現しない"). Silently drops a stale pin (e.g. the
    // pinned Unit is no longer in roster()) instead of showing a broken row.
    // Shares its screen region with the diff-preview panel below (both are
    // transient view aids, and only one is relevant at a time): the diff
    // preview wins whenever a candidate is actively hovered.
    if (!hover && gBaseScreen.comparisonUnitId && *gBaseScreen.comparisonUnitId != unit->id) {
        auto comparisonUnit = std::find_if(app.roster().begin(), app.roster().end(),
                                           [&](const jf::UnitTemplate& candidate) {
                                               return candidate.id == *gBaseScreen.comparisonUnitId;
                                           });
        if (comparisonUnit == app.roster().end()) {
            gBaseScreen.comparisonUnitId.reset();
        } else {
            Rectangle comparisonCard{42, 620, 470, 150};
            drawCard(comparisonCard, kColorCard, kColorBorderSoft, 0.04f);
            drawSectionHeading(tr("ui.unit_screen.comparison_heading"), 72, 636, 18);
            drawText(clipTextToWidth(unitDisplayNameFor(comparisonUnit->name) + "  " +
                         classNameFor(app.gameData(), comparisonUnit->classId), 18, 400),
                     72, 670, 18, kColorAccentGold);
            const jf::Stats& comparisonStats = app.gameData().classDefinition(comparisonUnit->classId).baseStats;
            drawText(clipTextToWidth("HP " + std::to_string(stats.maxHp) + " (" + tr("ui.unit_screen.comparison_prefix") +
                         std::to_string(comparisonStats.maxHp) + ")    STR " + std::to_string(stats.strength) +
                         " (" + std::to_string(comparisonStats.strength) + ")    MAG " +
                         std::to_string(stats.magic) + " (" + std::to_string(comparisonStats.magic) + ")", 15, 400),
                     72, 710, 15, kColorTextPrimary);
            drawText(clipTextToWidth("DEF " + std::to_string(stats.defense) + " (" + std::to_string(comparisonStats.defense) +
                         ")    RES " + std::to_string(stats.resistance) + " (" +
                         std::to_string(comparisonStats.resistance) + ")    MOV " + std::to_string(stats.move) +
                         " (" + std::to_string(comparisonStats.move) + ")", 15, 400),
                     72, 742, 15, kColorTextPrimary);
        }
    }

    if (hover) {
        Rectangle diffPanel{42, 620, 470, 150};
        drawEquipmentDiffPanel(app, *hover, diffPanel);
    }
    if (!tooltipLines.empty()) drawTooltipBox(mouse, tooltipLines);
}

// The facility card grid (list view, shown when no facility is visited
// yet). Split out of drawFacilitiesScreen(); no behavior change.
void drawFacilitiesList(jf::GameApp& app, Vector2 mouse, bool clicked, const jf::BaseState& base,
                         const Rectangle& backRect) {
    drawText(tr("ui.facilities.title"), 38, 24, 28, kColorTextPrimary);
    drawText(outpostStageNameFor(base.outpostStage()), 38, 60, 16, kColorTextMuted);
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
        // 「解放済み分岐 N」は削除(ユーザー指摘「91とか4とかって何？」):
        // クラフト/装飾品ノードも1件ずつ数えた素の合計で、装飾品拡張
        // (docs/implementation_status.md「装飾品拡張メモ」)以降は91のような
        // 大きい数字になり、意味のある「分岐」進捗として読めなくなっていた。
        const bool constructed = facilityIsConstructed(base, facility);
        drawText(constructed ? tr("ui.facility_node.constructed") : tr("ui.facility_node.not_constructed"),
                 static_cast<int>(card.x + 22), static_cast<int>(card.y + 59), 14,
                 constructed ? Color{105, 205, 145, 255} : Color{180, 125, 125, 255});
        Rectangle visitRect{card.x + card.width - 174.0f, card.y + 82.0f, 150.0f, 40.0f};
        if (button(visitRect, "Visit", "訪れる", mouse, clicked)) {
            gBaseScreen.visitedFacility = facility;
            gBaseScreen.forgeCraftClass.reset();
            gBaseScreen.facilityNodeScroll = 0.0f;
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
    // 「解放済み分岐 N」の表示は削除(ユーザー指摘「91とか4とかって何？」、
    // 上のdrawFacilitiesList()と同じ理由)。role文が multi-line
    // (embedded "\n", e.g. facility.command_post.role) なので
    // その分の縦スペースは維持する。
    // wrapped to the available width; the section heading/node list below it
    // are pushed down by the actual wrapped line count (same "measure, then
    // offset" shape as drawBaseRegionList()'s dynamic bottom-Y fix) instead
    // of a fixed gap, so a longer role description never collides with the
    // node list header either.
    const std::string wrappedRole = wrapTextToWidth(facilityRoleFor(facility), 14, 690);
    drawText(wrappedRole, 38, 90, 14, kColorTextMuted);
    const int roleLineCount = static_cast<int>(textLines(wrappedRole).size());
    const float headingY = 90.0f + static_cast<float>(roleLineCount) * 22.0f + 20.0f;
    if (forgeCraftPage) {
        if (button(backRect, tr("ui.forge.back_to_forge"), mouse, clicked)) {
            gBaseScreen.forgeCraftClass.reset();
            gBaseScreen.facilityNodeScroll = 0.0f;
        }
    } else if (button(backRect, tr("ui.facilities.back_to_list"), mouse, clicked)) {
        gBaseScreen.visitedFacility.reset();
        gBaseScreen.facilityNodeScroll = 0.0f;
    }

    drawSectionHeading(forgeCraftPage ? tr("ui.forge.recipes") : tr("ui.forge.upgrades"),
                       42, static_cast<int>(headingY), 18);
    std::vector<const jf::FacilityNode*> visibleNodes;
    for (const jf::FacilityNode& node : jf::facilityNodeRegistry()) {
        if (node.facility != facility) continue;
        const bool isWeaponRecipe = node.id.rfind("craft_", 0) == 0;
        if (facility == jf::FacilityId::Forge) {
            if (forgeCraftPage && (!isWeaponRecipe || node.weaponBranchClass != *gBaseScreen.forgeCraftClass)) continue;
            if (!forgeCraftPage && isWeaponRecipe) continue;
        }
        visibleNodes.push_back(&node);
    }

    const jf::FacilityNode* hoveredNode = nullptr;
    constexpr float kNodeRowStep = 44.0f;
    constexpr float kNodeRowHeight = 38.0f;
    const Rectangle nodeViewport{36.0f, headingY + 37.0f, 696.0f,
                                 static_cast<float>(kScreenHeight) - (headingY + 37.0f) - 24.0f};
    const int visibleRowCount = std::max(1, static_cast<int>(nodeViewport.height / kNodeRowStep));
    const int maxFirstRow = std::max(0, static_cast<int>(visibleNodes.size()) - visibleRowCount);
    if (CheckCollisionPointRec(mouse, nodeViewport)) {
        gBaseScreen.facilityNodeScroll =
            std::clamp(gBaseScreen.facilityNodeScroll - GetMouseWheelMove() * 2.0f,
                       0.0f, static_cast<float>(maxFirstRow));
    } else {
        gBaseScreen.facilityNodeScroll =
            std::clamp(gBaseScreen.facilityNodeScroll, 0.0f, static_cast<float>(maxFirstRow));
    }

    DrawRectangleRec(nodeViewport, Color{18, 21, 30, 255});
    const int firstRow = static_cast<int>(gBaseScreen.facilityNodeScroll);
    const int endRow = std::min(static_cast<int>(visibleNodes.size()), firstRow + visibleRowCount);
    float nodeY = nodeViewport.y + 5.0f;
    for (int row = firstRow; row < endRow; ++row) {
        const jf::FacilityNode& node = *visibleNodes[static_cast<std::size_t>(row)];
        Rectangle rowPanel{36.0f, nodeY - 5.0f, 696.0f, kNodeRowHeight};
        DrawRectangleRec(rowPanel, Color{25, 30, 42, 255});
        drawFacilityNodeRow(app, node, 48.0f, nodeY, 672.0f, mouse, clicked);
        if (CheckCollisionPointRec(mouse, rowPanel)) hoveredNode = &node;
        nodeY += kNodeRowStep;
    }
    if (maxFirstRow > 0) {
        const float thumbHeight = std::max(34.0f, nodeViewport.height * visibleRowCount /
                                                    static_cast<float>(visibleNodes.size()));
        const float thumbTrack = nodeViewport.height - thumbHeight;
        const float thumbY = nodeViewport.y + thumbTrack * (gBaseScreen.facilityNodeScroll /
                                                            static_cast<float>(maxFirstRow));
        DrawRectangleRounded(Rectangle{nodeViewport.x + nodeViewport.width + 8.0f, nodeViewport.y,
                                       5.0f, nodeViewport.height},
                             0.5f, 4, Color{38, 43, 55, 180});
        DrawRectangleRounded(Rectangle{nodeViewport.x + nodeViewport.width + 8.0f, thumbY,
                                       5.0f, thumbHeight},
                             0.5f, 4, kColorBorder);
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
            jf::UnitClass::HeavyInfantry, jf::UnitClass::FrontierEngineer, jf::UnitClass::MessengerCavalry,
            jf::UnitClass::FrontierRanger, jf::UnitClass::BannerBearer, jf::UnitClass::BattleMage,
        };
        for (int index = 0; index < 12; ++index) {
            // 2 columns x 6 rows so all 12 classes fit inside the existing
            // info panel (it only had room for 6 single-column rows before).
            const int col = index / 6;
            const int row = index % 6;
            Rectangle craftRect{794.0f + col * 202.0f, 232.0f + row * 58.0f, 198.0f, 46.0f};
            const std::string label = tr("ui.forge.craft_prefix") + classNameFor(app.gameData(), craftClasses[index]);
            // Any class with at least one registered craft_* recipe (all 12,
            // now that weapon branches are generalized) routes through the
            // real crafting panel instead of the "(未実装)" fallback.
            if (classHasWeaponBranchRecipes(craftClasses[index])) {
                if (button(craftRect, label, mouse, clicked)) {
                    gBaseScreen.forgeCraftClass = craftClasses[index];
                    gBaseScreen.facilityNodeScroll = 0.0f;
                }
            } else {
                disabledButton(craftRect, label + tr("ui.forge.craft_planned_suffix"));
            }
        }
    } else {
        // infoPanel is 470 wide starting at x=770; text starts at x=794, so
        // ~430px is actually available before the panel's own right edge -
        // neither string was wrapped to that width before, so a long one
        // (ui.forge.hover_hint in particular) ran past the panel and off the
        // window edge instead of wrapping inside it.
        constexpr int kInfoPanelTextWidth = 430;
        drawText(wrapTextToWidth(forgeCraftPage ? tr("ui.forge.crafted_weapons_note")
                                                : facilityRoleFor(facility),
                                 14, kInfoPanelTextWidth),
                 794, 192, 14, kColorTextPrimary);
        drawText(wrapTextToWidth(tr("ui.forge.hover_hint"), 14, kInfoPanelTextWidth),
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
