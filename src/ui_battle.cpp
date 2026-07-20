// Battle screen rendering: board tiles, unit sprites, HUD, combat preview
// popups, and the attack-lunge/message animation layers. Split out of
// main.cpp; no behavior change.
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "jf/battle/BattleController.hpp"
#include "jf/core/GameApp.hpp"
#include "jf/core/Grid.hpp"
#include "jf/data/GameData.hpp"
#include "ui_battle.hpp"
#include "ui_shared.hpp"

namespace jfui {

bool gBattleItemMenuOpen = false;
bool gBattleSkillMenuOpen = false;

bool containsTile(const std::vector<jf::GridPos>& tiles, jf::GridPos pos) {
    for (const auto& t : tiles) {
        if (t == pos) return true;
    }
    return false;
}

Vector2 unitLogicalCenter(const jf::Unit& unit) {
    Rectangle rect = tileRect(unit.position);
    return Vector2{rect.x + rect.width / 2.0f, rect.y - kUnitRadius + 9.0f};
}

// Smoothly slides each unit's on-screen position toward its logical grid
// tile instead of snapping there instantly. Battle logic (BattleState /
// BattleController) still moves units in a single instant step - this is a
// purely visual interpolation layer keyed by unit id, so it never affects
// move ranges, hit detection, or turn timing.
// Attack lunge: a brief, purely-visual "body slam" toward the target and
// back, triggered off BattleController::attackEventId() (see drawGrid).
// Never touches battle logic/positions - only offsets where the unit is
// drawn for a fraction of a second.
struct AttackLunge {
    Vector2 direction{0.0f, 0.0f};
    float timer = 0.0f;
};

constexpr float kAttackLungeDuration = 0.26f;
constexpr float kAttackLungeDistance = 18.0f;

std::unordered_map<std::string, AttackLunge>& attackLunges() {
    static std::unordered_map<std::string, AttackLunge> lunges;
    return lunges;
}

void triggerAttackLunge(const jf::Unit& attacker, const jf::Unit& target) {
    Vector2 from = unitLogicalCenter(attacker);
    Vector2 to = unitLogicalCenter(target);
    Vector2 direction{to.x - from.x, to.y - from.y};
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0.001f) {
        direction.x /= length;
        direction.y /= length;
    }
    attackLunges()[attacker.id] = AttackLunge{direction, kAttackLungeDuration};
}

// Bump curve: 0 -> 1 -> 0 over the lunge's lifetime, so the unit lurches
// toward the target and eases back rather than snapping.
Vector2 attackLungeOffset(const std::string& unitId, float dt) {
    auto& lunges = attackLunges();
    auto it = lunges.find(unitId);
    if (it == lunges.end()) return Vector2{0.0f, 0.0f};

    AttackLunge& lunge = it->second;
    lunge.timer -= dt;
    if (lunge.timer <= 0.0f) {
        lunges.erase(it);
        return Vector2{0.0f, 0.0f};
    }
    float progress = 1.0f - (lunge.timer / kAttackLungeDuration);
    float bump = std::sin(progress * 3.14159265f);
    return Vector2{lunge.direction.x * kAttackLungeDistance * bump,
                  lunge.direction.y * kAttackLungeDistance * bump};
}

// Smoothly slides each unit's on-screen position toward its logical grid
// tile instead of snapping there instantly. Battle logic (BattleState /
// BattleController) still moves units in a single instant step - this is a
// purely visual interpolation layer keyed by unit id, so it never affects
// move ranges, hit detection, or turn timing.
Vector2 animatedUnitCenter(const jf::Unit& unit, float dt) {
    static std::unordered_map<std::string, Vector2> visualCenters;
    constexpr float kMoveLerpRate = 9.0f;

    Vector2 target = unitLogicalCenter(unit);
    Vector2 lunge = attackLungeOffset(unit.id, dt);
    auto it = visualCenters.find(unit.id);
    if (it == visualCenters.end()) {
        visualCenters.emplace(unit.id, target);
        return Vector2{target.x + lunge.x, target.y + lunge.y};
    }

    Vector2& current = it->second;
    float t = 1.0f - std::exp(-kMoveLerpRate * dt);
    current.x += (target.x - current.x) * t;
    current.y += (target.y - current.y) * t;
    return Vector2{current.x + lunge.x, current.y + lunge.y};
}


// Battle messages: brief fading banners for combat feedback (hit/miss/
// damage, a unit falling, a unit dropping to critical HP) triggered off the
// same BattleController::attackEventId() the lunge animation uses. Purely
// informational overlay text - never affects input handling or state.
struct BattleMessage {
    std::string text;
    Color color;
    float timer = 0.0f;
};

constexpr float kBattleMessageDuration = 1.8f;
constexpr float kBattleMessageFadeTime = 0.35f;
constexpr std::size_t kMaxBattleMessages = 3;

std::vector<BattleMessage>& battleMessages() {
    static std::vector<BattleMessage> messages;
    return messages;
}

void pushBattleMessage(const std::string& text, Color color) {
    auto& messages = battleMessages();
    messages.push_back({text, color, kBattleMessageDuration});
    if (messages.size() > kMaxBattleMessages) messages.erase(messages.begin());
}

void drawBattleMessages(float dt) {
    auto& messages = battleMessages();
    float y = 84.0f;
    for (auto it = messages.begin(); it != messages.end();) {
        it->timer -= dt;
        if (it->timer <= 0.0f) {
            it = messages.erase(it);
            continue;
        }
        float alpha = it->timer < kBattleMessageFadeTime ? (it->timer / kBattleMessageFadeTime) : 1.0f;
        int fontSize = 20;
        int textW = textWidth(it->text, fontSize);
        float x = (static_cast<float>(kScreenWidth) - textW) / 2.0f;
        Rectangle box{x - 16.0f, y, static_cast<float>(textW) + 32.0f, 34.0f};
        drawCard(box, withAlpha(kColorCard, static_cast<unsigned char>(220 * alpha)),
                withAlpha(kColorAccentGold, static_cast<unsigned char>(200 * alpha)), 0.24f);
        drawText(it->text, static_cast<int>(x), static_cast<int>(y + 7), fontSize,
                withAlpha(it->color, static_cast<unsigned char>(255 * alpha)));
        y += 40.0f;
        ++it;
    }
}

// Only warns once per drop below the threshold (tracked per unit id) so it
// doesn't repeat on every subsequent hit while a unit stays critical.
std::unordered_set<std::string>& lowHpWarnedUnits() {
    static std::unordered_set<std::string> warned;
    return warned;
}
constexpr float kLowHpRatioThreshold = 0.25f;

// Per-wave last-seen state, so a Scheduled -> Announced transition fires the
// banner exactly once (ReinforcementAnnouncedEvent itself isn't readable from
// here - BattleMissionState::consumedEventIds is a write-only dedup set with
// no stored payload, so this polls BattleState::reinforcementWaves() instead,
// the same technique lowHpWarnedUnits() above uses for attack events).
std::unordered_map<std::string, jf::ReinforcementState>& reinforcementUiStates() {
    static std::unordered_map<std::string, jf::ReinforcementState> states;
    return states;
}

std::string reinforcementAnnouncedMessageText(int spawnRound) {
    return tr("battle.reinforcement_announced", {{"round", std::to_string(spawnRound)}});
}

// Same technique as reinforcementUiStates() above, keyed by Boss Unit id -
// BossTelegraphChangedEvent has the same write-only consumedEventIds
// limitation, so this polls Unit::bossRuntime.telegraph.state instead.
std::unordered_map<std::string, jf::TelegraphState>& bossTelegraphUiStates() {
    static std::unordered_map<std::string, jf::TelegraphState> states;
    return states;
}

std::string bossChargeTelegraphedMessageText() {
    return tr("battle.boss_charge_telegraphed");
}

// Same technique as bossTelegraphUiStates() above, keyed by Boss Unit id -
// BossStageChangedEvent (fired the instant Unit::bossEnraged flips) has the
// same write-only consumedEventIds limitation, so this polls the field
// directly instead.
std::unordered_map<std::string, bool>& bossEnrageUiStates() {
    static std::unordered_map<std::string, bool> states;
    return states;
}

std::string bossEnragedMessageText() {
    return tr("battle.boss_enraged");
}

std::string hitMessageText(const std::string& attackerName, const std::string& targetName, int damage) {
    return tr("battle.hit_message",
             {{"attacker", attackerName}, {"target", targetName}, {"damage", std::to_string(damage)}});
}

std::string missMessageText(const std::string& attackerName, const std::string& targetName) {
    return tr("battle.miss_message", {{"attacker", attackerName}, {"target", targetName}});
}

std::string lowHpMessageText(const std::string& targetName) {
    return tr("battle.low_hp_message", {{"target", targetName}});
}

std::string fallenMessageText(const std::string& targetName) {
    return tr("battle.fallen_message", {{"target", targetName}});
}

std::string objectHitMessageText(const std::string& attackerName, const std::string& objectName, int damage) {
    return tr("battle.object_hit_message",
             {{"attacker", attackerName}, {"object", objectName}, {"damage", std::to_string(damage)}});
}

std::string objectDestroyedMessageText(const std::string& objectName) {
    return tr("battle.object_destroyed_message", {{"object", objectName}});
}

// Notices any battle-state transition that should surface as a one-shot
// banner/animation this frame (a resolved attack, a reinforcement wave's
// Scheduled->Announced, a Boss telegraph's None->Announced, a Boss crossing
// its enrage threshold) and returns the currently-pending Boss telegraph's
// danger-zone tiles for drawBoardTiles() to highlight. Split out of
// drawGrid() purely to separate "notice state changes" from "draw the
// board" - no behavior change.
std::vector<jf::GridPos> detectAndAnnounceBattleEvents(const jf::BattleController& controller) {
    const jf::BattleState& battle = controller.battle();

    // Detect a newly-resolved attack (player or enemy) and kick off its
    // lunge animation exactly once, whichever screen/state notices it first.
    static std::uint64_t lastSeenAttackEvent = 0;
    if (controller.attackEventId() != lastSeenAttackEvent) {
        lastSeenAttackEvent = controller.attackEventId();
        if (const jf::Unit* attacker = controller.lastAttacker()) {
            if (const jf::Unit* target = controller.lastAttackTarget()) {
                triggerAttackLunge(*attacker, *target);

                std::string attackerName = unitDisplayNameFor(attacker->name);
                std::string targetName = unitDisplayNameFor(target->name);
                if (controller.lastAttackHit()) {
                    pushBattleMessage(hitMessageText(attackerName, targetName, controller.lastDamage()),
                                      Color{255, 205, 120, 255});
                    if (!target->isAlive()) {
                        pushBattleMessage(fallenMessageText(targetName), Color{225, 90, 90, 255});
                        lowHpWarnedUnits().erase(target->id);
                    } else {
                        float hpRatio = static_cast<float>(target->currentHp) /
                                       static_cast<float>(target->stats.maxHp);
                        if (hpRatio <= kLowHpRatioThreshold && lowHpWarnedUnits().insert(target->id).second) {
                            pushBattleMessage(lowHpMessageText(targetName), Color{235, 140, 90, 255});
                        }
                    }
                } else {
                    pushBattleMessage(missMessageText(attackerName, targetName), Color{190, 198, 210, 255});
                }
            }
        }
    }

    // Detect a wave's Scheduled -> Announced transition and surface it as a
    // one-shot banner (see reinforcementUiStates() above for why polling,
    // not the event itself, is the read path).
    auto& waveStates = reinforcementUiStates();
    for (const jf::ReinforcementWave& wave : battle.reinforcementWaves()) {
        auto [it, inserted] = waveStates.try_emplace(wave.id, jf::ReinforcementState::Scheduled);
        if (it->second != jf::ReinforcementState::Announced && wave.state == jf::ReinforcementState::Announced) {
            pushBattleMessage(reinforcementAnnouncedMessageText(wave.spawnRound), Color{235, 190, 120, 255});
        }
        it->second = wave.state;
    }

    // Same idea for a Boss's telegraphed charge (docs/boss_common_rules.md's
    // "予告"): a None/Executed/Cancelled -> Announced transition fires the
    // banner once, and while pending() the telegraphed danger-zone tiles
    // (BossTelegraph::lockedTiles, computed at telegraph time in
    // EnemyAI.cpp's computeBoarChargeTiles()) get highlighted below.
    auto& telegraphStates = bossTelegraphUiStates();
    std::vector<jf::GridPos> telegraphDangerTiles;
    for (const jf::Unit& unit : battle.units()) {
        const jf::BossTelegraph& telegraph = unit.bossRuntime.telegraph;
        auto [it, inserted] = telegraphStates.try_emplace(unit.id, jf::TelegraphState::None);
        if (it->second != jf::TelegraphState::Announced && telegraph.state == jf::TelegraphState::Announced) {
            pushBattleMessage(bossChargeTelegraphedMessageText(), Color{235, 150, 60, 255});
        }
        it->second = telegraph.state;
        if (telegraph.pending())
            telegraphDangerTiles.insert(telegraphDangerTiles.end(), telegraph.lockedTiles.begin(),
                                       telegraph.lockedTiles.end());
    }

    // Same idea for a Boss crossing its enrage HP threshold
    // (docs/regions/ashbough_forest.md「HPが半分以下になると激昂し」): a
    // false -> true transition on Unit::bossEnraged fires a one-shot banner.
    // Unlike the telegraph above, this is a permanent stat change with no
    // tile of its own, so no highlight is drawn for it.
    auto& enrageStates = bossEnrageUiStates();
    for (const jf::Unit& unit : battle.units()) {
        auto [it, inserted] = enrageStates.try_emplace(unit.id, false);
        if (!it->second && unit.bossEnraged) {
            pushBattleMessage(bossEnragedMessageText(), Color{225, 90, 90, 255});
        }
        it->second = unit.bossEnraged;
    }

    return telegraphDangerTiles;
}

// The 3x8 board itself: terrain, Battle Objects, and every Tile-highlight
// overlay (movement/attack/heal/item/board/skill/interact ranges, the
// pending Boss charge danger zone). Split out of drawGrid(); no behavior
// change.
void drawBoardTiles(const jf::BattleController& controller, const std::vector<jf::GridPos>& telegraphDangerTiles) {
    const jf::BattleState& battle = controller.battle();

    drawBattlefieldBackdrop();

    for (int row = 0; row < jf::kGridRows; ++row) {
        for (int col = 0; col < jf::kGridCols; ++col) {
            jf::GridPos pos{row, col};
            Rectangle rect = tileRect(pos);
            jf::TerrainType terrain = battle.terrainAt(pos);
            drawTilePanel(rect, terrainColor(terrain));
            if (terrain == jf::TerrainType::Barrier) {
                DrawLineEx(Vector2{rect.x + 26.0f, rect.y + 12.0f},
                           Vector2{rect.x + rect.width - 26.0f, rect.y + rect.height - 18.0f}, 5.0f,
                           Color{25, 28, 34, 220});
                DrawLineEx(Vector2{rect.x + rect.width - 26.0f, rect.y + 12.0f},
                           Vector2{rect.x + 26.0f, rect.y + rect.height - 18.0f}, 5.0f,
                           Color{25, 28, 34, 220});
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
            } else if (terrain == jf::TerrainType::HerbPatch) {
                DrawCircle(static_cast<int>(rect.x + rect.width * 0.5f), static_cast<int>(rect.y + 22.0f), 7.0f,
                           Color{105, 190, 105, 230});
            }

            // Battle Objects are separate from Terrain. In particular, a
            // fallen log sits on a passable Floor tile but blocks movement;
            // it must remain visibly distinct so the board never presents a
            // flat-looking tile that rejects movement.
            if (const jf::BattleObjectState* object = battle.objectAt(pos);
                object && object->state != jf::BattleObjectStateKind::Destroyed) {
                const jf::BattleObjectDefinition* definition = battle.objectDefinition(object->definitionId);
                if (definition && definition->kind == jf::BattleObjectKind::Barrier) {
                    const float y = rect.y + rect.height * 0.42f;
                    DrawLineEx({rect.x + 16.0f, y + 4.0f}, {rect.x + rect.width - 16.0f, y - 4.0f},
                               13.0f, Color{82, 55, 38, 255});
                    DrawLineEx({rect.x + 19.0f, y + 1.0f}, {rect.x + rect.width - 19.0f, y - 7.0f},
                               4.0f, Color{151, 105, 63, 255});
                    DrawCircle(static_cast<int>(rect.x + 16.0f), static_cast<int>(y + 4.0f), 7.0f,
                               Color{56, 37, 28, 255});
                    DrawCircle(static_cast<int>(rect.x + rect.width - 16.0f), static_cast<int>(y - 4.0f), 7.0f,
                               Color{56, 37, 28, 255});
                } else if (definition && definition->kind == jf::BattleObjectKind::Marker) {
                    DrawCircleLines(static_cast<int>(rect.x + rect.width * 0.5f),
                                    static_cast<int>(rect.y + rect.height * 0.42f), 13.0f,
                                    Color{244, 199, 92, 230});
                } else if (definition && definition->kind == jf::BattleObjectKind::SpawnPoint) {
                    DrawTriangle({rect.x + rect.width * 0.5f, rect.y + 8.0f},
                                 {rect.x + rect.width * 0.35f, rect.y + rect.height - 14.0f},
                                 {rect.x + rect.width * 0.65f, rect.y + rect.height - 14.0f},
                                 Color{205, 82, 82, 190});
                } else if (definition && definition->kind == jf::BattleObjectKind::Container) {
                    // docs/regions/cinderwatch_gate.md「3. アイアンウォッチ物資庫」's
                    // supply crates: a small crate-shaped rectangle (box +
                    // lid seam) so a Container reads as distinct from the
                    // Barrier's crossed planks and the Marker's plain ring.
                    Rectangle box{rect.x + rect.width * 0.5f - 13.0f, rect.y + rect.height * 0.42f - 10.0f, 26.0f,
                                  20.0f};
                    DrawRectangleRec(box, Color{163, 118, 66, 235});
                    DrawRectangleLinesEx(box, 2.0f, Color{88, 60, 30, 255});
                    DrawLineEx({box.x, box.y + box.height * 0.5f}, {box.x + box.width, box.y + box.height * 0.5f},
                               2.0f, Color{88, 60, 30, 255});
                } else if (definition && definition->kind == jf::BattleObjectKind::Device) {
                    // docs/regions/cinderwatch_gate.md「5. 信号塔下層」's control
                    // panels: an upright panel with a status light, so an
                    // un-operated Device (Active, dim/red) visibly changes
                    // once interacted with (Opened, lit/green) - distinct
                    // from the Barrier/Marker/Container shapes above.
                    Rectangle panel{rect.x + rect.width * 0.5f - 9.0f, rect.y + rect.height * 0.5f - 15.0f, 18.0f,
                                    26.0f};
                    DrawRectangleRec(panel, Color{70, 74, 82, 235});
                    DrawRectangleLinesEx(panel, 2.0f, Color{30, 32, 36, 255});
                    bool operated = object->state == jf::BattleObjectStateKind::Opened;
                    DrawCircle(static_cast<int>(panel.x + panel.width * 0.5f), static_cast<int>(panel.y + 7.0f), 4.0f,
                              operated ? Color{110, 220, 130, 255} : Color{214, 70, 70, 255});
                }
            }

            bool reachable = containsTile(controller.reachableTiles(), pos);
            const jf::Unit* occupant = battle.unitAt(pos);
            bool occupiedByAlly = occupant && controller.selectedUnit() &&
                                  occupant->team == controller.selectedUnit()->team;

            // Movement takes visual priority on overlapping tiles, and an
            // ally's occupied tile should never look like an attack target.
            if (containsTile(controller.attackRangeTiles(), pos) && !reachable && !occupiedByAlly)
                DrawRectangleRec(rect, Color{238, 72, 72, 112});
            if (reachable)
                DrawRectangleRec(rect, Color{58, 155, 255, 125});
            if (containsTile(controller.targetableTiles(), pos) ||
                (controller.pendingTarget() && controller.pendingTarget()->position == pos))
                DrawRectangleRec(rect, Color{230, 28, 38, 185});
            if (containsTile(controller.objectTargetableTiles(), pos) ||
                (controller.pendingObjectTarget() && controller.pendingObjectTarget()->position == pos))
                DrawRectangleRec(rect, Color{230, 28, 38, 185});
            if (containsTile(controller.healableTiles(), pos))
                DrawRectangleRec(rect, Color{55, 205, 115, 155});
            if (containsTile(controller.itemTargetTiles(), pos))
                DrawRectangleRec(rect, Color{70, 210, 145, 175});
            if (containsTile(controller.boardTargetTiles(), pos))
                DrawRectangleRec(rect, Color{220, 185, 70, 150});
            if (containsTile(controller.skillTargetTiles(), pos))
                DrawRectangleRec(rect, Color{90, 200, 235, 165});
            if (containsTile(controller.objectInteractableTiles(), pos))
                DrawRectangleRec(rect, Color{190, 150, 235, 165});
            if (containsTile(telegraphDangerTiles, pos))
                DrawRectangleRec(rect, Color{235, 150, 60, 140});
        }
    }
}

// Every living Unit's sprite: shadow, selection ring, body, HP bar, status
// badges, and name tag. Split out of drawGrid(); no behavior change.
void drawUnitSprites(const jf::BattleController& controller, float dt) {
    const jf::BattleState& battle = controller.battle();

    for (const jf::Unit& unit : battle.units()) {
        if (!unit.isAlive()) continue;

        Vector2 center = animatedUnitCenter(unit, dt);
        float radius = kUnitRadius;

        Color c = teamColor(unit.team);
        if (unit.hasActed) c = Fade(c, 0.45f);

        DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y + radius * 0.92f),
                   radius * 0.85f, radius * 0.22f, kColorShadow);

        if (controller.selectedUnit() == &unit) {
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius + 7.0f,
                            withAlpha(kColorAccentGold, 235));
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius + 8.5f,
                            withAlpha(kColorAccentGold, 140));
        }

        DrawCircleV(center, radius, c);
        DrawCircleGradient(static_cast<int>(center.x - radius * 0.18f), static_cast<int>(center.y - radius * 0.2f),
                           radius * 0.72f, scaleColor(c, 1.22f), c);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius,
                        Color{10, 14, 22, 230});

        float barWidth = radius * 2.0f;
        float barHeight = 7.0f;
        Rectangle hpBack{center.x - barWidth / 2.0f, center.y + radius + 6.0f, barWidth, barHeight};
        float hpRatio = static_cast<float>(unit.currentHp) / static_cast<float>(unit.stats.maxHp);
        Color hpColor = hpRatio > 0.5f ? Color{95, 205, 120, 255}
                                       : (hpRatio > 0.25f ? Color{230, 165, 70, 255} : Color{215, 75, 75, 255});
        DrawRectangleRounded(hpBack, 0.5f, 6, Color{18, 20, 26, 255});
        if (hpRatio > 0.0f) {
            DrawRectangleRounded(Rectangle{hpBack.x, hpBack.y, hpBack.width * hpRatio, hpBack.height}, 0.5f, 6,
                                 hpColor);
        }
        DrawRectangleRoundedLinesEx(hpBack, 0.5f, 6, 1.0f, withAlpha(BLACK, 120));

        // Status-effect badges (docs/status_effects.md "UI"): up to 3 icons,
        // a 4th+ collapses into a "+N" badge instead of growing the row.
        std::vector<StatusBadge> badges = activeStatusBadges(unit);
        if (!badges.empty()) {
            constexpr float kBadgeRadius = 9.0f;
            constexpr float kBadgeGap = 4.0f;
            int shown = static_cast<int>(std::min<std::size_t>(badges.size(), 3));
            float rowWidth = shown * (kBadgeRadius * 2.0f) + (shown - 1) * kBadgeGap;
            float badgeY = hpBack.y + hpBack.height + 6.0f + kBadgeRadius;
            float badgeX = center.x - rowWidth / 2.0f + kBadgeRadius;
            for (int i = 0; i < shown; ++i) {
                bool overflowSlot = (i == 2 && badges.size() > 3);
                Vector2 badgeCenter{badgeX, badgeY};
                DrawCircleV(badgeCenter, kBadgeRadius, overflowSlot ? Color{70, 74, 84, 255} : badges[i].color);
                DrawCircleLines(static_cast<int>(badgeCenter.x), static_cast<int>(badgeCenter.y), kBadgeRadius,
                                withAlpha(BLACK, 150));
                std::string glyph = overflowSlot ? "+" + std::to_string(static_cast<int>(badges.size()) - 2)
                                                  : badges[i].glyph;
                int glyphWidth = textWidth(glyph, 9);
                drawText(glyph, static_cast<int>(badgeCenter.x - glyphWidth / 2.0f),
                         static_cast<int>(badgeCenter.y - 6.0f), 9, RAYWHITE);
                badgeX += kBadgeRadius * 2.0f + kBadgeGap;
            }
        }

        std::string displayName = unitDisplayNameFor(unit.name);
        int fontSize = 13;
        int nameWidth = textWidth(displayName, fontSize);
        float nameX = center.x - nameWidth / 2.0f;
        float nameY = center.y - radius - fontSize - 5.0f;
        DrawRectangleRounded(Rectangle{nameX - 6.0f, nameY - 2.0f, static_cast<float>(nameWidth) + 12.0f,
                                       static_cast<float>(fontSize) + 6.0f},
                             0.35f, 6, Color{14, 16, 22, 150});
        drawText(displayName, static_cast<int>(nameX), static_cast<int>(nameY), fontSize, kColorTextPrimary);
    }
}

void drawGrid(const jf::BattleController& controller, float dt) {
    std::vector<jf::GridPos> telegraphDangerTiles = detectAndAnnounceBattleEvents(controller);
    drawBoardTiles(controller, telegraphDangerTiles);
    drawUnitSprites(controller, dt);
    drawBattleMessages(dt);
}

void drawPhaseBanner(const jf::BattleController& controller) {
    bool isPlayerPhase = controller.battle().phase() == jf::Phase::PlayerPhase;
    std::string label = tr(isPlayerPhase ? "ui.phase.player" : "ui.phase.enemy");
    Color color = isPlayerPhase ? Color{60, 120, 220, 255} : Color{200, 60, 60, 255};
    DrawRectangleGradientV(0, 0, kScreenWidth, 40, scaleColor(color, 1.12f), scaleColor(color, 0.8f));
    DrawLine(0, 40, kScreenWidth, 40, withAlpha(BLACK, 130));
    int w = textWidth(label, 22);
    drawText(label, (kScreenWidth - w) / 2, 8, 22, kColorTextPrimary);
    drawText("JOJIFrontier - Battle Prototype", 12, 44, 18, kColorTextFaint);
}

float easeInOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

// Big banner that streams in from the left edge, holds in the center, then
// continues on and streams out the right edge - triggered whenever the
// battle phase changes (Player Phase <-> Enemy Phase). Purely a rendering
// effect: it watches BattleState::phase() but never touches game state.
void drawTurnChangeAnnouncement(jf::Phase currentPhase, float dt) {
    static jf::Phase lastPhase = currentPhase;
    static jf::Phase announcedPhase = currentPhase;
    static float elapsed = -1.0f; // negative = no animation in flight

    constexpr float kSlideIn = 0.28f;
    constexpr float kHold = 0.5f;
    constexpr float kSlideOut = 0.28f;
    constexpr float kTotal = kSlideIn + kHold + kSlideOut;

    if (currentPhase != lastPhase) {
        lastPhase = currentPhase;
        announcedPhase = currentPhase;
        elapsed = 0.0f;
    }

    if (elapsed < 0.0f) return;
    elapsed += dt;
    if (elapsed >= kTotal) {
        elapsed = -1.0f;
        return;
    }

    bool isPlayerPhase = announcedPhase == jf::Phase::PlayerPhase;
    std::string label = tr(isPlayerPhase ? "ui.phase.player" : "ui.phase.enemy");
    Color bannerColor = isPlayerPhase ? Color{35, 88, 205, 240} : Color{200, 40, 45, 240};

    int fontSize = 42;
    int textW = textWidth(label, fontSize);
    float bannerW = static_cast<float>(textW) + 140.0f;
    float bannerH = 90.0f;
    float centerX = (static_cast<float>(kScreenWidth) - bannerW) / 2.0f;
    float y = static_cast<float>(kScreenHeight) / 2.0f - bannerH / 2.0f;

    float x;
    if (elapsed < kSlideIn) {
        float t = easeInOutCubic(elapsed / kSlideIn);
        x = -bannerW + t * (centerX + bannerW);
    } else if (elapsed < kSlideIn + kHold) {
        x = centerX;
    } else {
        float t = easeInOutCubic((elapsed - kSlideIn - kHold) / kSlideOut);
        x = centerX + t * (static_cast<float>(kScreenWidth) - centerX + bannerW);
    }

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 60});
    Rectangle bannerRect{x, y, bannerW, bannerH};
    DrawRectangleRounded(Rectangle{bannerRect.x + 3.0f, bannerRect.y + 4.0f, bannerRect.width, bannerRect.height},
                         0.16f, 10, kColorShadow);
    DrawRectangleRounded(bannerRect, 0.16f, 10, scaleColor(bannerColor, 0.88f));
    DrawRectangleRounded(Rectangle{bannerRect.x, bannerRect.y, bannerRect.width, bannerRect.height * 0.5f}, 0.16f,
                         10, scaleColor(bannerColor, 1.16f));
    DrawRectangleRoundedLinesEx(bannerRect, 0.16f, 10, 3.0f, withAlpha(kColorAccentGold, 235));
    drawText(label, static_cast<int>(x + (bannerW - static_cast<float>(textW)) / 2.0f),
              static_cast<int>(y + (bannerH - static_cast<float>(fontSize)) / 2.0f), fontSize, kColorTextPrimary);
}

// Standalone popup shown while confirming an attack. The HUD strip at the
// bottom is too cramped/low-contrast for this much text to read clearly, so
// it gets its own panel with a dark background and a bright border instead.
void drawCombatPreviewPopup(const jf::CombatPreview& preview) {
    // Only dim the battlefield above the HUD strip - the HUD (with the
    // Confirm/Cancel buttons for this exact popup) is drawn before this and
    // must stay fully lit/interactive-looking, not washed out underneath it.
    DrawRectangle(0, 0, kScreenWidth, static_cast<int>(kHudY), Color{0, 0, 0, 100});

    float panelW = 480.0f;
    float panelH = 196.0f;
    float x = (static_cast<float>(kScreenWidth) - panelW) / 2.0f;
    float y = 120.0f;

    Rectangle panel{x, y, panelW, panelH};
    drawCard(panel, kColorCard, withAlpha(kColorAccentGold, 235), 0.08f);

    int tx = static_cast<int>(x) + 24;
    int ty = static_cast<int>(y) + 18;

    std::string attackerName = unitDisplayNameFor(preview.attackerName);
    std::string targetName = unitDisplayNameFor(preview.targetName);

    drawText(tr("ui.combat.preview_title"), tx, ty, 20, kColorAccentGold);
    ty += 34;
    drawText(attackerName + "  ->  " + targetName, tx, ty, 22, kColorTextPrimary);
    ty += 30;
    drawText(weaponNameFor(preview.weaponId, preview.weaponName), tx, ty, 15, kColorTextMuted);
    ty += 28;
    drawText(tr("ui.combat.damage") + ": " + std::to_string(preview.damage), tx, ty, 19,
             Color{255, 140, 120, 255});
    ty += 30;
    drawText(tr("ui.combat.hit") + ": " + std::to_string(preview.hitChance) + "%", tx + 235, ty - 30, 17,
             preview.hitChance < 100 ? kColorAccentGold : kColorTextMuted);
    drawText(targetName + " HP: " + std::to_string(preview.targetHpBefore) + " -> " +
                 std::to_string(preview.targetHpAfter),
             tx, ty, 19, Color{235, 90, 90, 255});
}

// Battle Object統合: drawCombatPreviewPopup()のObject版。Objectには命中率・
// 反撃・状態異常が無いためDamageと耐久のみを表示する簡略版。
void drawObjectAttackPreviewPopup(const jf::ObjectAttackPreview& preview) {
    DrawRectangle(0, 0, kScreenWidth, static_cast<int>(kHudY), Color{0, 0, 0, 100});

    float panelW = 480.0f;
    float panelH = 196.0f;
    float x = (static_cast<float>(kScreenWidth) - panelW) / 2.0f;
    float y = 120.0f;

    Rectangle panel{x, y, panelW, panelH};
    drawCard(panel, kColorCard, withAlpha(kColorAccentGold, 235), 0.08f);

    int tx = static_cast<int>(x) + 24;
    int ty = static_cast<int>(y) + 18;

    std::string attackerName = unitDisplayNameFor(preview.attackerName);
    std::string objectName = battleObjectNameForKind(preview.objectKind);

    drawText(tr("ui.combat.preview_title"), tx, ty, 20, kColorAccentGold);
    ty += 34;
    drawText(attackerName + "  ->  " + objectName, tx, ty, 22, kColorTextPrimary);
    ty += 44;
    drawText(tr("ui.combat.damage") + ": " + std::to_string(preview.damage), tx, ty, 19,
             Color{255, 140, 120, 255});
    ty += 30;
    drawText(objectName + " HP: " + std::to_string(preview.durabilityBefore) + " -> " +
                 std::to_string(preview.durabilityAfter),
             tx, ty, 19, Color{235, 90, 90, 255});
}

void drawHoverInfo(const jf::GameData& data, const jf::BattleController& controller, Vector2 mouse) {
    jf::GridPos pos;
    if (!tileFromScreen(mouse, pos)) return;

    const jf::BattleState& battle = controller.battle();
    std::vector<TooltipLine> lines;

    if (const jf::Unit* unit = battle.unitAt(pos)) {
        Color nameColor = teamColor(unit->team);
        lines.push_back({unitDisplayNameFor(unit->name) + "  " + classNameFor(data, unit->unitClass), nameColor, 17});
        lines.push_back({tr("ui.unit.hp_label") + " " + std::to_string(unit->currentHp) + " / " +
                              std::to_string(unit->stats.maxHp),
                          Color{190, 205, 215, 255}, 14});
        lines.push_back({"STR " + std::to_string(unit->stats.strength) + "  MAG " +
                              std::to_string(unit->stats.magic) + "  DEF " + std::to_string(unit->effectiveDefense()) +
                              "  RES " + std::to_string(unit->stats.resistance) + "  MOV " +
                              std::to_string(unit->effectiveMove()),
                          Color{170, 180, 195, 255}, 13});
        lines.push_back({weaponNameFor(unit->weapon.id, unit->weapon.name) + " (Mt " +
                              std::to_string(unit->weapon.might) + ", Rng " +
                              std::to_string(unit->minimumAttackRange()) + "-" +
                              std::to_string(unit->weapon.maxRange) + ")",
                          Color{170, 180, 195, 255}, 13});
        for (const StatusBadge& badge : activeStatusBadges(*unit)) {
            lines.push_back({badge.detail, badge.color, 12});
        }
        jf::TerrainType terrain = battle.terrainAt(pos);
        if (terrain != jf::TerrainType::Floor) {
            std::string effect = terrainNameFor(terrain);
            if (jf::defenseBonus(terrain) > 0) effect += "  DEF +" + std::to_string(jf::defenseBonus(terrain));
            if (jf::evasionBonus(terrain) > 0)
                effect += "  " + tr("ui.combat.evasion") + " +" + std::to_string(jf::evasionBonus(terrain)) + "%";
            if (terrain == jf::TerrainType::HerbPatch) effect += "  HP +5";
            lines.push_back({effect, Color{175, 208, 184, 255}, 12});
        }
    } else {
        jf::TerrainType terrain = battle.terrainAt(pos);
        lines.push_back({terrainNameFor(terrain), RAYWHITE, 16});
        if (const jf::BattleObjectState* object = battle.objectAt(pos);
            object && object->state != jf::BattleObjectStateKind::Destroyed) {
            if (const jf::BattleObjectDefinition* definition = battle.objectDefinition(object->definitionId)) {
                lines.push_back({battleObjectNameFor(*definition), Color{244, 199, 92, 255}, 16});
                if (definition->blocksMovement || definition->blocksStopping)
                    lines.push_back({tr("ui.tile.blocks_movement"),
                                     Color{225, 120, 120, 255}, 13});
            }
        }
        if (!jf::isPassable(terrain)) {
            lines.push_back({tr("ui.tile.impassable"), Color{225, 120, 120, 255}, 13});
        } else {
            lines.push_back({tr("ui.tile.move_cost") + " " + std::to_string(jf::movementCost(terrain)),
                              Color{190, 190, 205, 255}, 13});
            int def = jf::defenseBonus(terrain);
            if (def > 0) lines.push_back({"DEF +" + std::to_string(def), Color{175, 208, 184, 255}, 13});
            int evasion = jf::evasionBonus(terrain);
            if (evasion > 0)
                lines.push_back({tr("ui.combat.evasion") + " +" + std::to_string(evasion) + "%",
                                 Color{175, 208, 184, 255}, 13});
            if (terrain == jf::TerrainType::HerbPatch)
                lines.push_back({tr("ui.tile.heal_5_hp"),
                                 Color{130, 210, 145, 255}, 13});
        }
    }

    drawTooltipBox(mouse, lines);
}

// The fixed 5-slot action button row (plus the conditional 6th "Interact"
// slot - see its own comment below) at the bottom of the Battle HUD, and
// every inputState()-specific variant of it: item/skill sub-menus,
// Confirm/Cancel pairs, and the plain "Cancel" shown while selecting a
// target. Split out of drawBattleHud() itself (which also draws the
// mission name / selected-unit info / step label above this row) purely to
// keep each piece under a screenful - no behavior change.
void drawBattleActionButtons(jf::GameApp& app, jf::BattleController& controller, Vector2 mouse, bool clicked,
                             int hudTop, int buttonWidth, int buttonHeight, int buttonGap, int firstActionX,
                             int secondActionX, int thirdActionX, int fourthActionX, int fifthActionX,
                             int sixthActionX) {
    switch (controller.inputState()) {
        case jf::BattleInputState::SelectUnit:
            if (!gBattleItemMenuOpen) {
                if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.items"), mouse, clicked)) {
                    gBattleItemMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.end_turn"), mouse, clicked)) {
                    controller.endPlayerTurn();
                }
                break;
            }
            {
                int itemX = firstActionX;
                int usableItemCount = 0;
                const bool hasHealingTarget = std::any_of(
                    controller.battle().units().begin(), controller.battle().units().end(), [](const jf::Unit& unit) {
                        return unit.team == jf::Team::Player && unit.isAlive() && !unit.hasActed &&
                               unit.currentHp < unit.stats.maxHp;
                    });
                const auto drawNeutralHealingItem = [&](jf::ItemType type, const std::string& en,
                                                        const std::string& ja) {
                    const int count = app.expedition().count(type);
                    if (!hasHealingTarget || count <= 0) return;
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                               en + " " + std::to_string(count), ja + " " + std::to_string(count), mouse, clicked) &&
                        app.chooseNeutralBattleHealingItem(type)) {
                        gBattleItemMenuOpen = false;
                    }
                    itemX += buttonWidth + buttonGap;
                    ++usableItemCount;
                };
                drawNeutralHealingItem(jf::ItemType::FieldTreatmentKit, tr("item.field_treatment_kit.short_name"),
                                        tr("item.field_treatment_kit.short_name"));
                if (usableItemCount == 0) {
                    drawText(tr("ui.battle.no_usable_items"), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.back"), mouse, clicked)) {
                    gBattleItemMenuOpen = false;
                }
            }
            break;
        case jf::BattleInputState::SelectAction:
            if (!gBattleItemMenuOpen && !gBattleSkillMenuOpen) {
                if (controller.canInteract() &&
                    button(Rectangle{static_cast<float>(sixthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.interact"), mouse, clicked)) {
                    controller.chooseInteract();
                }
                if (button(Rectangle{static_cast<float>(firstActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.attack"), mouse, clicked)) {
                    controller.chooseAttack();
                }
                if (button(Rectangle{static_cast<float>(secondActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.skills"), mouse, clicked)) {
                    gBattleSkillMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.items"), mouse, clicked)) {
                    gBattleItemMenuOpen = true;
                }
                if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.wait"), mouse, clicked)) {
                    controller.chooseWait();
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.back"), mouse, clicked)) {
                    controller.returnToMoveSelection();
                }
                break;
            }

            if (gBattleSkillMenuOpen) {
                jf::Unit* selected = controller.selectedUnit();
                bool anySkillShown = false;
                if (selected && jf::canHeal(selected->unitClass)) {
                    anySkillShown = true;
                    if (button(Rectangle{static_cast<float>(firstActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                               tr("ui.button.heal"), mouse, clicked)) {
                        controller.chooseHeal();
                        if (controller.inputState() != jf::BattleInputState::SelectAction) {
                            gBattleSkillMenuOpen = false;
                        }
                    }
                }
                // docs/implementation_roadmap.md M4 item 1: the 2 equipped
                // Tier-1 skill slots, shown per docs/skill_system.md "使用
                //不能スキルは非表示にせず、理由付きで無効表示" - an equipped
                // but currently-unusable skill still shows, grayed out, with
                // its reason on hover, rather than disappearing.
                if (selected) {
                    const auto skills = controller.selectedUnitSkills();
                    const int slotX[2] = {secondActionX, thirdActionX};
                    for (std::size_t i = 0; i < skills.size(); ++i) {
                        if (skills[i].skillId.empty()) continue;
                        anySkillShown = true;
                        const jf::SkillDefinition* def = jf::findSkill(skills[i].skillId);
                        std::string label = def ? pick(def->nameEn, def->nameJa) : skills[i].skillId;
                        Rectangle rect{static_cast<float>(slotX[i]), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)};
                        if (skills[i].available) {
                            if (button(rect, label, label, mouse, clicked)) {
                                controller.chooseSkill(static_cast<int>(i));
                                if (controller.inputState() != jf::BattleInputState::SelectAction) {
                                    gBattleSkillMenuOpen = false;
                                }
                            }
                        } else {
                            disabledButton(rect, label);
                            if (CheckCollisionPointRec(mouse, rect)) {
                                drawText(pick(skills[i].reasonEn, skills[i].reasonJa), slotX[i],
                                         static_cast<int>(hudTop) + 74, 12, kColorTextMuted);
                            }
                        }
                    }
                }
                if (!anySkillShown) {
                    drawText(tr("ui.battle.no_usable_skills"), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.back"), mouse, clicked)) {
                    gBattleSkillMenuOpen = false;
                }
                break;
            }

            {
                jf::Unit* selected = controller.selectedUnit();
                const bool needsHealing = selected && selected->currentHp < selected->stats.maxHp;
                int itemX = firstActionX;
                int usableItemCount = 0;
                const auto drawHealingItem = [&](jf::ItemType type, const std::string& en, const std::string& ja) {
                    const int count = app.expedition().count(type);
                    if (!needsHealing || count <= 0) return;
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                               en + " " + std::to_string(count), ja + " " + std::to_string(count), mouse, clicked) &&
                        app.useBattleHealingItem(type)) {
                        gBattleItemMenuOpen = false;
                    }
                    itemX += buttonWidth + buttonGap;
                    ++usableItemCount;
                };
                drawHealingItem(jf::ItemType::FieldTreatmentKit, tr("item.field_treatment_kit.short_name"),
                                tr("item.field_treatment_kit.short_name"));

                const int boardCount = app.expedition().count(jf::ItemType::ProtectiveBoard);
                if (boardCount > 0) {
                    if (button(Rectangle{static_cast<float>(itemX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                               tr("item.protective_board.short_name") + " " + std::to_string(boardCount),
                               tr("item.protective_board.short_name") + " " + std::to_string(boardCount), mouse,
                               clicked) &&
                        app.chooseProtectiveBoard()) {
                        gBattleItemMenuOpen = false;
                    }
                    ++usableItemCount;
                }
                if (usableItemCount == 0) {
                    drawText(tr("ui.battle.no_usable_items"), firstActionX, hudTop + 39, 14,
                             kColorTextMuted);
                }
                if (button(Rectangle{static_cast<float>(fifthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                           tr("ui.button.back"), mouse, clicked)) {
                    gBattleItemMenuOpen = false;
                }
            }
            break;
        case jf::BattleInputState::ConfirmAttack:
            // The readable preview is drawn as its own popup (see
            // drawCombatPreviewPopup); the HUD here just keeps the buttons.
            if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.confirm"), mouse, clicked))
                controller.confirmAttack();
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.cancel"), mouse, clicked))
                controller.cancelAttackSelection();
            break;
        case jf::BattleInputState::ConfirmSkillAttack:
            // M4 item 3: same Confirm/Cancel pair as ConfirmAttack, for the
            // 3 attack-shape skills' pre-resolution preview.
            if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.confirm"), mouse, clicked))
                controller.confirmSkillAttack();
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.cancel"), mouse, clicked))
                controller.cancelAttackSelection();
            break;
        case jf::BattleInputState::ConfirmObjectAttack:
            // Battle Object統合: 同じConfirm/Cancelペア。Previewは
            // drawCombatPreviewPopup相当の別ポップアップ側で描画。
            if (button(Rectangle{static_cast<float>(thirdActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.confirm"), mouse, clicked)) {
                // attackEventId()方式(lastAttacker_/lastAttackTarget_)はUnit
                // 専用のためObjectには使えない - ここでConfirm直前にPreviewを
                // 読み、確定後のバナー文言をその場で組み立てる
                auto preview = controller.pendingObjectPreview();
                controller.confirmObjectAttack();
                if (preview) {
                    std::string objectName = battleObjectNameForKind(preview->objectKind);
                    pushBattleMessage(
                        objectHitMessageText(unitDisplayNameFor(preview->attackerName), objectName, preview->damage),
                        Color{255, 205, 120, 255});
                    if (preview->durabilityAfter <= 0)
                        pushBattleMessage(objectDestroyedMessageText(objectName), Color{225, 90, 90, 255});
                }
            }
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 43.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.cancel"), mouse, clicked))
                controller.cancelAttackSelection();
            break;
        case jf::BattleInputState::SelectMove:
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.cancel"), mouse, clicked))
                controller.cancelToUnitSelect();
            break;
        case jf::BattleInputState::SelectTarget:
        case jf::BattleInputState::SelectHealTarget:
        case jf::BattleInputState::SelectItemTarget:
        case jf::BattleInputState::SelectBoardTarget:
        case jf::BattleInputState::SelectSkillTarget:
        case jf::BattleInputState::SelectInteractTarget:
            if (button(Rectangle{static_cast<float>(fourthActionX), hudTop + 29.0f, static_cast<float>(buttonWidth), static_cast<float>(buttonHeight)},
                       tr("ui.button.back"), mouse, clicked))
                controller.cancelAttackSelection();
            break;
        default:
            break;
    }
}

void drawBattleHud(jf::GameApp& app, Vector2 mouse, bool clicked) {
    jf::BattleController& controller = app.battle();
    if (controller.inputState() != jf::BattleInputState::SelectAction &&
        controller.inputState() != jf::BattleInputState::SelectUnit) {
        gBattleItemMenuOpen = false;
        gBattleSkillMenuOpen = false;
    }
    constexpr int hudTop = static_cast<int>(kHudY);
    constexpr int buttonWidth = 108;
    constexpr int buttonHeight = 38;
    constexpr int buttonGap = 8;
    int fifthActionX = kScreenWidth - buttonWidth - 18;
    int fourthActionX = fifthActionX - buttonWidth - buttonGap;
    int thirdActionX = fourthActionX - buttonWidth - buttonGap;
    int secondActionX = thirdActionX - buttonWidth - buttonGap;
    int firstActionX = secondActionX - buttonWidth - buttonGap;
    // Battle Object統合(Interact配線): 5固定Slotの外側に置く6個目。他4Actionと
    // 違い「対象があるSelectAction中だけ現れる」条件付きButtonのため、既存Slotを
    // ずらさず追加した(現状出荷済みコンテンツにInteract可能なObjectが無いため
    // 通常プレイでは表示されない)。
    int sixthActionX = firstActionX - buttonWidth - buttonGap;

    DrawRectangleGradientV(0, hudTop, kScreenWidth, kScreenHeight - hudTop, Color{24, 28, 39, 255},
                           Color{15, 17, 24, 255});
    DrawLine(0, hudTop, kScreenWidth, hudTop, withAlpha(kColorBorder, 200));
    DrawRectangle(0, hudTop - 44, kScreenWidth, 44, Color{16, 20, 29, 245});
    DrawLine(0, hudTop - 44, kScreenWidth, hudTop - 44, withAlpha(kColorBorderSoft, 180));
    std::string missionName = pick(app.currentMissionName(), app.currentMissionNameJa());
    drawText(tr("ui.battle.location_prefix", {{"mission", missionName}}),
             18, hudTop - 36, 17, kColorAccentGold);

    if (jf::Unit* selected = controller.selectedUnit()) {
        drawText(unitDisplayNameFor(selected->name) + "  " + classNameFor(app.gameData(), selected->unitClass), 18, hudTop + 12, 19,
                 kColorTextPrimary);
        const std::string hpText = "HP " + std::to_string(selected->currentHp) + "/" +
                                   std::to_string(selected->stats.maxHp);
        drawText(hpText, 18, hudTop + 42, 16, kColorTextMuted);
        std::string stats = "STR " + std::to_string(selected->stats.strength) + "   MAG " +
                            std::to_string(selected->stats.magic) + "   DEF " +
                            std::to_string(selected->stats.defense) + "   RES " +
                            std::to_string(selected->stats.resistance);
        drawText(stats, 18 + textWidth(hpText, 16) + 28, hudTop + 42, 14, kColorTextFaint);
    } else {
        drawText(tr("ui.battle.select_unit_prompt"), 18, hudTop + 33, 15, kColorTextMuted);
    }

    std::string stepLabel;
    switch (controller.inputState()) {
        case jf::BattleInputState::SelectMove:
            stepLabel = tr("ui.battle.choose_move");
            break;
        case jf::BattleInputState::SelectAction:
            if (gBattleItemMenuOpen) stepLabel = tr("ui.battle.choose_item");
            else if (gBattleSkillMenuOpen) stepLabel = tr("ui.battle.choose_skill");
            else stepLabel = tr("ui.battle.choose_action");
            break;
        case jf::BattleInputState::SelectItemTarget:
            stepLabel = tr("ui.battle.choose_item_target");
            break;
        case jf::BattleInputState::SelectTarget:
            stepLabel = tr("ui.battle.choose_target");
            break;
        case jf::BattleInputState::SelectHealTarget:
            stepLabel = tr("ui.button.heal");
            break;
        case jf::BattleInputState::SelectBoardTarget:
            stepLabel = tr("ui.button.place_barrier");
            break;
        case jf::BattleInputState::SelectSkillTarget:
            stepLabel = tr("ui.battle.choose_skill_target");
            break;
        case jf::BattleInputState::SelectInteractTarget:
            stepLabel = tr("ui.button.interact");
            break;
        default:
            break;
    }
    if (!stepLabel.empty()) drawText(stepLabel, 420, hudTop + 12, 15, kColorAccentGold);
    drawBattleActionButtons(app, controller, mouse, clicked, hudTop, buttonWidth, buttonHeight, buttonGap,
                           firstActionX, secondActionX, thirdActionX, fourthActionX, fifthActionX, sixthActionX);
}

void drawVictoryOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 190});
    drawCard(Rectangle{static_cast<float>(kScreenWidth) / 2.0f - 230.0f, 220.0f, 460.0f, 200.0f}, kColorCard,
            withAlpha(kColorAccentGold, 220), 0.1f);
    std::string title = tr("ui.result.victory");
    drawText(title, kScreenWidth / 2 - textWidth(title, 44) / 2, 260, 44, kColorAccentGold);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 345, 260, 50};
    if (button(rect, tr("ui.button.proceed_to_camp"), mouse, clicked)) {
        app.proceedToCamp();
    }
}

void drawDefeatOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 190});
    drawCard(Rectangle{static_cast<float>(kScreenWidth) / 2.0f - 230.0f, 220.0f, 460.0f, 230.0f}, kColorCard,
            withAlpha(Color{200, 70, 70, 255}, 220), 0.1f);
    std::string title = tr("ui.result.defeat_title");
    drawText(title, kScreenWidth / 2 - textWidth(title, 38) / 2, 260, 38, Color{225, 90, 90, 255});
    std::string line = tr("ui.defeat.loot_lost_line");
    drawText(line, kScreenWidth / 2 - textWidth(line, 16) / 2, 312, 16, kColorTextPrimary);
    Rectangle rect{static_cast<float>(kScreenWidth / 2 - 130), 380, 260, 50};
    if (button(rect, tr("ui.button.return_to_base"), mouse, clicked)) {
        app.acknowledgeDefeat();
    }
}

}  // namespace jfui
