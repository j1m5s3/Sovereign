# Record: units and combat rules the spec audit found off (05)

Status: done, 2026-10-09 (parts 1, 2 and 3). Previous: `2026-10-09-theater-square.md`. A read-only audit compared specs/civ6/05-units-and-combat.md with the core, leaving out what `2026-10-06-spec-audit-gaps.md`, `2026-10-06-spec-audit-part-2.md` and the decisions already settle, and confirmed 12 rules that differ in code. Chosen by Claude under James's standing consent; one PR per part.

## Part 1: healing, XP and borders

- **Medics and Supply Convoys heal** (05: Healing): +20 HP a turn to friendly units within a plot (`ABILITY_MEDIC_HEALING`). Only Chaplain Apostles used to heal; now every unit with a heal aura does (`Game::healAndFortify`). Healers do not add up: a unit gets the best aura within a plot (Sovereign reading of the base game, where two Medics heal no more than one; two Chaplains used to add up). As before, only a unit that did not move or attack heals.
- **A unit a city or Encampment fires on earns XP** (05: XP, "district attacking a unit 2"; `EXPERIENCE_DISTRICT_VS_UNIT`), if it survives (`CityStrike` in `Game::applyCombat`).
- **An alliance opens borders** both ways while it lasts (05: Borders; `Game::grantsOpenBorders`), so allies' units pass, their tourism counts as across open borders, and an Open Borders deal between allies is refused as already in force.
- **Traders and Great People ignore closed borders** (05: Borders; `Game::moveTraits`): a Trader, and a unit carrying a Great Person (`Unit::greatPerson`). The data gives neither the ability.
- Docs: spec 05 now says roads bridge rivers from the Classical era, as 01 and the data do; the 2026-10-06 audit's note on Amphibious points to #198, which made Amphibious units cross rivers at no extra cost.

Results:
- 128 AI games (Small, 6 AI, turn 200) against main: 13 games change; the averages dip slightly (science 130.0 to 129.7, culture 76.1 to 75.9, gold 219 to 216; t about -2). Inferred: allies can no longer trade Open Borders to each other, and some units now take other paths.
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; 7 of them play out differently.
- Tests: `medics_and_supply_convoys_heal_the_units_beside_them`, `an_alliance_opens_borders`, Traders and Great People in `units_that_ignore_borders_cross_closed_ones`, the struck unit's XP in `walled_city_strikes_once_per_turn`. Mutation check: 9 mutants, all caught (a unit two plots from a Medic was added to catch the last).

## Left open

- **City-states' borders:** spec 05 says city-states let everyone in unless at war, but Gunboat Diplomacy ("open borders with city-states you sent an Envoy to") and Portugal's ability ("Open Borders with all city-states", 09) only make sense if they close. The core keeps them closing at Early Empire, like majors'.

## Part 2: combat strength

- **Attacks on an Encampment** count the river and flanking at the Encampment, not at the city center (`unitStrength`'s new `at`, passed by `previewAttack` and `attackEncampment`).
- **Interception uses the strength formula** (05): a patrolling fighter fights with its full strength less the wounded penalty, an anti-air gun with its anti-air strength (+25 with Air Defense Initiative) less the same penalty, and the aircraft it hits defends with its own (`Game::interception`, `applyCombat`). Before, both sides scaled by health. The wounded penalty is now one function, `Game::woundedPenalty`.
- **Aircraft fight in the air:** no terrain, fortification, river, flanking or support counts for an aircraft (`unitStrength`'s `air`).
- **Corps and Armies get +7 against aircraft** instead of +10/+17 (05; `COMBAT_CORPS_ANTIAIR_STRENGTH_MODIFIER`, `COMBAT_ARMY_ANTIAIR_STRENGTH_MODIFIER`).
- **A land unit attacking from the water gets -10** (05; `COMBAT_AMPHIBIOUS_ATTACK_PENALTY`), melee or ranged.
- **An embarked defender starts from its era's strength** and keeps the bonuses after it (difficulty, formation, Military Advisory and the rest); before, the era strength replaced the ones added ahead of it.

Results:
- 128 AI games (Small, 6 AI, turn 200) against part 1: all 128 play out exactly the same (inferred: these fights are rare before turn 200, with no aircraft yet).
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; 5 of them play out differently from part 1.
- Tests: `an_encampment_attack_counts_the_river_and_flanks_at_the_encampment` (the preview and the attack), `interception_uses_the_strength_formula`, `corps_and_armies_get_less_against_aircraft`, `embarked_units_keep_their_modifiers_and_attack_from_the_water_weaker`. Mutation check: 11 mutants, all caught (the attack itself was added to the Encampment test to catch the last); a 12th, dropping `at` entirely, does not build.

## Part 3: plots and escorts

- **A melee victor takes every unit left on the plot** (05: Stacking): each civilian or support unit the defender escorted is captured (Settlers and Builders) or destroyed (the rest), as when a unit walks onto unguarded civilians. Before, only the first was, and our unit could end up sharing the plot with another civ's (`Game::applyCombat`).
- **Civilians can be linked to a military escort** (05: Formations), as the leader could: `LinkEscort` takes a leader or a civilian on the escort's plot, never a support unit. The pair moves together at the slower unit's pace, an order to either moves both, the escort needs no orders of its own, and the linked pair keeps out of other players' units (`Game::escortOf`, `advanceUnit`, the path search).

Results:
- 128 AI games (Small, 6 AI, turn 200) against part 2: all 128 play out exactly the same (inferred: a melee kill on a plot holding a civilian and a support unit is rare, and the AI links no escorts).
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; all 8 end exactly as with part 2.
- Tests: the Battering Ram in `kill_advances_and_captures_civilians`, `a_civilian_moves_with_its_linked_escort`. Mutation check: 8 mutants, all caught.

## Left for later

- The Unreal L key still links an escort to the leader only; a civilian's link needs the same key on civilians (Unreal side).
- The AI links no escorts: whether escorted Settlers would settle more cities is untested.
