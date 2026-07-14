# JOJIFrontier Agent Guide

Use this file as the default entry point for future work in this repository.

## Documentation authority

Read `docs/README.md` before changing design documents. It classifies files as
authoritative specifications, explanatory documents, or progress records.

- Put stable IDs, values, behavior, exceptions, save fields, and acceptance
  criteria only in the owning authoritative specification.
- Explanatory documents summarize and link; they do not define new rules.
- Progress records describe implementation state and never override a spec.
- When two specs overlap, consolidate the rule into the owner listed in
  `docs/README.md` and replace the duplicate with a link.

## Required world reference

Shared setting canon is owned by the sibling repository at
`../JojiWorldBible`. Before changing names, kingdoms, classes, locations,
magic, technology, factions, story, or tone:

1. Read `../JojiWorldBible/AGENTS.md` completely.
2. Read `../JojiWorldBible/README.md` and `CANON_POLICY.md`.
3. Follow the Bible routing table to the defining file.
4. Treat the defining Bible file as authoritative over Frontier docs or code.
5. State which Bible files informed the change.

Never use `WORLD_BIBLE_COMPILED.md` as source canon. It is derived output.

If the sibling repository is not present, do not invent shared canon. Keep the
change engine-local, label it as a proposal, and report that canon validation
could not be completed.

## Routing for Frontier work

| Work | Bible sources | Frontier sources |
|---|---|---|
| Embermarch setting | `docs/world/embermarch.md`, `military.md`, `heraldry.md`, `naming.md` | `docs/frontier_setting.md` |
| Classes and weapons | `military.md`; for reusable SRPG roles, `docs/story/war_of_rivermark/class_tree_ja.md` | `docs/class_reference.md`, `data/classes.json`, `data/weapons.json` |
| Items and expedition economy | `docs/story/war_of_rivermark/game_systems_ja.md`, `docs/world/economy.md` | `docs/battle_system.md`, `ExpeditionState` |
| Magic or healing | `docs/world/magic.md`, `continent.md` | class/weapon data and combat rules |
| Characters and names | `naming.md`, relevant kingdom profile, `characters.md` | `data/units.json`, scenario docs |
| Locations and missions | `locations.md`, `docs/world/geography.md`, relevant kingdom profile | `BattleFactory`, `GameApp`, scenario docs |

## Project boundaries

- `JojiWorldBible` owns shared canon.
- `JojiFrontier` owns its 3x8 rules, expedition scenarios, UI, tests, and
  engine-specific class implementations.
- `Joji Strategy Engine` is a code/reference source for tactical algorithms,
  not a dependency and not a second canon source.
- `JojiKingdomEngine` may later provide scenario seeds, but Frontier must not
  depend on its runtime.

Do not copy Bible prose into JSON or C++ data. Link to canon and encode only the
small IDs, numbers, and scenario facts required by the game.

## Class-change workflow

1. Check the current implementation in `data/classes.json` and `UnitClass`.
2. Check Embermarch's typical roster in the Bible.
3. Use the Rivermark class tree only for role/mechanic patterns that are not
   specific to its named campaign.
4. Record the mapping and canon status in `docs/class_reference.md`.
5. Add/update parsing, display names, data, and tests together.
6. Do not introduce level/promotion assumptions unless Frontier explicitly
   adopts them; its current progression is facility- and option-based.

Run `sh scripts/check_world_bible.sh` to verify that the expected canon sources
are available, then run the normal build and CTest suite.
