# Record: Religious alliance level 3 pressure (08)

Status: done, 2026-10-09. Previous: `2026-10-06-alliance-effects.md`, which modelled Religious level 3's +1 Faith per follower of the ally's religion and left its +20 pressure unmodelled. Spec 08 Alliance levels; data: `alliance pressure from no ally religion (Amount=20)`.

## Change

- **A Religious alliance at level 3 adds +20 pressure of the ally's founded religion** in each of the civ's cities that have none of that religion's followers (Civilopedia: "bonus Religious Pressure in cities with no followers of your ally's Religion"). Level 1 still stops the allies' cities pressing each other; this bonus is separate and runs in `Game::processReligion` after that adjacent pass (`Game::alliance`, `allianceLevel`). A city that already has followers of the ally's religion is skipped, as is a level-1 or level-2 alliance, and an ally that has founded no religion (the same reading as the Faith bonus in `city.cpp`).

## Tests

- `a_religious_alliance_presses_cities_with_no_ally_followers`: both allies' cities gain 20 of the other's religion in one world turn; a city that already follows the ally's religion does not; a level-2 alliance adds none; a player's own Holy City still presses its other city.
- `ctest` Release, all 7 tests passed (including `sovereign_tests`).
