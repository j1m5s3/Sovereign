# Record: occupied cities and war weariness amenity floors (rules gaps)

Status: done, 2026-10-09. Previous: `2026-10-09-districts-audit.md`. Spec 02 Amenities and spec 08 ("Occupied cities: −amenities until peace grants them") were missing in the core; `2026-10-06-spec-audit-part-2.md` left the amount open. Growth already treated a city taken from a civ still at war with the original owner as occupied (`CITY_GROWTH_OCCUPATION_MULTIPLIER`).

## Rules

- **Occupied** (`Game::occupied`): the same test growth already used, a city whose original owner is another living war opponent. Growth now calls that helper.
- **Amenities** (`Game::cityReport`): war weariness still takes 1 Amenity per 400 points, but only down to a floor of (required − `WAR_WEARINESS_LOSS_OVER_REQ_AMENITIES_*`) by city kind: 0 below for a city the owner founded (`FOUNDED_CITY`), 1 below for a captured city at peace (`NONFOUNDED_CITY`), 3 below for an occupied city (`AT_WAR_CITY`). Weariness never raises a city already under that floor. A captured city at peace with no weariness loses nothing, so conquering never costs amenities forever. Spec 08's occupied-city penalty is this deeper floor until peace. This also caps weariness in a civ's own cities, which used to have no floor. No save-version bump (derived).

## Tests

- `war_weariness_amenities_stop_at_the_city_kind_floor`: a founded city at war with heavy weariness stops at its requirement; a captured city at peace stops 1 below; an occupied city stops 3 below; with no weariness none of them lose any. `occupied` is true only of the captured city at war.
- `occupied_cities_do_not_grow` still covers growth.
