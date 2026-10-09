# Record: era, space race and tourism rules the spec audit found off (09, part 2)

Status: done, 2026-10-09. Previous: `2026-10-09-climate-audit.md`, which fixed the audit's 5 disaster rules. This part fixes its other 3. Chosen by Claude under James's standing consent; one PR.

## Change

- **The world gets 10 turns' warning of the next era** (09: Global era transitions, "players get a 10-turn warning"; data: `NEXT_ERA_TURN_COUNTDOWN` 10). Before, the world moved on the turn its trigger was met. Now a countdown starts first and the era changes when it ends. It starts in time for the era to end at its minimum length, once half the civs are ahead, or at its maximum when they are not. So an era still lasts 40 to 60 turns on Standard, and one that waited on the civs runs up to 10 turns longer than before (Sovereign reading: the spec gives the bounds and the warning, not how they combine). `GameState::eraEndsOn` holds the turn it ends (save version 90), and `Game::eraCountdown()` gives the turns left, or -1.
- **A Terrestrial Laser Station counts only while its city is powered** (09: Science Victory, "only counts if the city is powered"). Before, every station built added its light-year a turn. Now `Game::expeditionSpeed` counts the stations in the civ's powered cities (supply at least the demand, which each station raises by 5), up to the number the civ built. Lagrange stations are unchanged.
- **Wish You Were Here doubles National Park tourism** (09: dedications, "+100% National Park tourism" in a Golden Age). Before, only its +50% in cities with a governor applied. Now a Golden or Heroic Age civ with the dedication gets twice its parks' tourism (`Game::tourismBase`).
- Follow-up for the Unreal front end: show the countdown (`eraCountdown()`) and say when it starts.

Results:
- 128 AI games (Small, 6 AI, turn 200) against part 1: the later eras cost the AI a little research, as techs and civics of eras behind the world's are cheaper (`TECH_COST_PERCENT_CHANGE_BEFORE_GAME_ERA` -20%): techs -0.33 ± 0.04 of 34.3, civics -0.32 ± 0.04 of 23.6, science -1.9 ± 0.7 of 138; population +0.4 ± 0.2, production and Gold unchanged. Eight 150-turn 6-AI games take the same time (11.2 to 11.9 s against 11.2 to 11.9 s, three runs each).
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; all 8 end in a different state (the four with human seats only by the new field).
- Tests: `the_world_is_warned_ten_turns_before_the_next_era` (the countdown opens 10 turns before the era's minimum with one civ of two ahead, before its maximum with none, and survives a save), `a_terrestrial_laser_station_needs_its_city_powered` (10 Power for two stations, 8, and a third station the civ did not build), `wish_you_were_here_doubles_national_park_tourism`. `the_world_era_sets_each_civs_age` and `dark_age_cards_come_with_a_dark_age_and_go_with_it` start with the countdown run out, and `the_space_race_and_the_science_victory` powers its stations with Solar Farms. Mutation check: 11 mutants, all caught.

## Unsure, not changed

- Whether the countdown scales with game speed (the core: a flat 10 turns, as the data gives it).
- Where a city's power reaches from (the core: its center).
- Reyna's Renewable Subsidizer: 09 says "+2 Power" per renewable, 08 and the data say +2 Gold; the core gives Gold.
