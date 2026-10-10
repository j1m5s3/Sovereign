# Record: the diplomacy, city-states and governors spec audited again against the core

Status: done, 2026-10-10. Previous: `2026-10-10-economy-audit-2.md`. A second read of specs/civ6/08-diplomacy-city-states-governors.md (the first is `2026-10-09-diplomacy-audit.md`) against the core found 4 rules off; 3 are fixed here and the counterspy's district is left for its own change. Chosen by Claude under James's standing consent.

## Fixes

- **A conquered city-state can be liberated** (08: Diplomatic Favor, "liberating a city-state ... +100"). A city-state has one city, so losing it ended the player, and `canLiberateCity` wanted the original owner alive: the Favor for liberating a city-state could never be earned, and the AI's rule handing city-state cities back never fired. A city-state's city taken from its conqueror may now be liberated; the city-state comes back to life with its city as its capital (granted buildings restored). A fallen major civ is not revived (Sovereign reading: its dynasty ended with its ruler), so `FAVOR_FOR_REVIVE_PLAYER` stays unused. Free Cities are never liberated to.
- **Allies do not denounce each other** (08: Declaration of Friendship and alliances). Since allies renew an alliance without a running friendship (`2026-10-09-diplomacy-audit.md`), an ally whose friendship had lapsed could denounce mid-alliance, opening a formal war and the Betrayal emergency. `canDenounce` now refuses while an alliance runs.
- **Industrial (and Militaristic) envoy Production is not for projects** (08: Envoy tier bonuses, "toward districts and buildings"). `envoyProduction` sent every item that was not a unit or district to the buildings bonus, projects included (space race, nuclear, district projects).

## Results

- 128 AI games (Small, 6 AI, turn 200) against #319: 1 to 3 games of 128 change at all; every figure within one standard error.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.

## Tests

- `a_conquered_city_state_is_liberated_back_to_life` (test_rules_gaps.cpp; a fallen major is not), `allies_do_not_denounce_each_other` (test_diplomacy.cpp), a project case in `a_civ_s_unique_building_earns_its_base_s_envoy_bonus` (test_citystates.cpp). All fail on the old code.
- Mutation check: 6 mutants (each fix undone, the city-state condition widened to any dead player, the revival or capital dropped); the tests catch all 6.

## Left open

- **A counterspy guards one district** (08: Espionage, "Counterspy (16 turns; defends a district in your city)"): one counterspy now raises the odds against every operation in its city. It needs a guarded district on the agent (save format), on the command and in the AI.
