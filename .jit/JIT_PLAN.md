# Plan: belief effects (step 6, "everything else")

Status: active, 2026-10-06. Previous: `jit_history/2026-10-06-wonder-effects.md`. Chosen by Claude under James's standing consent: of the 59 beliefs, 26 had no effect (no modifier in `modifiers.json` and no code; the 9 worship beliefs only unlock their buildings, which works). James can redirect at any point.

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
2. **Next: Follower beliefs:** Divine Inspiration, Jesuit Education, Religious Community, Reliquaries, Warrior Monks, Work Ethic, Zen Meditation.
3. **Then: Founder and enhancer beliefs:** Papal Primacy, Religious Unity, Sacred Places, Holy Waters (its healing is already in, with God of Healing).

## Decisions (Claude's recommendations; James gave standing consent)

- Player-wide modifier collections (`PLAYER_CITIES`, `PLAYER_CAPITAL`, `PLAYER_CITY_PLOTS`) from a belief reach every city of a player whose pantheon or founded religion has it. City collections keep following each city's majority religion, or its owner's pantheon while it has none.
- Beliefs read in hot paths are looked up once per `Game` (`Game::Bf`, `cityFollows`, `playerHasBelief`, `beliefInPlay`).
- God of War: Faith for a kill next to a Holy Site, as the data says (Civ VI: within 8 tiles). God of Craftsmen counts any improvement on the strategic resource.
