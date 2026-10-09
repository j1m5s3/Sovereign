# Record: the AI slots policy cards by what they do for its cities (step 6, "everything else")

Status: done, 2026-10-09. Chosen by Claude under James's standing consent, after a look at the AI's economy at turn 200.

## What was wrong

- **The AI ranked policy cards by how many modifiers they have.** `policyValue` (ai.cpp) scores a card 1, plus 3 a modifier (five at most), plus 2 for a Military card while at war, and the AI put the best into each slot while changes were free. A card's text splits into a modifier per yield, building or era, so the count says little: God King (+1 Faith and +1 Gold in the capital, two modifiers) beat Urban Planning (+1 Production in every city, one), and cards whose effects are in code (Rationalism, Free Market, Public Transport and others, no modifiers) scored least. In 8 AI games at turn 200 the most held were Veterancy (by all 48 major civs), Civil Prestige (47), Limes (42), Feudal Contract (41), Charismatic Leader (37) and Maritime Industries (31), four of them production toward walls, units and military buildings.

## Fix

- `choosePolicies` (ai.cpp), while changes are free: a major civ makes a copy of the game as it stands (the `Game` constructor, as for the beliefs since `2026-10-08-religion-beliefs.md`) and tries on it each card that may change its cities: a slotted card by taking it out, any other in a slot it fits (one of its own kind if there is one, else a Wildcard slot) in place of that slot's card. A card's value is what it adds to its cities against an empty slot: their yields weighted by the AI's posture, 3 an Amenity and 2 a Housing (`citiesWorth`, the measure of the pantheon and belief choices). The best cards take the slots, a slot of their own kind first, then a Wildcard slot; behind them, cards that do nothing for the cities rank by `policyValue` as before; a card that costs the cities stays out. A slotted card that may not be slotted again (no longer available) leaves the copy changed, so the next card starts from a new copy; a civic's window retires such cards first, so in AI games this never happens.
- Not tried on the copy (`cardsForCities`): cards whose every modifier is of a kind a city report does not show (units, production toward items, great people, religion, loyalty, tourism, diplomacy), and Military cards without modifiers, whose code acts on units, strategic resources, plunder, upkeep and city defence. Dark Age cards and the other cards without modifiers are tried. Leaving out the Military ones changes no game and cut the tries on a Huge map by 28%.
- City-states rank every card by `policyValue`, as before.
- `Game::cityReports(player)`: a player's city reports in one run, sharing the work as a turn's processing does.

## Results

8 AI games (Small, 6 AI) at turn 200, the cards most held:

| Build | major civs | most held |
|---|---|---|
| main | 48 | Veterancy 48, Civil Prestige 47, Limes 42, Feudal Contract 41, Charismatic Leader 37, Maritime Industries 31 |
| this | 45 | Retainers 42, Natural Philosophy 41, Republican Legacy 36, Urban Planning 35, Charismatic Leader 29, Craftsmen 25 |

Three major civs of this build's games were conquered by turn 200, none of main's.

64 seeds (Small, 6 AI, turn 200):

| Build | cities | pop | techs | civics | era | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|---|
| main | 8.2 | 49.2 | 28.9 | 20.7 | 2.9 | 72.7 | 49.9 | 118.5 | 175 |
| this | 8.3 | 51.6 | 30.6 | 21.3 | 3.1 | 98.3 | 56.2 | 146.0 | 177 |

Speed (instructions, counted by Callgrind): a Small 6-AI game to turn 200 (seed 1) takes 4.7% more (13.90 against 13.27 billion; choosing cards 3.9%), and 20 turns of a Huge 12-AI game from turn 250 14% more (6.54 against 5.74 billion; choosing cards 11%, about 23 tries a window, each a report of some 25 cities). In time the Huge turns took 1.00 s against 0.89 s (medians of 5 runs).

Save and reload: 8 games in 5 setups, each saved and loaded once or twice, end on the straight game's state. The four replay games of the speed-up checks replay their own command logs; their end states move, as for any AI change.

## Tests

- `ai_slots_the_policy_cards_worth_most_to_its_cities`, three cities under Chiefdom unless said: Urban Planning takes the Economic slot, empty or God King's, for player 0 and for player 1 (whose copy must be on its own turn), and a Military card the Military slot. With two specialty districts, Insulae's Housing beats Ilkum (listed first, as many modifiers), and Liberalism's Amenity, with which the cities stay Content, takes Insulae's slot though it gains less over Insulae than Insulae is worth. Rationalism, in code, beats Ilkum and Liberalism in cities of 15 with a Library. A city-state still slots God King. In cities of 10 under Autocracy, Urban Planning takes God King's Economic slot and God King moves to the Wildcard one ahead of Insulae, and Music Censorship's -1 Amenity goes; Music Censorship cannot be slotted again there, so the copy starts over. Nothing changes outside a free window. On main all but the city-state and free-window checks fail.
- `luxuries_reach_the_cities_that_need_them_most` also checks that `cityReports` gives each city its own report.
- Mutation check: 28 mutants, each part of the choice broken alone; 21 are caught. Five of the other seven change only what is tried or how: every card tried, every card with modifiers (which does not build, its list of kinds left unused), the Military cards without modifiers too, the choice made outside a free window too (where the game refuses every change), and each city report made apart. One tries a card in a Wildcard slot before one of its own kind (each card weighs against an empty slot either way). One drops the old rule that keeps out a card worth nothing to the cities whose modifiers score 0 or less; only two Information-era Dark Age cards score that low.

## Left open

- Card effects outside a city report (Gold to the treasury such as Merchant Confederation's, great person points, Favor, Influence, Tourism, units) count only by their modifiers, behind any card that does something for the cities.
- Choosing costs the most on large late-game maps; reports of only the cities a card reaches, or a cheaper copy of the game, would cut it.
