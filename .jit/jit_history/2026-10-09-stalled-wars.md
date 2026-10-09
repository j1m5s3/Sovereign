# Record: the AI makes peace when a war stalls and keeps a peace 30 turns (step 6, "everything else")

Status: done, 2026-10-09. Chosen by Claude under James's standing consent: wars were the largest cause of the AI's pace left after `2026-10-09-builder-value.md` (about half the civs at war at any time, 18-26% of their production on the military).

## What was wrong

- **Wars that went nowhere ran on.** An AI offered peace only when losing (below 80% of the enemy's strength), when tired (50 turns of war or two Amenities of war weariness, unless clearly winning) or when the enemy offered and it was not winning (`diplomacy`, ai.cpp). The stronger side of a war that never reached a city fought on to turn 50.
- **Peace was followed by the next war at once.** Once the 10-turn peace treaty ran out, an AI could declare on the same civ again.
- In 32 AI games (Small, 6 AI, seeds 1000-1031) to turn 200, the major civs were at war with another major civ on 29.5% of their turns: 8.7 wars a game, 25.4 turns on average, and in 46% of them no city changed hands. 131 of the 277 wars were a pair going back to war, half of them within 17 turns of the peace.

## Fix

- **A stalled war ends:** with no city of either side attacked for `kStalledWar` (5) turns, counting from the war's start (`City::lastAttackedTurn`), the AI offers peace whatever the strengths. Attacks by anyone count, so a city under siege by a third party keeps the war going. The 10-turn minimum war (`DIPLOMACY_WAR_MIN_TURNS`) still applies.
- **A peace holds `kPeaceHolds` (30) turns:** the AI picks no civ as a war target within 30 turns of making peace with it (`Relation::since`, the turn the peace began; never-fought pairs have 0 and are not held). Calls to arms and emergencies are unchanged.
- Tried on seeds 1-128 and left out: a war counted as stalled after 1, 3, 8 or 12 quiet turns, or only after 15 or 20 turns of war (science +2.2 to +3.8 for 1-8 quiet turns, +0.4 for 12, -0.3 for 20 turns and 12 quiet); the stall rule alone (science +3.8 ± 1.3); the 30-turn peace alone (science +1.3 ± 1.1); a 20-turn peace (+4.2), 40 (+5.8) and 50 (+6.6) with the stall rule. 30 turns was kept as a moderate pause: longer ones make the AI ever more peaceful for a little more pace.

## Results

256 seeds (Small, 6 AI, turn 200; seeds 129-384, run after the choice), each game set against the same seed on main:

| Build | cities | pop | techs | civics | era | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|---|
| main | 8.4 | 57.5 | 31.7 | 22.4 | 3.3 | 110.9 | 65.2 | 153.0 | 190.9 |
| this | 8.7 | 60.8 | 32.2 | 22.8 | 3.4 | 117.6 | 68.5 | 161.1 | 196.6 |
| change | +0.37 ± 0.06 | +3.34 ± 0.36 | +0.52 ± 0.08 | +0.38 ± 0.05 | +0.08 ± 0.01 | +6.68 ± 0.92 | +3.28 ± 0.47 | +8.15 ± 1.19 | +5.6 ± 3.2 |

(± one standard error of the mean change.) On seeds 1-128, where the numbers were chosen: science +5.2 ± 1.2, population +2.9 ± 0.5, production +5.1 ± 1.8.

Wars (seeds 1000-1031, to turn 200): at war on 19.2% of the civs' turns (29.5% on main); 9.4 wars a game (8.7), 13.8 turns long (25.4); 115 cities changed hands in wars (203). More wars now end with no city taken (73%), being short: the AI still declares wars it cannot carry to a city, then makes peace.

Speed: the 150-turn benchmark game (seed 5) takes 1.21 s of CPU against 1.18 s on main (4 runs each); a different game by then, with more cities and people. The new check looks over the cities once per enemy in each AI turn.

Save and reload: 8 AI games in 5 setups, each saved and loaded once or twice, end on the straight game's state.

## Tests

- `an_ai_offers_peace_when_the_war_has_stalled`: the AI, stronger, at war 15 turns: it offers peace when no city was attacked, or the last attack on the human's capital or on its own town was five turns ago; not four turns after.
- `ai_keeps_a_peace_30_turns_before_declaring_war_again`: the weak-neighbour war of `ai_declares_war_on_a_weak_neighbour` waits 30 turns after a peace with that neighbour; a never-fought neighbour is attacked at turn 20 too.
- Both fail on main. A mutation check of the new lines: 9 mutants, 7 fail the tests and 1 the build; the survivor counts the quiet turns from turn 0 instead of the war's start, the same in any game while the 10-turn minimum war is longer than the 5 quiet turns.

## Left open

- The AI still starts wars it cannot carry to a city (73% of the shorter wars take none): choosing targets by whether its army can reach and take a city is the next step on war (inferred from the counts, not traced).
