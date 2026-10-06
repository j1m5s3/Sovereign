# Plan: natural wonders (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-villages.md`. Chosen by Claude under James's standing consent: natural wonders from 01-map-and-terrain, which the map never had. One milestone, one PR.

Specs: 01-map-and-terrain (Map sizes: natural wonders per size; Natural wonders); data/terrain-features-resources (Natural wonders).

## Milestone

1. **Done: natural wonders** as generated features (`naturalWonder`, `tiles`, `adjacentYields`, `doublesAdjacentTerrain`, `appeal`, `freshWater`, `impassable`, `validTerrains`; 34 rows in terrain.json): the map script places the map size's count (Duel 2, Tiny 3, Small 4, Standard 5, Large 6, Huge 7) on clusters of valid, unowned, featureless plots without resources, at least 4 from any start. Their own yields come through the feature yields; neighbouring plots gain their adjacent yields, and Torres del Paine doubles neighbours' terrain yields; appeal, fresh water and impassability use the feature paths already there. No city, district or world wonder may stand on one. Unreal: their plots are tinted gold.

## Decisions (Claude's recommendations; James gave standing consent)

- Footprint: the first plot plus enough valid neighbours (any shape; Civ's exact shapes are not in the data).
- Special effects (added after, `Game::nextToNaturalWonder`): the Dead Sea heals land units ending beside it fully; Lysefjord gives ships ending beside it a promotion's XP; the Giant's Causeway gives land units beside it +5; Ik-Kil +50% production toward a wonder built beside it; Païtiti +4 Gold on international routes from the city owning it; Pamukkale +1 amenity per natural wonder in its city's land.
- Not modelled: the permanent unit abilities (Everest's hill movement, Fountain of Youth's healing, Bermuda's movement and teleport), discovery era score.
- Found on the way: an early relic (now possible from tribal villages) won a Culture victory on turn 59 of an all-AI duel, because each visiting tourist also comes off the rival's domestic tourists (07, engine). Sovereign floor: a Culture victory also needs at least 5 visiting tourists per rival major (`cultureVictor`); the AI pace test and `test_eras` cover it.
