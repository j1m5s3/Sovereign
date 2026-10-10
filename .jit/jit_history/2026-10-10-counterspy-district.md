# Record: a counterspy guards one district and those beside it

Status: done, 2026-10-10. Previous: `2026-10-10-diplomacy-audit-2.md`, which left this open. Chosen by Claude under James's standing consent.

## Change

- **A counterspy guards a district** (08: Espionage, "Counterspy (16 turns; defends a district in your city)"). Before, one counterspy raised the odds against every operation in its city. Now it guards one plot, the City Center or a complete district of the city, and the districts beside it (engine; unverified: the spec names one district; Civ VI's guarded district covers its neighbours, the reading taken here). Only a counterspy guarding the operation's target (`Game::spyAim`: its district, else the City Center) makes the operation harder or the escape easier for the defender.
- `Agent::guard` keeps the guarded plot (save version 98). `Command::spyMission` takes an optional plot; one that is not the city's center or a complete district of it, or none, lets the core pick `Game::counterspyPlot`: the plot covering the most of the city's districts with the center counted, the center first on ties. The AI sends its counterspies without a plot, so they take that pick; the Unreal client keeps compiling (the plot argument defaults) and can add a district choice later.
- `Game::spyEscapeNeed` holds the escape roll, so tests can read it.
- The 08 spec line now names the neighbouring districts and the default.

## Results

- 128 AI games (Small, 6 AI, turn 200) against #320: identical figures (inferred: AI operations there rarely meet a counterspy away from its pick).
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.

## Tests

- `a_counterspy_guards_its_district_and_those_beside_it` (test_espionage.cpp): success odds and escape need with the counterspy on the Campus, the Hub or the center, the default pick and its tie, the command's chosen and fallback plots, a save round trip.
- Mutation check: 7 mutants (reach widened or narrowed, the tie broken the other way, every counterspy counted on escape, the chosen plot or the guard dropped, the guard lost on load); the tests catch all 7.
