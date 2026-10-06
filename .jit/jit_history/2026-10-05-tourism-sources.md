# Plan: tourism from the land (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-dedications.md`. Chosen by Claude under James's standing consent: the tourism sources of 07 the core left out — resorts and improvement tourism, National Parks, Rock Bands. One milestone.

Specs: 07-great-people-great-works-tourism (Tourism sources, National Parks, Seaside Resorts and Ski Resorts, Rock Bands); data/improvements (Tourism, Min appeal); data/units (Rock Band results); data/global-parameters (NATIONAL_PARK_*, ROCK_BAND_MAX_LEVEL).

## Milestone

1. **Done** (generated improvement `tourism`, `minAppeal` and `coastal`, and the `rockBandResults` table; `ImprovementType::tourismSource`, `tourismPercent`, `tourismAfter`, `minAppeal`, `coastal`; `Rules::rockBandResults`; `Plot::park`, save version 59; commands `DesignatePark` 63 and `PerformConcert` 64; new `src/tourism.cpp`):
   - **Improvement tourism.** Improvements with a tourism source give it once their tech is known (none needed for resorts).
     - Seaside and Ski Resorts give their plot's appeal.
     - After Flight, Pastures, Plantations and the others in the data give their Culture.
     - A Seaside Resort needs a Breathtaking plot (appeal 4) on the coast.
   - **National Parks.**
     - A Naturalist (Conservation, bought with Faith: 300, +50 per copy) designates a diamond: its plot, a neighbour, and the two plots beside both.
     - All four must be owned by one city, Charming or better (appeal 2+), with no city, district, wonder or improvement.
     - The park gives tourism equal to its plots' appeal. Its city gains NATIONAL_PARK_AMENITIES_OWNING_CITY (2) amenities, and each of that civ's NATIONAL_PARK_NUM_OTHER_AMENITY_CITIES (4) nearest other cities gains 1.
     - Park land takes no improvements.
   - **Rock Bands [GS].**
     - A Rock Band (Cold War, bought with Faith) plays in a foreign major's city center, district or wonder plot, if not at war with it.
     - The outcome is drawn by the data's base probabilities; each band level moves 2 points from the two worst outcomes to the two best.
     - The band's owner gains album sales plus the tourism bomb as tourism toward that civ.
     - The band may gain a level (up to ROCK_BAND_MAX_LEVEL; kept in its XP) or break up.
   - **Faith purchase.** Naturalists and Rock Bands are bought with Faith whatever the city follows, and carry no religion.
   - **AI.**
     - With a park site in its land and Faith to spare (or a culture strategy), it buys one Naturalist and walks it to the nearest site.
     - With a culture strategy it keeps up to two Rock Bands touring the nearest foreign cities.
     - Builders build a resort where one is allowed.
   - **Unreal.** The build chooser (B) offers "Designate a National Park" and "Perform a concert". The production chooser already lists Faith purchases.

## Decisions (Claude's recommendations; James gave standing consent)

- The park's diamond, the "nearby" cities for amenities, and the reading of album sales plus the tourism bomb as one tourism gain are Sovereign readings of rules the data leaves open.
- Not modelled: Rock Band promotions, Landmarks. The Ski Resort's amenity followed (generated improvement `amenities`).
