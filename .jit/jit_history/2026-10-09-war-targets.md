# Record: the AI starts only wars it can carry to a city (step 6, "everything else")

Status: done, 2026-10-09. Previous: `jit_history/2026-10-09-stalled-wars.md`, whose "Left open" this follows. Chosen by Claude under James's standing consent.

## What was wrong

- **Most wars the AI started took nothing.** It picked as a target the weakest civ with a city within 14 plots of one of ours (`kNeighbourRange`) that it outmatched by its posture's war ratio (130% of the target's military strength by default, 110% under the Domination strategy, 15 points less for the conqueror agendas; `diplomacy`, ai.cpp).
- In 64 AI games (Small, 6 AI, seeds 1000-1063, to turn 200, with the stalled-war peace), the declaring civ took a city in 146 of its 541 wars (27%). It did far better against close targets (35% within 5 plots of one of its cities, 28% at 6-8, 15% at 9-11, 13% beyond) and with a wide margin (15% under 150% of the target's strength, 23% at 150-199%, 36% at 200-299%, 32% beyond).

## Fix

- A war target's city must lie within `kWarRange` (9) plots of one of ours; `kNeighbourRange` (14) still marks neighbours for the AI's posture and diplomacy.
- Starting a war needs `kWarMargin` (50) points over the posture's war ratio: 180% by default, 160% under Domination, 135% against an emergency's target (three quarters, as before); a grudge lowers it as before. Keeping a war going or making peace uses the posture's ratio unchanged.
- `ai_soak_takes_a_capital_and_replays` moved from seed 87, where no AI takes a capital within 250 turns any more, to seed 83 (a capital falls on turn 83; on main it falls on turn 73 of seed 87).
- Tried on seeds 1-128 (against the stalled-war build): the margin alone (science +4.6 ± 0.9) and the range alone (+2.4 ± 0.9); both together gave +5.5 ± 0.9.

## Results

256 seeds (Small, 6 AI, turn 200; seeds 129-384, run after the choice), each game set against the same seed on main (with the stalled-war peace of #237):

| Build | cities | pop | techs | civics | era | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|---|
| main | 8.7 | 60.8 | 32.2 | 22.8 | 3.4 | 117.6 | 68.5 | 161.1 | 196.6 |
| this | 8.8 | 62.2 | 32.7 | 23.1 | 3.4 | 123.0 | 71.8 | 166.0 | 204.7 |
| change | +0.09 ± 0.04 | +1.39 ± 0.29 | +0.46 ± 0.06 | +0.31 ± 0.04 | +0.07 ± 0.01 | +5.4 ± 0.7 | +3.3 ± 0.4 | +4.9 ± 1.1 | +8 ± 3 |

(± one standard error of the mean change.) Against the main of the morning, before #237: science +12.1 ± 0.9 (+11%), population +4.7 ± 0.3, production +13.0 ± 1.1.

Wars, seeds 1000-1063: 351 wars instead of 541, the declarer taking a city in 132 (37%) instead of 146 (27%), and losing one of its own in 4 instead of 19. Seeds 1000-1031: at war on 12.2% of the civs' turns (19.2% with #237, 29.5% before it), 5.8 wars a game (9.4), 99 cities changing hands in wars (115).

Speed: the 150-turn benchmark game (seed 5) takes 1.21 s of CPU against 1.28 s on main (4 runs each), with fewer armies on the march.

Save and reload: 8 AI games in 5 setups, each saved and loaded once or twice, end on the straight game's state.

## Tests

- `ai_starts_only_wars_it_can_carry_to_a_city`: the weak-neighbour setup with the target 9 plots away and three times as strong is picked (war declared or denounced first); 10 plots away it is not; a little over twice as strong it is, at 1.75 times it is not; in an emergency against it, 1.75 times is enough and 1.25 times is not. Three of its checks fail on main.
- A mutation check of the new lines: 5 mutants, all fail the tests.

## Left open

- On main, seed 83 of the soak test ends a turn with a levied archer standing on its city-state's Builder: Levy Military hands the city-state's units over where they stand, and the test counts a plot shared with another player's unit. With this change seed 83 never levies there. Moving levied units off shared plots is not done.
- The AI still values its army by total strength, wherever the units stand; what reaches the target is not weighed.
