# JOJIFrontier Battle System

行動とEventの確定順は[`combat_resolution_order.md`](combat_resolution_order.md)、反撃は
[`counterattack_rules.md`](counterattack_rules.md)、敵増援は
[`reinforcement_rules.md`](reinforcement_rules.md)、Boss共通処理は
[`boss_common_rules.md`](boss_common_rules.md)、盤面Objectは
[`battle_objects.md`](battle_objects.md)、攻撃予測は[`combat_forecast.md`](combat_forecast.md)、敵AIは
[`enemy_ai_rules.md`](enemy_ai_rules.md)を正本とする。

This documents how the battle prototype actually works today, as implemented
in `include/jf/battle/`, `include/jf/core/`, and `src/battle/`. It describes
mechanics, not narrative — see
[`frontier_setting.md`](frontier_setting.md) for the story/scenario layer that
sits on top of this system (Cinderwatch Gate, Ironwatch Stores, The Last
Signal).

## Architecture

Battle rules live entirely outside raylib, under `jf::battle` and `jf::core`,
so they can run headlessly (and have been verified that way). `src/main.cpp`
only renders the current state and forwards clicks into `BattleController`;
it never decides outcomes itself.

| Layer | Files | Responsibility |
|---|---|---|
| Data model | `BattleState`, `Unit`, `Stats`, `Weapon`, `Terrain` | Roster, positions, terrain, phase — no rules |
| Rules | `Movement`, `CombatResolver`, `EnemyAI` | Pure functions: reachable tiles, damage, AI decisions |
| Flow | `BattleController` | The `BattleInputState` state machine described below |
| Scenario | `BattleFactory`, `GameApp` | Builds a `BattleState` per mission stage; owns the expedition loop |
| Rendering | `src/main.cpp` | Draws `BattleController`'s current state; owns no game rules |

## Grid

Fixed **3 rows × 8 columns** (`jf::kGridRows`, `jf::kGridCols`), one unit per
tile, orthogonal movement only (no diagonals). Player units spawn at
`(0,0) (1,0) (1,1) (2,1)`; enemies at `(0,6) (0,7) (1,5) (2,6)`, matching the
ASCII layout in the original design doc.

## Units and stats

```cpp
struct Stats {
    int maxHp, strength, magic, speed, defense, resistance, move;
};
```

No character levels or XP — a unit's stats come entirely from its
`UnitClass`, loaded from `data/classes.json`:

| Class | HP | STR | MAG | DEF | RES | MOV | Weapon |
|---|---|---|---|---|---|---|---|
| March Captain | 22 | 7 | 1 | 5 | 4 | 4 | Iron Sword |
| Veteran Guard | 28 | 8 | 0 | 10 | 3 | 3 | Iron Lance |
| Watch Archer | 18 | 6 | 0 | 3 | 2 | 4 | Watch Bow |
| Frontier Scout | 18 | 5 | 0 | 3 | 3 | 5 | Scout Blade |
| Spearman | 23 | 7 | 0 | 7 | 3 | 4 | Iron Lance |
| Dawn Chirurgeon | 17 | 2 | 6 | 2 | 8 | 4 | Dawn Staff |
| Bandit (enemy) | 22 | 9 | 0 | 3 | 1 | 4 | Iron Axe |

Weapons (`data/weapons.json`) carry `might`, a `[minRange, maxRange]` band,
and a `DamageType` (Physical or Magical):

| Weapon | Might | Range | Type |
|---|---|---|---|
| Iron Sword | 5 | 1 | Physical |
| Iron Lance | 6 | 1–2 | Physical |
| Iron Axe | 7 | 1 | Physical |
| Watch Bow | 5 | 2–3 | Physical |
| Scout Blade | 4 | 1 | Physical |
| Dawn Staff | 3 | 1–2 | Magical |

## Damage

Damage is deterministic and there are no critical hits or damage variance:

```
damage = attacker.STR_or_MAG + weapon.might - target.DEF_or_RES - terrainDefenseBonus
damage = max(damage, 1)
```

STR is used for Physical weapons, MAG for Magical ones
(`Unit::attackPower()`). `terrainDefenseBonus` is the defender's tile bonus
(see Terrain below), so standing on a Watch Post reduces incoming damage by 2
regardless of damage type. `CombatResolver::previewAttack()` computes the
exact same formula ahead of time for the confirmation popup, so what's shown
before attacking always matches what happens after confirming.

Attacks normally have 100% hit chance. A defender standing on Brush receives
20% evasion, reducing incoming hit chance to 80%. The combat preview displays
this value. Hit rolls use the battle seed and attack sequence, so replaying the
same saved battle state produces the same result rather than allowing reloads
to reroll a miss.

## Terrain

Each battle has a `std::array<TerrainType, rows*cols>` (`BattleState::terrain_`).
`BattleFactory::generateFieldTerrain()` creates it from the expedition seed and
field type:

| Terrain | Move cost | Defense bonus | Passable | Looks like |
|---|---|---|---|---|
| Floor | 1 | 0 | yes | default panel |
| Ash | 2 | 0 | yes | brownish-grey tile |
| Rubble | 2 | 0 | yes | tan/grey tile |
| Watch Post | 1 | +2 | yes | green-tinted tile |
| Barrier | 999 (effectively infinite) | — | **no** | dark blocked tile |
| Brush | 1 | 0; evasion +20% | yes | dense green growth |
| Herb Patch | 1 | 0 | yes | green medicinal plants; heals a player unit 5 HP when it ends its action there, then becomes Floor |

Terrain affects both `computeReachableTiles` (movement cost, impassability)
and `computeDamage`/`resolveAttack` (defense bonus for whoever is standing on
the tile being hit) — attacking into a Watch Post is measurably worse than
attacking the same unit on open Floor.

### Field generation

The logical grid remains 3x8, but terrain placement is not fixed. Each new
expedition receives a random seed. The seed is retained internally for
reproduction but is intentionally hidden from the normal battle HUD.
The three stages use different generation profiles:

- **Cinderwatch Outpost**: more Rubble and occasional Watch Posts; no Brush.
- **Ash Road**: Ash is dominant, with scattered Rubble and Brush.
- **Signal Tower**: more Watch Posts, defensive structure, and occasional Brush.

Generation rolls terrain for all 24 tiles, including both teams' 3x3 deployment
zones. Barriers are capped at one per column, preventing a full three-row wall
from splitting the battlefield. After units are assigned random starting
positions, only an occupied impassable tile is converted to Floor; generated
terrain on every other deployment-zone tile remains intact. Free deployment may
also select an impassable tile, which is opened only when battle starts. Every
field forces at least one signature tile. The same seed and field type always
reproduce the same terrain.

This policy also applies to every future expedition field, including
Ashbough Forest. Scenario documents define generation profiles, required
special-tile counts, spawn zones, and solvability constraints rather than a
stored 24-tile layout. A new expedition produces a new field; loading or
resuming an existing expedition reproduces its already chosen field.

## Turn structure

Fire Emblem-style: **Player Phase** then **Enemy Phase**, alternating.
`BattleState::isTeamDone(Team)` checks whether every living unit on a team
has `hasActed`; `beginPlayerPhase()`/`beginEnemyPhase()` reset that flag for
the relevant team only. Each unit acts exactly once per phase it's alive for.

## Input flow (`BattleInputState`)

```
SelectUnit → SelectMove → SelectAction → SelectTarget → ConfirmAttack → SelectUnit
                 ↑              │
                 └── Back ──────┘
                            ↑
              SelectTarget ─┘ (Back/Cancel from target or confirm step)
```

| State | What's shown | What advances it |
|---|---|---|
| `SelectUnit` | Nothing selected; Items / End Turn commands | Click a player unit that hasn't acted → `SelectMove`; Items selects a wounded unacted ally; End Turn hands off to Enemy Phase |
| `SelectMove` | Blue = reachable tiles (BFS, respects `move` stat + terrain cost); light red = full attack-range preview from every reachable tile | Click a reachable tile → `SelectAction`. Cancel → `SelectUnit` |
| `SelectAction` | Attack / Skills / Items / Wait / Back commands | Pick one; Back undoes the move and returns to `SelectMove` |
| `SelectTarget` | Red = tiles with a valid enemy in weapon range | Click a target → `ConfirmAttack`. Back → `SelectAction` |
| `SelectHealTarget` | Green = wounded self/adjacent allies | Click an ally to heal 8 HP and end the Chirurgeon's action. Back → `SelectAction` |
| `SelectItemTarget` | Green = wounded, living, unacted allies | Click an ally to use the selected healing item; that ally becomes acted. Back → `SelectUnit` |
| `ConfirmAttack` | Full-screen combat preview popup (attacker → target, weapon, damage, HP before/after) | Confirm resolves the attack and marks the unit acted; Cancel → `SelectAction` |
| `EnemyTurn` | Enemies act automatically, paced ~0.6s apart | Auto-advances to `SelectUnit` (next Player Phase) once every enemy has acted |
| `Victory` / `Defeat` | End-of-battle overlay | Handled by `GameApp`, not `BattleController` |

Every `BattleController` method is a documented no-op outside its expected
state (e.g. calling `chooseAttack()` while in `SelectMove` does nothing), so
the UI layer can wire buttons up per-state without extra guards.

### Movement and attack-range preview

`computeReachableTiles` is a Dijkstra-style search (priority queue keyed by
accumulated move cost, since terrain move cost can be >1) that respects the
mover's `move` stat and terrain cost/impassability. Allied units may be crossed
but their occupied tiles cannot be chosen as destinations. Enemy-occupied tiles
block both pathing and stopping. Future Zone of Control rules should selectively
stop path expansion without changing these basic occupancy rules.
`computeAttackRangeTiles`
unions weapon range over a set of origin tiles — while still choosing where
to move it's evaluated over *every* reachable tile (a full threat-range
preview); once the unit has moved it narrows to just that final tile.

## Expedition item specification

正式な消耗品12種類、探索道具6種類、製作費、使用制限は
[`item_system.md`](item_system.md)を正本とする。以下は現行コードで実装済みの6種類を示す実装表であり、
正式カタログ全体ではない。

The old fixed three-potion rule is retired. Before departure, the player fills
an expedition bag with up to six items. One copy occupies one slot, duplicate
items are allowed, and use consumes that copy. Items do not replenish during an
expedition. Defeat discards pending loot but bag items already consumed remain
consumed; returning to base ends the expedition and the next bag is prepared
from scratch.

| Internal type | Display name | Usable in battle | Usable at camp | Current effect | Action and target rules |
|---|---|---:|---:|---|---|
| `FirstAidKit` | First Aid Kit / 救急セット | No | Yes | Restore 20 HP to one living ally, capped at max HP | Camp command; does not revive |
| `FieldTreatmentKit` | Field Treatment Kit / 野戦治療キット | Yes | Yes | Restore 10 HP to one living ally, capped at max HP | In battle, using it ends the selected/target ally's action; only wounded allies are valid |
| `RescuePack` | Rescue Pack / 救命包 | No | Yes | Revive one defeated ally at 25% max HP (minimum 1) | Only defeated allies are valid |
| `CampRations` | Camp Rations / 野営食 | No | Yes | Restore 5 HP to every living wounded ally | Consumed only if at least one ally is healed |
| `ProtectiveBoard` | Protective Board / 防護板 | Yes | No | Change one adjacent empty passable tile into an impassable Barrier | Ends the acting unit's action; requires a valid adjacent tile |
| `ReturnFlare` | Return Flare / 帰還信号弾 | No | Yes | Immediately secure pending loot/discoveries and return to base | Ends the current expedition |

`FirstAidKit` is deliberately camp-only despite its larger recovery. The
smaller `FieldTreatmentKit` is the only direct HP consumable currently usable
during battle. This preserves a distinction between efficient recovery at a
safe camp and emergency recovery that costs a unit action in combat.

## Core class abilities

- **March Captain — Formation Bonus:** one adjacent allied Captain grants the
  defender +1 combat defense. It does not stack and does not buff the Captain
  itself.
- **Veteran Guard — Zone of Control:** an enemy may enter a tile adjacent to the
  Guard, but cannot continue moving from it. Multiple Guards do not stack.
- **Watch Archer — Long Shot:** minimum attack range is always 2, even if a
  future equipped weapon would normally allow an adjacent attack.
- **Frontier Scout — Ashwise:** Ash costs 1 movement instead of 2.
- **Spearman — Interception Stance（迎撃姿勢）:** gains +2 combat defense against an attacker that moved
  at least 2 tiles before attacking.
- **Dawn Chirurgeon — Field Treatment:** heals self or an adjacent wounded ally
  for 8 HP without consuming an expedition item, then ends the action.

## Enemy AI (`takeEnemyTurn`)

Deterministic, no randomness:

1. Find the nearest living player unit by Manhattan distance.
2. If already in weapon range, attack it (or any other in-range player unit
   if the nearest one somehow isn't reachable by the weapon).
3. Otherwise move to whichever reachable tile minimizes distance to that
   target.
4. Attack if now in range; otherwise just end the turn.

This is intentionally simple and class-agnostic for now — a hook for
class-specific AI later without touching `BattleController`.

## Victory, defeat, and the expedition loop

The complete rules for incapacitated units, mission-specific victory and
defeat conditions, manual retreat, boss retries, pending-asset loss, and the
active-battle save marker are defined in
[`defeat_and_retreat.md`](defeat_and_retreat.md). The paragraphs below describe
the narrower behavior currently implemented by the prototype.

Reusable objective definitions, AND/OR groups, battle events, reward timing,
and AI priorities are defined in [`mission_objectives.md`](mission_objectives.md).
Region-specific objectives remain in their files under `docs/regions/`.

`BattleController::evaluateOutcome()` checks after every action: all enemies
dead → `Victory`; all players dead → `Defeat`; otherwise, if the player team
has finished acting, hand off to the enemy phase.

`GameApp` owns what happens around a battle:

- **Victory** → `proceedToCamp()`: banks that stage's loot as *pending* (not
  yet permanent). Remaining bag items carry forward unchanged.
- **Defeat** → `acknowledgeDefeat()`: all pending loot is lost, expedition
  resets to stage 0.
- **Camp → Continue Expedition**: advances to the next stage
  (`createScenarioContinuationBattle`), keeping surviving players' current
  HP and spawning a fresh enemy roster (with per-stage terrain).
- **Camp → Return to Base**: pending loot becomes permanent ("Loot
  Secured"), then a brand new expedition starts at stage 0.

Three scripted mission stages exist today (`kMissionNames` in `GameApp.cpp`),
with terrain generated per expedition rather than stored as fixed layouts:
Cinderwatch Gate (stage 0, 3 enemies), Ironwatch Stores (stage 1, full 4-enemy
roster), and The Last Signal (stage 2, full roster where the first enemy is
upgraded into the "Former Captain" — +10 max HP, +2 DEF — matching the
regional-boss beat in the scenario doc).

## Deliberately not implemented yet

Character levels/XP, permanent death, hit chance, critical hits, weapon
durability, the weapon triangle, diagonal movement, and class-specific
battlefield abilities. See the design doc history and `frontier_setting.md`
for what any of these might become.
