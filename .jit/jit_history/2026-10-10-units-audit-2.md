# Record: the units and combat spec audited again against the core

Status: done, 2026-10-10. Previous: `2026-10-10-counterspy-district.md`. A second read of specs/civ6/05-units-and-combat.md (the first is `2026-10-09-units-audit.md`) against the core found one rule off; the rest (unit stats, movement, the strength formula, combat resolution, XP, healing, formations, upgrades, nuclear weapons) match. Chosen by Claude under James's standing consent.

## Fix

- **Apostles, Inquisitors and Rock Bands are civilians** (05: Stacking, "civilian layer (... religious units)"; "Religious units can only be removed by military units at war via Condemn Heretic"). The rules generator put every class but "civilian" and "Support" on the military layer, and their classes in the data are "Religious Apostle", "Religious Inquisitor" and "Rock Band". So they could not share a plot with their own military unit or be escorted, an enemy at war fought and killed them (3 XP, kill war weariness) instead of condemning them (Condemn Heretic's pressure loss and World Religion's Favor never fired), and they counted as flankers. `tools/rules_gen/gen_rules.py` now puts them on the civilian layer, like Missionaries and Gurus; `data/rules/units.json` regenerated.

## Results

- 128 AI games (Small, 6 AI, turn 200) against #321: every figure within one standard error.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.
- The 30-turn golden game's rules checksum changes (its state hash file regenerated).

## Tests

- `religious_units_are_civilians` (test_combat.cpp): the four on the civilian layer, an Apostle escorted by its own Warrior, an enemy Apostle condemned (removed, the attacker advancing with no XP). It fails on the old data.
