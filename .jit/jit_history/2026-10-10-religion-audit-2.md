# Record: the religion spec audited again against the core

Status: done, 2026-10-10. Previous: `2026-10-10-units-audit-2.md`. A second read of specs/civ6/06-religion.md (the first is `2026-10-09-religion-audit.md`) against the core found six rules off; the rest (founding, beliefs, pressure ranges, spread charges, theological combat damage, Condemn Heretic, the Religious victory) match. Chosen by Claude under James's standing consent.

## Fixes

- **Religious units carry their ability classes.** The rules generator gave Missionaries, Apostles, Inquisitors and Gurus no tags, so abilities granted by class (`RELIGIOUS`, `RELIGIOUS_ALL`, `MISSIONARY`, `INQUISITOR`, `GURU`: Theocracy's +5 religious strength, the Inquisition card and others) reached none of them. `tools/rules_gen/gen_rules.py` now tags them as the base game's TypeTags do; `data/rules/units.json` regenerated.
- **Missionaries and Gurus do not attack** (data: their `ABILITY_PEACEKEEPER`). The spec said every religious unit can start theological combat; the data wins, and the 06 line now says so. `Game::unitHasAbility` added.
- **Passive pressure keeps its tenths.** A plain city's 1 pressure a turn lost Scripture's +25% and the Bishop's percent to integer rounding; `City::pressureTenths` carries the remainder (save version 99).
- **Great Prophets stop once every religion is claimed**: founded, or a Prophet already recruited toward one, not only founded.
- **God of the Forge** is for Ancient and Classical units only (`maxEra` in `data/rules/modifiers.json`).
- **The Inquisition's +15** counts once when the Dark Age card already grants the same ability.

## Left open

- Holy Waters heals near any Holy Site of a city following your religion; the core counts your own cities only.
- Dar-e Mehr's Faith per era.
- Other ability classes no unit carries yet (MEDIC, TRADER, SETTLER and others in promotions.json) not reviewed.

## Results

- 128 AI games (Small, 6 AI, turn 200) against #322: every figure identical (the AI never reaches these paths in them).
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.
- Mutation: 5 of 6 mutants caught; the one missed (`unitHasAbility` true for a missing ability) cannot happen with the shipped rules, which carry both abilities.
- The 30-turn golden game's state hash file regenerated (rules and save version changed).

## Tests

`theocracy_strengthens_religious_units`, `missionaries_do_not_attack`, `scripture_adds_to_a_plain_citys_pressure`, `prophets_stop_once_every_religion_is_claimed`, `god_of_the_forge_is_for_ancient_and_classical_units` and an Inquisition card case in `an_apostle_launches_the_inquisition` (test_religion.cpp). Each fails on the old code.
