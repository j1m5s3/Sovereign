# Plan: world wonder effects (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-suzerain-bonuses.md`. Chosen by Claude under James's standing consent: of the 52 world wonders, only a handful applied their effects (free units, Eurekas, Hanging Gardens, Etemenanki, trade capacity); the rest were text (`text` in `wonders.json`). James can redirect at any point.

Specs: 03-districts-buildings-wonders (Wonders); data/wonders ("Effects (modifiers)").

## Milestones (one PR each)

1. **Done: Generated wonder effects.** The generator reads each wonder's effects with the policy card parser (source: the wonder's building id, so they apply while it stands) plus a few wonder forms:
   - an unqualified "+N% yield" (its own city), Appeal, unit production in all cities, plot yields by feature or terrain;
   - one-time effects on completion, which reuse the great person effect kinds: random techs and (new) random civics, Inspirations, diplomatic victory points, governor titles, population in every city, a promotion for every military unit, a share of the treasury, several free units;
   - lasting: a strategic resource a turn (Jebel Barkal, read with the great people's `RESOURCE_PER_TURN`);
   - fields: `spreadCharges` (Hagia Sophia: + charges on bought religious units) and `policySlots` (Alhambra, Forbidden City, Potala Palace, Big Ben; read in milestone 2).
   What is left is listed per wonder in `untrackedEffects`.
2. **Done: Policy slots and the first effects in code.** Wonders' policy slots (Alhambra, Forbidden City, Potala Palace, Big Ben) follow the government's (`Game::policySlotType`, `syncPolicySlots`). Also:
   - Colosseum: +2 loyalty within 6 tiles; Statue of Liberty: no loyalty loss within 6 tiles (`Game::nearOwnWonder`);
   - Machu Picchu: Mountain adjacency; Venetian Arsenal: a second naval unit; Taj Mahal: era score; Mont St. Michel: Martyr Apostles.
   The generator now also reads Pyramids' Builder charge, Ruhr Valley's Mines and Quarries, and Casa de Contratación's "and" conditions.
3. **Done: Trade, city-state and great person wonders:**
   - Kilwa Kisiwani: `Game::kilwaPercent`, yields and production by suzerainties of each kind;
   - University of Sankore: +2 Science per foreign route to it, and +1 Science and +1 Gold for the route's origin;
   - Great Zimbabwe: +2 Gold per bonus resource of the origin; Torre de Belém: +2 Gold per luxury at an international destination;
   - Oracle: +2 points per district, Faith patronage 25% cheaper; Meenakshi Temple: Gurus 30% cheaper;
   - Apadana: +2 envoys per wonder completed in its city; Országház: double favor from suzerainties.
4. **Done: Terrain and tourism wonders:**
   - generated: Petra (desert and its hills, not floodplains), Mausoleum (Coast), Amundsen-Scott (`CITY_MIN_TERRAIN_TILES`);
   - in code: Mausoleum's extra Great Engineer charge, the Great Bath (+1 Faith per flooded plot), Torre de Belém's free buildings (`grantTorreBuildings`);
   - tourism and Appeal: the Golden Gate Bridge doubles tourism from improvements and parks, Cristo Redentor doubles Seaside Resorts, and Biosphère gives Rainforest and Marsh +1 Appeal;
   - a fix: Appeal modifiers (Eiffel Tower, Golden Gate Bridge) were read only in cities with great people (`plotAppeal`'s guard), and now apply.

Not planned:
- Huey Teocalli's lake effects: the map has no lakes apart from Coast.
- St. Basil's "Tourism from scaled 200%" (the source is unnamed).
- Cristo Redentor's religious tourism: there is no decay to prevent.
- Golden Gate Bridge's roads and cliffs.
- Biosphère's power and green-energy tourism.
- The Great Library's boosts from Great Scientists.
- Stonehenge's Apostle.

## Decisions (Claude's recommendations; James gave standing consent)

- One effect parser for great people and wonders (`parseEffect` in `rules.cpp`). Wonder effect kinds that are lasting count while the owner holds the wonder (`greatPersonEffectTotal` reads the owner's cities' wonders for `RESOURCE_PER_TURN`).
- Big Ben's "multiplies treasury by 50" is +50% of the treasury (the Civ VI value), kind `TREASURY_PERCENT`.
- `Game::wonderCompleted` gives scenarios and tests a wonder's completion effects.
- Wonder policy slots come after the government's in `Player::policies`; a save may hold more slots than the government has. When a wonder is lost, the slots are resized at its old owner's next turn and cards in slots of the wrong type come out.
- Wonders read in hot paths are looked up once per `Game` (`Game::W`, `wonderType`, `holdsWonder`); string lookups of long ids allocate.
- The generator drops accented letters from ids (`BUILDING_ORSZ_GH_Z`, `BUILDING_TORRE_DE_BEL_M`); code uses the ids as generated.
