# Plan: belief effects (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-wonder-effects.md`. Chosen by Claude under James's standing consent: of the 59 beliefs, 26 had no effect (no modifier in `modifiers.json` and no code; the 9 worship beliefs only unlock their buildings, which works). James can redirect at any point.

Specs: 06-religion (Pantheons, Beliefs); data: `religion.json` `text`.

## Milestones (one PR each)

1. **Done: Pantheons (all 15):**
   - Hand-written modifiers:
     - City Patron Goddess, God of the Forge, Monument to the Gods, Religious Settlements' borders;
     - Divine Spark, God of Craftsmen, Religious Idols (new `PLOT_HAS_RESOURCE_CLASS`);
     - River Goddess (new `CITY_DISTRICT_NEXT_TO_RIVER`).
   - In code:
     - Dance of the Aurora, Desert Folklore and Sacred Path: Holy Site adjacency;
     - Earth Goddess: Faith on plots of Appeal 4+;
     - God of Healing: +30 healing in and next to Holy Sites;
     - God of War: Faith from kills next to Holy Sites;
     - Initiation Rites: Faith and a full heal for clearing a camp.
2. **Done: Follower beliefs:**
   - Divine Inspiration: +4 Faith per wonder;
   - Jesuit Education: Campus and Theater Square buildings for Faith at their Gold price;
   - Religious Community: +2 Gold on international routes for each of the Holy Site, Shrine, Temple and worship building;
   - Reliquaries: triple Faith and tourism from relics;
   - Warrior Monks: bought with Faith where the city follows it, and a new Holy Site claims the unowned plots around it;
   - Work Ethic: the Holy Site's Faith adjacency as Production;
   - Zen Meditation: +1 Amenity with two specialty districts (a modifier).
3. **Done: Founder and enhancer beliefs:**
   - Sacred Places: +2 Faith, Culture, Science and Gold per following city with a wonder;
   - Papal Primacy: +200 pressure in the city-state with each envoy;
   - Religious Unity: an envoy when pressure turns a city-state to the religion;
   - Holy Waters: +10 healing at Holy Sites, from milestone 1.

## Decisions (Claude's recommendations; James gave standing consent)

- Player-wide modifier collections (`PLAYER_CITIES`, `PLAYER_CAPITAL`, `PLAYER_CITY_PLOTS`) from a belief reach every city of a player whose pantheon or founded religion has it. City collections keep following each city's majority religion, or its owner's pantheon while it has none.
- Beliefs read in hot paths are looked up once per `Game` (`Game::Bf`, `cityFollows`, `playerHasBelief`, `beliefInPlay`).
- God of War: Faith for a kill next to a Holy Site, as the data says (Civ VI: within 8 tiles). God of Craftsmen counts any improvement on the strategic resource.
- Religious Unity counts conversions by passive pressure only (`processReligion`); Missionaries' conversions do not award the envoy.
- Warrior Monks could not be bought at all before; they now need the belief in the city's religion.

## Follow-up: the AI weighs pantheons

The AI took the first pantheon with data modifiers, so code-only beliefs were never chosen (`beliefModelled` now counts them). It now tries each available pantheon on a copy of the game and keeps the one that raises its cities' yields, Amenities and Housing most (`bestPantheon` in `ai.cpp`; it weighs only once a pantheon is affordable). In two test games (seeds 7 and 3, 6 civs) the picks spread across Desert Folklore, Dance of the Aurora, Divine Spark, City Patron Goddess, Earth Goddess, Lady of the Reeds and Marshes and God of the Open Sky. Pace benchmark at turn 200: cities 7.6 (was 7.9), science 59.2 (51.9), production 91.3 (88.4).

The AI picks its founder and follower beliefs the same way (`bestReligionBeliefs`): the best follower with the first founder belief, then the best founder with that follower, each founded on a copy of the game and scored by its cities and its founder yields. Seed 7 picked Cross-Cultural Dialogue with Choral Music, Lay Ministry with Divine Inspiration, Papal Primacy with Feed the World, and Pilgrimage with Jesuit Education. The pace benchmark did not move.
