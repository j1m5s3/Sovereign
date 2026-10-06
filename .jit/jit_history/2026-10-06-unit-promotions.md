# Plan: unit promotion effects (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-great-person-effects.md`. Chosen by Claude under James's standing consent: about 200 promotion and ability effects and conditions were kept only as text (`UNTRACKED` in `promotions.json`). Most belong to unique units of civs outside Sovereign's roster; the roster's uniques are hand-written. This plan covers the regular promotions and the shared effect forms. One milestone, one PR.

Specs: 05-units-and-combat (Promotions); data/promotions, data/units (Unit abilities).

## Milestone

1. **Done: promotion effects.** Generated, with the core reading each one:
   - `IGNORE_HILLS` (Alpine), `IGNORE_FOREST` (Ranger), `IGNORE_TERRAIN`: movement in `terrainCost`;
   - `FREE_EMBARK` (Amphibious and four unique abilities): no extra cost to embark or disembark;
   - `SEES_THROUGH_FEATURES` (Sentry): woods and rainforest do not block its sight (`lineOfSight(..., throughFeatures)`);
   - `COASTAL_RAID`: any ship with it may raid the coast;
   - `HEAL_NEUTRAL` and `HEAL_ENEMY` (Auxiliary Ships, Supply Fleet, Supercarrier);
   - `AIR_SLOTS` (Flight Deck, Hangar Deck, Folding Wings): +1 aircraft on a carrier;
   - `KILL_YIELD` (Boarding and unique abilities): Gold, Faith or Culture as a share of a killed unit's strength, optionally only against ships (`Game::killReward`).

   New combat conditions:
   - `IN_FORMATION` (Convoy) and `TILE_FORT` (Garrison, with `DISTRICT_TILE`, which now counts any district, not only a city center);
   - the opponent wounded (Rout's "unit damage minimum");
   - `COASTAL_TILE`, `HOME_CONTINENT`, `OPPONENT_MINOR`, `OPPONENT_FREE_CITY` and `NEAR_OWN_TERRITORY` for the unique abilities that use them.

## Decisions (Claude's recommendations; James gave standing consent)

- Not modelled:
  - Camouflage ("hidden, only visible when adjacent"); unit visibility is per plot;
  - Hold the Line's aura, Emplacement's city-center condition, Escort Mobility and Commando's cliffs;
  - the Giant Death Robot's promotions; the spy promotions in this table (spies use `espionage.json`'s);
  - Rock Band promotions at civs' unique districts.

## Follow-up: Apostle promotions

- Each Apostle bought gets one random promotion of `PROMOTION_CLASS_RELIGIOUS_APOSTLE` (06: "Each new Apostle gets one random promotion"; `Game::grantApostlePromotion`).
- Generated effects, with the core's readings:
  - Orator: +2 charges.
  - Debater: +20 religious strength. Unconditional `STRENGTH` on a religious unit now counts toward theological combat, which also brings in the Religious Orders and Theocratic Legacy buffs.
  - Proselytizer: removes 50% more of other religions, so 75% in all.
  - Translator: triple spread strength in other civs' cities.
  - Pilgrim: +3 charges the first time it stands next to a natural wonder.
  - Indulgence Vendor: Gold the first time it turns a city. The data says "+100 Gold when a unit is trained/bought"; the spec's reading is used.
  - Heathen Conversion: spreading turns the barbarians next to it.
  - Martyr: a Relic into a free slot if it falls in theological combat.
  - Chaplain: +20 healing for the owner's units next to it.
- The one-time Pilgrim and Indulgence Vendor gifts are marked spent with bits 0x40 and 0x80 of `Unit::wonderAbilities`, so the save format does not change.
