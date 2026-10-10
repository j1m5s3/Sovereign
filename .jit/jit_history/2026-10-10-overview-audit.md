# Record: the overview spec audited against the core, part 1

Status: part 1 done, 2026-10-10. Previous: `2026-10-10-religion-dedication-leftovers.md`. A read of specs/civ6/00-overview.md (turn segments, end-turn blockers, game speed, difficulty, AI starting units, score, turn order) against the core. Chosen by Claude under James's standing consent.

## Fixes

- **An owed dedication blocks the end of the turn** (00: end-turn blockers): `CommandError::DedicationNeeded` while `dedicationsPending > 0` and one is open, since an unchosen dedication is lost when the next era begins. The AI already chooses; the random bot (core/tools/random_bot.h) now picks one too. The Unreal front end must open its dedication chooser on this error (its end-turn button and the bridge playthrough test have no case for it yet).
- **City-states start stronger at Immortal and Deity** (00: Difficulty levels): +2 and +3 Warriors and Ancient Walls, `DifficultyType::cityStateExtraWarriors` and `cityStateWalls` in setup.json. Civ also gives Emperor +1; the Sovereign rule keeps every AI bonus to Immortal and Deity.
- **In a later era's start an AI's extra Warriors are that era's defender** (game-setup.md, AI-only starting units): Spearman (Classical, Medieval), Pikeman, Pike and Shot, Ranger, AT Crew (Atomic, Information); `EraStartType::aiDefender`.

## Matches

Difficulty values at the levels the core applies (Immortal, Deity; human Settler and Chieftain; camp gold; diplomacy offset; camp distance), AI starting units' floor formula, score line items for civics, techs, wonders, cities, districts, population, great people and era score, game speed cost percents and turns, the negative-gold disband rule.

## Left for later parts

- Keep-or-raze and World Congress votes as end-turn blockers (each needs a "keep" command or an Unreal panel first); envoys, governors, empty policy slots and pantheons only accumulate, so they stay non-blocking.
- Human martial-law loyalty (+7 at Settler and Chieftain, -3 at Emperor and up): Civ's garrison loyalty this adjusts has no clear source; not built.
- The barbarian raid throttle per difficulty (`BARBARIAN_LOWER_THROTTLE_PER_DIFFICULTY`): raids are not throttled in the core.
- Score: era buildings and converted citizens lines; religion as 5 per belief (00) or 5 for a founded religion (09, what the core does): the specs disagree.
- The Half and Slight game-speed duration scalings: timed effects scale by the full cost percent.
- Negative gold: amenities lost per 10 Gold below 0 round up (-1 Gold costs one); kept.

## Results

- 128 AI games (Small, 6 AI, turn 200, Prince) against the leftovers: every figure within two standard errors (city-states' Warriors now spawn within 2 plots, so games differ).
- 8 long AI games in 8 setups end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.
- Mutation: every mutant (6) caught.
- The 30-turn golden game's state hash file regenerated (rules checksum).

## Tests

`an_owed_dedication_blocks_the_end_of_the_turn` and dedications cleared in `the_world_era_sets_each_civs_age` (test_eras.cpp), `city_states_and_era_starts_follow_the_difficulty` (test_difficulty.cpp).
