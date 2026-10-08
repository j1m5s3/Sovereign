# Record: the spec audit, part 2 (step 6, "everything else")

Status: done, 2026-10-06 (PRs #126–#130 and the closing PR). Previous: `2026-10-06-spec-audit-gaps.md` (part 1: specs 01, 02, 03, 05). Two read-only audits compared specs/civ6 04, 06, 07, 08 and 09 against the core and confirmed 28 gaps by grep. Chosen by Claude under James's standing consent.

## What was built

1. **Small rules** (#126):
   - Tech and civic costs −20% behind the world era and +20% ahead [GS].
   - Religious tourism is halved after The Enlightenment, and reduced toward civs of another main religion.
   - Trade routes to city-states give a bonus by type.
   - Condemn Heretic removes the victim religion's pressure nearby.
   - War on a city-state brings grievances from its suzerain and from civs with envoys there.
   - The suzerain gets the city-state's luxuries and strategics.
   - Monarchy: +50% envoys, and +2 Favor per Renaissance Walls.
   - The score counts wonders, great people and a founded religion.
   - Moved or traded art is locked for 10 turns.
   - Legacy policy cards can be slotted after their government.
2. **Gold, faith and trade** (#127):
   - `BuyPolicyChanges`: Gold opens government and policy changes between civics.
   - Launch Inquisition, Inquisitors, and Gurus with heal charges (`HealReligious`).
   - Trading Posts: range refuels in own cities and at posts, and +1 Gold per foreign post passed. Routes may now go to city-states.
   - Future Tech and Future Civic repeat.
3. **Deals** (#128):
   - Ceding cities in peace deals; Diplomatic Favor as a deal item.
   - Make Demand, as a one-sided deal.
   - Suzerain War: city-states join their suzerain's wars and its peace.
   - The dialogue layer reads `favor` and `city` items.
4. **The larger items** (#129, #130, closing PR):
   - **Governor promotions:** Air Defense Initiative, Foreign Investor, Grand Inquisitor, Laying On Of Hands, Patron Saint, Black Marketeer, Vertical Integration. Liang's Aquaculture and Parks and Recreation open the Fishery and the City Park; the generator now emits both.
   - **Disasters:**
     - districts and their buildings are pillaged, and a meltdown destroys the buildings;
     - storms move on for 3 turns, and forest fires spread;
     - meteor showers strike one plot;
     - deforestation scales CO2's effect from −20% to +50%;
     - the Flood Barrier costs its base × the city's coastal lowland plots × (1 + sea level rises already seen).
   - **World Congress:** ten more resolutions (Luxury Policy, World Religion, Heritage Organization, World Ideology, Border Control, Public Works, Global Energy, Sovereignty, Deforestation Treaty, Espionage Pact), with their own target kinds.
   - **Historic moments:** about 75 more, including bold city placement (beside a volcano or floodplains, near a rival, on a new continent).
   - Save version 73.

## Not done

- **Occupied cities' Amenity penalty:** the spec gives no amount.
- **Era score in the score:** the core keeps no lifetime era score.
- **Mercenary Companies and Arms Control resolutions:** their extracted effects are unclear.
- **Moments that need exploration tracking or more state:**
  - circumnavigation, discovering a new continent, railroad connections, strategic potential;
  - City of Awe, unique districts, levies near enemies, pacified city-states;
  - the score-0 ones.
- **Meteor sites** (their goody) and the Fishery's sea-resource adjacency.
- **From part 2's own list:** AI random agendas (each leader has one hand-written agenda), liberation envoys by era (no data), trade route length in round trips (the flat length stands).

## Decisions (Claude's, under James's standing consent)

- **Policy changes for Gold:** the cost is POLICY_COST_BASE + (POLICY_COST_INCREASE_TO_BE_EXPONENTED × civics done)^1.5, rounded down to POLICY_COST_VISIBLE_DIVISOR.
- **Launch Inquisition:** only the founder of the Apostle's religion can launch it, once. Inquisitors are bought in cities that follow the player's religion.
- **Make Demand:** an AI yields to a civ at least twice as strong when the demand costs it at most 200 Gold of worth, plus 100 for each further multiple of strength. It resents the demand (−10 opinion).
- **Ceded cities:** they arrive at 50 loyalty. An AI losing badly values peace at 400.
- **Grand Inquisitor:** the +10 applies in Moksha's city's territory, because the core does not record where a unit was bought.
- **Black Marketeer:** 80% fewer strategic resources, rounded up.
- **Deforestation:** the level is set by the world's share of woods lost since climate tracking began, not by a running average.
- **Luxury Policy A:** doubles the cities a luxury reaches (a reading of "no cap").
- **Splendid districts:** "Splendid ..." and "fully developed" moments go to the first district of a type with every building.
- **Unit promoted with distinction:** a unit's third promotion, at most once per era.
- **Threatening camp:** one within 6 plots of a city.
- **Aggressive placement:** a city within 6 plots of a rival's city.
- **Ages:** in three 250-turn AI games after the new moments, ages split Normal 69%, Golden 14%, Dark 13% and Heroic 4%, so ERA_SCORE_THRESHOLD_ADJUST stays at −10.

## Follow-up (2026-10-08): the AI makes demands and offers cities

- **City for peace:** an AI at war with a major civ whose army is more than twice its own, after offering that civ a white peace, offers peace with one of its cities: the one it values least, never one worth more to it than the peace. To an AI it offers only a deal that AI accepts; a human answers it like any deal. It asks again at most every 10 turns, like its other deals.
- **Tribute:** an AI asks a neighbour it dislikes (a city it has seen within 14 plots of its own; not a friend or ally) and outmatches at least twice over for Gold, on one turn in 30 for each pair. It asks for as much as Make Demand says an AI that weak would hand over, up to the whole purse in tens, and only from a purse of 50 or more.
- **AI pace:** over 24 bench games (6 civs, Small, 200 turns) science at T200 went 66.8 → 65.6 and cities 7.8 → 7.9; the era stayed at 2.8.
