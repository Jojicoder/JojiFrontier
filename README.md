# JOJIFrontier

A browser-playable (eventually) fantasy tactics RPG prototype set on Asteria's
Embermarch frontier: 4-character party combat on a fixed side-view battlefield, with
Fire Emblem-inspired classes, deterministic damage, and an expedition
risk/reward loop in place of character levels.

This repository contains the first playable **Silent Posts** expedition slice:
three linked battles, a Camp screen, shared healing supplies, pending loot, and
the Continue/Return expedition loop. Text exploration and full base building are
not implemented yet.

## Build

Requires CMake 3.20+ and a C++20 compiler. Raylib is picked up via
`find_package` if already installed (e.g. `brew install raylib`);
otherwise CMake fetches raylib 5.5 and nlohmann/json automatically via
`FetchContent`, so a plain checkout builds standalone.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/JOJIFrontier
```

Run it from a directory containing `data/` and `assets/` (the build
copies both next to the executable automatically as a post-build step,
so `./build/JOJIFrontier` works out of the box).

## Controls

Mouse only:

- **Click a highlighted unit** during Player Phase to select it.
- **Click a blue-highlighted tile** to move the selected unit (or click its
  own tile to stay in place).
- **Attack / Items / Wait / Back** buttons in the bottom HUD choose or revise
  the unit's action. A potion restores 8 HP and ends that unit's action.
- **Click a red-highlighted tile** to pick an attack target.
- **Confirm / Cancel** in the bottom HUD resolves or backs out of the
  attack after reviewing the combat preview.
- Victory/Defeat/Camp screens are click-through buttons
  (`Proceed to Camp`, `Return to Base`, `Continue Expedition`, `Continue`).

## Implemented in this milestone

- 3x8 battlefield geometry, one unit per tile, with seeded terrain generated
  anew for each expedition and field type.
- Five terrain types (Floor, Ash, Rubble, Barrier, Watch Post), weighted
  movement costs, impassable barriers, and Watch Post defense bonuses.
- 4-unit player party (March Captain, Veteran Guard, Watch Archer, Dawn
  Chirurgeon), with Frontier Scout and Spearman reserve data, versus a 4-unit
  raider roster, loaded from
  `data/classes.json`, `data/units.json`, `data/weapons.json`, and
  `data/terrain_profiles.json`.
- Deterministic combat: `STR/MAG + weapon Might - target DEF/RES`,
  floored at 1 damage. No hit chance, no crits, no variance.
- Fire Emblem-style Player Phase / Enemy Phase turn structure; each unit
  acts once per phase (`hasActed`), then phases alternate automatically.
- Orthogonal weighted movement respecting `move`, terrain, occupancy, allied
  pass-through, and Veteran Guard Zone of Control
  (`jf::computeReachableTiles`), reused by both the player flow and AI.
- `SelectUnit -> SelectMove -> SelectAction -> SelectTarget ->
  ConfirmAttack -> SelectUnit` input state machine
  (`jf::BattleController`), fully independent of raylib.
- Simple deterministic enemy AI: nearest-target-by-Manhattan-distance,
  attack if in range, otherwise close the distance and attack if now
  possible.
- Three linked Silent Posts encounters with field-specific random terrain and enemy setups,
  stage-specific loot, survivor HP carryover, and a final Former Captain boss.
- Victory/Defeat detection, Camp party status, expedition-long potion attrition, pending loot,
  and the Continue Expedition / Return to Base loop.
- Expedition loot is only "pending" until a safe return to base; a
  defeat clears it (`jf::ExpeditionState`).
- Clean separation of rules (`jf::battle`, `jf::core`), data
  (`jf::data`), and rendering (`src/main.cpp`, raylib-only) — battle
  logic has no raylib dependency and was verified headlessly (movement,
  damage formula, turn cycling, AI, and the attack/preview/confirm flow
  all checked in isolation from the renderer).

## Known limitations (by design, for this milestone)

- No character levels, XP, permanent death, critical hits, hit chance,
  or weapon durability.
- No weapon triangle or diagonal movement.
- No text exploration or full base-building systems yet.
- Class-specific battlefield behavior and AI are not yet differentiated
  beyond stats/weapon range.
- Placeholder circular units and simple movement/phase animation.
- Automated CTest coverage for weighted movement, barriers, attack range,
  terrain defense, move rollback, and healing items.

## Setting and reuse

- [`docs/regions/ashbough_forest.md`](docs/regions/ashbough_forest.md)
  records the proposed three-node introductory forest region.
- [`docs/save_system.md`](docs/save_system.md) defines versioned desktop and
  browser persistence, migration, backup, and export/import rules.
- [`docs/exploration_system.md`](docs/exploration_system.md) defines the
  node-based text exploration flow and its battle consequences.
- [`docs/base_development.md`](docs/base_development.md) records the proposed
  settlement stages, six facility trees, and shared unlock conditions.
- [`docs/frontier_setting.md`](docs/frontier_setting.md) defines the proposed
  Embermarch expedition scenario without overriding shared World Bible canon.
- [`docs/reuse_plan.md`](docs/reuse_plan.md) records what can be adapted from
  Joji Strategy Engine and JojiKingdomEngine, and what should remain separate.
- [`docs/class_reference.md`](docs/class_reference.md) separates currently
  implemented classes from World Bible archetypes and the recommended
  Embermarch roster.

Future setting and class work must follow [`AGENTS.md`](AGENTS.md). Verify the
sibling canon repository with:

```bash
sh scripts/check_world_bible.sh
```

## Project layout

```
JOJIFrontier/
├── CMakeLists.txt
├── assets/                 # placeholder art (currently empty)
├── data/                   # unit, class, weapon, and terrain profile definitions
├── include/jf/
│   ├── battle/              # BattleState, BattleController, Movement, CombatResolver, EnemyAI, BattleFactory
│   ├── core/                # Stats, Weapon, UnitClass, Unit, Grid, ExpeditionState, GameApp
│   └── data/                # GameData loader
└── src/
    ├── main.cpp              # raylib window/input/rendering only
    ├── battle/
    ├── core/
    └── data/
```

## Future WebAssembly build direction

The CMake setup already branches on `EMSCRIPTEN`: `find_package(raylib)`
is skipped and raylib is fetched/built for `PLATFORM_WEB`, `data/` and
`assets/` are preloaded into the virtual filesystem via
`--preload-file`, and the output target is suffixed `.html`. Because
`jf_lib` (all game rules, battle state, and data loading) never calls
into raylib directly, porting to the web is expected to only touch
`src/main.cpp`'s windowing/input glue, not the battle logic itself.

To build for the web once `emscripten` is installed and activated:

```bash
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
```

This produces `JOJIFrontier.html/.js/.wasm`, which can be committed to
a `gh-pages` branch (or a `/docs` folder on `main`) for GitHub Pages
hosting.
