# JOJIFrontier

A browser-playable (eventually) fantasy tactics RPG prototype: 4-character
party combat on a fixed Mega Man Battle Network-style battlefield, with
Fire Emblem-inspired classes, deterministic damage, and an expedition
risk/reward loop in place of character levels.

This repository contains the **first playable battle prototype** only —
one scripted battle, a Camp screen, and the Continue/Return expedition
loop. Story, exploration, and full base building are not implemented yet.

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
- **Attack / Wait** buttons in the side panel choose the unit's action.
- **Click a red-highlighted tile** to pick an attack target.
- **Confirm / Cancel** in the side panel resolves or backs out of the
  attack after reviewing the combat preview.
- Victory/Defeat/Camp screens are click-through buttons
  (`Proceed to Camp`, `Return to Base`, `Continue Expedition`, `Continue`).

## Implemented in this milestone

- 3x8 fixed battlefield, single shared grid, one unit per tile.
- 4-unit player party (Lord, Armor Knight, Archer, Mage) vs. a 4-unit
  enemy roster (2 Bandits, 1 Archer, 1 Soldier), loaded from
  `data/classes.json`, `data/units.json`, `data/weapons.json`.
- Deterministic combat: `STR/MAG + weapon Might - target DEF/RES`,
  floored at 1 damage. No hit chance, no crits, no variance.
- Fire Emblem-style Player Phase / Enemy Phase turn structure; each unit
  acts once per phase (`hasActed`), then phases alternate automatically.
- Orthogonal BFS movement respecting `move` stat and tile occupancy
  (`jf::computeReachableTiles`), reused by both the player flow and AI.
- `SelectUnit -> SelectMove -> SelectAction -> SelectTarget ->
  ConfirmAttack -> SelectUnit` input state machine
  (`jf::BattleController`), fully independent of raylib.
- Simple deterministic enemy AI: nearest-target-by-Manhattan-distance,
  attack if in range, otherwise close the distance and attack if now
  possible.
- Victory/Defeat detection, a Camp placeholder screen (party HP, pending
  loot, battles won), and the Continue Expedition / Return to Base loop.
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
- No weapon triangle, terrain cost, obstacles, or diagonal movement.
- No story, exploration, or full base-building systems — Camp is a
  placeholder screen.
- Class-specific battlefield behavior and AI are not yet differentiated
  beyond stats/weapon range.
- Placeholder rectangle art; no animation.

## Project layout

```
JOJIFrontier/
├── CMakeLists.txt
├── assets/                 # placeholder art (currently empty)
├── data/                   # classes.json, units.json, weapons.json
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
