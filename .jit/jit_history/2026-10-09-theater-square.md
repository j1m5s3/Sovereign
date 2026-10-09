# Record: the Theater Square's misnamed effects

Status: done, 2026-10-09. Previous: `2026-10-09-city-focus.md`, which found the problem. Three places named the Theater Square `DISTRICT_THEATER`, an id the rules don't have (`districts.json` calls it `DISTRICT_THEATER_SQUARE`), so their effects never fired. Chosen by Claude under James's standing consent.

## Rules

- **Grand Opera** (04: Policies; `kCards` in `Game::cityReport`, `city.cpp`): +50% of a Theater Square's building Culture now applies, as Free Market's, Rationalism's and Simultaneum's already did for their districts.
- **Pen, Brush, and Voice** (09: Dedications; `Game::completeItem`): +1 era score in a Normal Age for each Theater Square building now applies.
- **Glam Rock** (07: Rock Bands): its +2 levels at a Theater Square now apply. The fix is in the generator (`BAND_PLACES` in `tools/rules_gen/gen_rules.py`); `promotions.json` is regenerated, not edited.
- **Guard:** the rules now refuse a promotion or ability whose concert bonus names a place `Game::performConcert` can never match (anything but a district, an improvement, `WONDER`, `NATIONAL_PARK` or `NATURAL_WONDER`).
- A scan of every quoted id in `core/src` and `core/include` against the rules files found no other unknown id (the rest are prefixes or global names).

## Results

- The 30-turn golden game's state hash is unchanged; only its rules checksum moves, with `promotions.json`.

## Tests

- `rules_reject_an_unknown_concert_place` (the four kinds of place load; a misnamed one is refused on a level bonus or a burst, on a promotion or an ability; Glam Rock names the Theater Square), Grand Opera in `policy_cards_in_code_military_and_economy`, and `building_dedications_score_their_districts_buildings` (all four building dedications: Free Inquiry, Pen, Brush, and Voice, Heartbeat of Steam, Sky and Stars).
- Mutation check: 11 mutants (each fixed id put back, each part of the place check dropped); the tests catch all 11.
