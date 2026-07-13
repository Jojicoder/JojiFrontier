# JOJIFrontier Class Reference

## Canon status

JOJIFrontier is set around Embermarch, so its roster should first express
Embermarch's canonical military doctrine. The World Bible defines archetypes,
while this repository defines exact stats and battlefield mechanics.

Primary sources:

- `../../JojiWorldBible/docs/world/embermarch.md`
- `../../JojiWorldBible/military.md`
- `../../JojiWorldBible/docs/story/war_of_rivermark/class_tree_ja.md`
- `../../JojiWorldBible/docs/world/magic.md`

The Rivermark class tree is authoritative for that named campaign, not a universal
class list for every Joji game. Frontier may reuse its mechanical roles without
claiming that every Rivermark title exists in Embermarch.

## Implemented core roster

| Frontier class | Bible basis | Battlefield rule | Exploration contribution |
|---|---|---|---|
| March Captain | Embermarch command culture | MOV 4; adjacent ally receives defense +1; no stacking | military orders and negotiation |
| Veteran Guard | Embermarch typical unit | entering an adjacent tile ends enemy movement | force and endurance choices |
| Watch Archer | Embermarch typical unit | MOV 4; range 2-3; cannot attack adjacent | scouting and deployment preview |
| Frontier Scout | Embermarch typical unit | MOV 5; Ash costs 1 | hidden routes and encounter warning |
| Spearman | Embermarch typical unit | defense +2 against attackers that moved 2+ tiles | hold crossings and escort actions |
| Dawn Chirurgeon | Embermarch Church of Dawn healers | heals self/adjacent ally for 8 HP; action ends | medicine and casualty choices |

The playable starting party is March Captain, Veteran Guard, Watch Archer, and
Dawn Chirurgeon. Frontier Scout and Spearman are loaded as reserve roster data;
party selection UI is the next requirement before they can be deployed normally.

Bandit remains as the generic enemy-only raider class. Enemy archers and spear
users now use the same Watch Archer and Spearman rules as player units.

## Planned expansion roster

Heavy Infantry, Frontier Engineer, Messenger Cavalry, Banner Bearer, Frontier
Ranger, and Battle Mage remain planned. Ranger is a tracker/trapper under current
canon, not a monster-hunter class; Asteria's wildlife is not presently a combat
system. Battle Mage should remain tied to a rare named specialist.

## Reusable Rivermark patterns

Useful mechanical patterns from `class_tree_ja.md` include Scout, Field Healer,
Skilled Spearman, Archer, Heavy Infantry, Engineer, Messenger Cavalry, and support
banner roles. Names tied to Linus's campaign, its promotions, or its unique cast
should not be copied into Frontier without a deliberate cross-project decision.

## Rules that remain Frontier-specific

- No character levels or level-gated promotions.
- Growth comes from facilities, equipment traits, class techniques, and roster
  options.
- Basic attacks remain deterministic and always hit.
- Classes should differ through movement, range, terrain interaction, protection,
  and action options rather than stat inflation alone.
