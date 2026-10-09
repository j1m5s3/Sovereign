# Record: leader character rules the audit found off (part 7)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-audit-6.md` (part 1, `2026-10-09-leader-audit.md`, lists the audit's findings). This part builds diplomacy in person (leader doc section 8.4). Chosen by Claude under James's standing consent; the numbers are Sovereign tuning in `data/rules/leader.json`.

## Change

- **A city-state receives the ruler in person.** The first time a civ's ruler stands on or next to a city-state's city (not at war with it), the civ gains `IN_PERSON_ENVOYS` (1) envoys there (`Game::inPersonVisit`, called from `Game::enterPlot`). Once per city-state city a game: the city's plot is kept with the ruler's visits (`Player::leaderVisits`), so there is no new saved state. A foreign city earns no visit XP.
- **An AI hears a ruler who came in person.** While a civ's ruler stands on or next to an AI civ's capital, that AI accepts a deal from the civ worth `IN_PERSON_DEAL_VALUE` (50) Gold less than it otherwise asks (`Game::wouldAccept`, `Game::rulerVisiting`). The capital only, not any city. A human judge gets nothing (doc: "vs. human players: no mechanical bonus").
- The risk the doc names needs no new code: a ruler away from home is exposed to assassins (`leaderExposed`) and to capture if war breaks out. Delegations and embassies stay as they were, so a civ never has to send its ruler.

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against part 6): every game identical. Inferred, not traced: AI rulers do not travel to city-states or foreign capitals.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; 4 of 8 end states differ from part 6.
- Tests: `a_ruler_in_person_wins_envoys_from_a_city_state` (two plots off gives nothing, beside the city gives 1 envoy and no XP, a second visit nothing, nothing while at war), `an_ai_hears_a_ruler_who_came_in_person` (a deal 30 Gold short is refused far off, accepted beside the capital, refused beside another city of the AI, refused by a human judge). Mutation: 9 mutants, all caught (the capital check after the AI's other city was listed first in the test).

## Not done yet

- The AI does not send its own ruler on visits.
- Relationship gains from a visit (an opinion memory) are left out: the doc names deal acceptance and envoys as the examples.
- Bodyguards (named companions, doc section 8.3) remain the audit's last larger item.
