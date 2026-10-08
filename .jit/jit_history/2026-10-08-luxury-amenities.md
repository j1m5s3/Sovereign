# Record: luxuries reach the cities that need them most (step 6, "everything else")

Status: done, 2026-10-08. Chosen by Claude under James's standing consent, after a look at the AI's economy at turn 200.

Each luxury type gives +1 Amenity to up to 4 of its owner's cities, those that need them most (02: Amenities).

## What was wrong

- **A civ's fifth and later cities got no luxury Amenities.** Since the rules core, each luxury went to the four largest cities, a stand-in for Civ's "neediest first" (`2026-10-05-rules-core.md`, Decisions). With 8 cities a civ by turn 200, the four largest held every luxury, far beyond what they asked (seed 1: London had 15 Amenities for the 6 it needed), and the rest had none. 36% of the AI's cities were short at turn 200 (Displeased: -10% yields, -15% growth), so the smaller cities stayed small.

## Fix

- `Game::luxuryShares` deals each luxury type, one at a time, to the cities whose population asks the most Amenities that luxuries have not given yet (`CITY_POP_PER_AMENITY`), then the larger, then the older city. Buenos Aires' bonus resources are dealt the same way after the luxuries. A run of city reports works it out once (`ReportShare::luxuryShares`); a city not among its owner's gets none.
- Sovereign reading: a city's need is what its population asks, not its whole Amenity balance. The other sources (Palace, districts, buildings, policies, great people, stances) come out of the whole city report, so dealing by them would take a report of every city before any one.
- Specs: 02 Amenities says which cities a luxury reaches.

## Results

8 AI games (Small, 6 AI) at turn 200:

| Build | cities short of Amenities | cities at +5 or more | population per city |
|---|---|---|---|
| main | 36% | 40% | 6.09 |
| this | 1% | 22% | 6.10 |

64 seeds (Small, 6 AI, turn 200):

| Build | cities | pop | techs | civics | science | culture | prod |
|---|---|---|---|---|---|---|---|
| main | 8.0 | 47.6 | 28.9 | 20.7 | 71.6 | 49.7 | 114.1 |
| this | 8.2 | 49.2 | 29.1 | 20.8 | 71.8 | 50.2 | 117.7 |

Speed: the 8 games to turn 150 took 9.4 s of CPU against 9.5 s on main, and 20 turns of a Huge 12-AI game from turn 250 0.93 s on both (medians of 5 runs).

Save and reload: 8 games in 5 setups, each saved and loaded once or twice, end on the straight game's state. The four replay games of the speed-up checks replay their own command logs; their end states move, as for any rules change.

## Tests

- `luxuries_reach_the_cities_that_need_them_most` replaces `a_luxury_reaches_the_largest_cities_first_then_the_oldest` and keeps its two cases (one luxury: the largest cities, then the first founded). Three luxuries over five cities, the capital asking 4 Amenities and the others 1, give 3, 3, 2, 2, 2 (the four largest took all twelve before), with or without another civ's larger cities beside them. Cities that ask none share what is left, the larger first, none getting a second before each has one, and get none while others still ask.
- `luxury_policy_and_deforestation_treaty` also checks that Luxury Policy A takes a luxury to six cities of six (four without it).
- Mutation check: 15 mutants, each part of the dealing broken alone (the need and its order either way, the size and age tie-breaks, the need taken from the population or ignored, a city's own share, other civs' cities, Luxury Policy A and B, Buenos Aires, the reach). All 15 are caught; three were caught only after the checks for other civs' cities, Luxury Policy A and a sixth city of population 1 were added.

## Left open

- Dealing by each city's whole Amenity balance rather than what its population asks.
- The AI built no Entertainment Complex by turn 200 in any of the 8 games, on either build; with luxuries spread, few of its cities need one by then.
