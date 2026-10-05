# Plan: the remaining Civ systems (step 6, item 4)

Status: active, 2026-10-05. Previous: `jit_history/2026-10-05-battle-ai.md`.

Goal: James's item 4, the Civ VI systems the core does not have yet, built to `specs/civ6/` (Sovereign docs override). The order follows dependencies, not the list order: ships and the water districts come first because movement, the AI, Harbor great admirals, trade by sea and coastal wonders all need them; great people come before religion because a Great Prophet founds a religion.

## Milestones (one PR each)

1. **Naval and embarkation:** sea movement and pathing (Coast; Ocean after Cartography), naval units trained in coastal cities, naval melee and ranged combat, embarking (Shipbuilding; Builders after Sailing), embarked strength by era, the embark cost, naval healing, and the Harbor district with its buildings. AI: trains a few ships and keeps them home; embarks to settle and attack across water later. *Built 2026-10-05:* all of it except the AI's ships. The AI does not train ships yet, and its land units march overland (a new `overland` move option), because embarked armies cannot attack and defend at 10 to 15. Teaching the AI to use the sea is a follow-up. Harbor buildings: coast-tile yields as modifiers; their +25% XP for trained ships waits for per-city XP modifiers.
2. **Great people:** points from districts and buildings, the per-era pool with rising costs, patronage with gold or faith, recruitment, and the effects that touch systems already built (generals and admirals as auras, engineers and merchants with direct effects). The rest get their effects when their system arrives.
3. **Religion:** faith, pantheons, the Great Prophet and founding, beliefs, missionaries and apostles, pressure and conversion, theological combat, holy cities, religious victory.
4. **Trade routes:** Traders, route yields by destination, capacity from Markets and Lighthouses, roads built by traders, trade by sea.
5. **Wonders:** placement and production, effects through modifiers, the ones that touch systems already built.
6. **City-states and envoys:** city-state players, envoys and suzerainty bonuses, quests left for later.
7. **Eras and ages:** era score and historic moments, golden, normal and dark ages with dedications; tourism and the culture victory.

## Decisions (Claude's recommendations; James gave standing consent)

- Rules stay data: new districts, units and buildings come from `tools/rules_gen` (extend `PLACEABLE_DISTRICTS` and friends), never hand-edited generated JSON; hand-written tuning goes in the hand-written files.
- Each milestone bumps the save version and regenerates the golden hash.
- AI behaviour for each system is simple and rule-following first; deeper planning arrives with the strategy AI work (leader doc §10, layer 1).
