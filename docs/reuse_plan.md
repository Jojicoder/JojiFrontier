# Joji Engine Reuse Plan

## Source projects

- `../../JojiWorldBible`: source of truth for setting, names, systems, and tone.
- `../../Joji Strategy Engine`: source candidate for tactical systems and tests.
- `../../JojiKingdomEngine`: future source for campaign seeds, kingdom personality,
  season, army role, and strategic battle context.

Do not physically merge these repositories. Port small rule modules and adapt
their namespaces/data contracts to `jf`; keep attribution in the commit history
and preserve JOJIFrontier's fixed 3x8 battle assumptions.

## Reuse now

### Test structure

Port the Strategy Engine's small assert-based CTest executables first. Frontier
currently has no automated test target. Priority coverage:

- movement and occupied-tile behavior;
- attack-range union and min/max range;
- move -> action -> target -> confirm state transitions;
- Back restoring the move origin;
- deterministic damage and minimum damage of one;
- enemy AI choosing a legal attack origin.

### Terrain movement model

Adapt `MovementSystem`'s weighted search and movement-type costs rather than
extending Frontier's BFS repeatedly. Frontier needs a reduced terrain set:

- Floor: cost 1;
- Ash: cost 2 for infantry, 3 for cavalry;
- Rubble: cost 2, Armor cost 3;
- Barrier: impassable;
- Watch Post: cost 1 with a defensive bonus.

The 3x8 geometry stays fixed. Seeded field generation changes terrain placement
and tile rules per expedition, not screen geometry.

### Enemy action scoring

Port the Strategy Engine AI's useful principles, not its entire `GameState`:

- score lethal attacks first;
- use real forecast damage rather than raw attack power;
- avoid clearly losing counterattacks;
- prefer defensible tiles when no favorable attack exists.

Frontier should add an AI personality parameter later so raiders, guards, and
wild threats do not all behave identically.

## Reuse after the combat slice

### Skills

Adapt the Strategy Engine's split between passive and active skills. Keep the
first Frontier version smaller:

- passive triggers: Always, OnTerrain, WhenDefending;
- active effects: Damage, Heal, Guard;
- uses reset per expedition leg or at camp, depending on the skill.

Do not import hit chance, critical chance, durability, or level growth. They
conflict with Frontier's deterministic, level-free combat direction.

### Scenario objectives

Reuse the concept behind Strategy Engine's escape goal for Frontier encounters:
reach an exit, protect a carrier, hold a post for several turns, or recover an
object. These objectives fit expeditions better than elimination-only battles.

### KingdomEngine handoff

Later, a KingdomEngine event can seed a Frontier expedition:

- kingdom personality -> enemy AI style;
- army role -> enemy composition and starting columns;
- season -> route and terrain modifier;
- battle result -> available salvage and regional danger.

This is a data handoff, not a shared runtime dependency.

## Do not reuse directly

- Strategy Engine's large-map renderer and cursor camera;
- its level, hit, critical, durability, and permanent-death assumptions;
- its unrestricted map dimensions;
- KingdomEngine's continent-scale pathfinding or battle resolver;
- World Bible prose copied into JSON or source files.

## Recommended implementation order

1. Add CTest and port focused rule tests.
2. Introduce tile terrain data without changing the renderer geometry.
3. Replace BFS with weighted movement.
4. Improve enemy scoring using deterministic combat forecasts.
5. Add one healing item and one class technique.
6. Add the Silent Posts expedition data and non-elimination objective.
