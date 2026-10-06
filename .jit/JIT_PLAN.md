# Plan: world wonder effects (step 6, "everything else")

Status: active, 2026-10-06. Previous: `jit_history/2026-10-06-suzerain-bonuses.md`. Chosen by Claude under James's standing consent: of the 52 world wonders, only a handful applied their effects (free units, Eurekas, Hanging Gardens, Etemenanki, trade capacity); the rest were text (`text` in `wonders.json`). James can redirect at any point.

Specs: 03-districts-buildings-wonders (Wonders); data/wonders ("Effects (modifiers)").

## Milestones (one PR each)

1. **Done: Generated wonder effects.** The generator reads each wonder's effects with the policy card parser (source: the wonder's building id, so they apply while it stands) plus a few wonder forms:
   - an unqualified "+N% yield" (its own city), Appeal, unit production in all cities, plot yields by feature or terrain;
   - one-time effects on completion, which reuse the great person effect kinds: random techs and (new) random civics, Inspirations, diplomatic victory points, governor titles, population in every city, a promotion for every military unit, a share of the treasury, several free units;
   - lasting: a strategic resource a turn (Jebel Barkal, read with the great people's `RESOURCE_PER_TURN`);
   - fields: `spreadCharges` (Hagia Sophia: + charges on bought religious units) and `policySlots` (Alhambra, Forbidden City, Potala Palace, Big Ben; read in milestone 2).
   What is left is listed per wonder in `untrackedEffects`.
2. **Next: Effects in code.** Wonders' policy slots; Colosseum and Statue of Liberty loyalty; Machu Picchu's Mountain adjacency; Venetian Arsenal's extra ships; Mont St. Michel's Martyr; Great Bath floods; Pyramids' Builder charge; Oracle's district points; Petra, Ruhr Valley, Huey Teocalli and Mausoleum plot yields; Kilwa Kisiwani; University of Sankore; Great Zimbabwe; Taj Mahal; Casa de Contratación; Amundsen-Scott; Biosphère; Golden Gate Bridge; Cristo Redentor; St. Basil's; Torre de Belém; Országház; Apadana; Meenakshi Temple.

## Decisions (Claude's recommendations; James gave standing consent)

- One effect parser for great people and wonders (`parseEffect` in `rules.cpp`). Wonder effect kinds that are lasting count while the owner holds the wonder (`greatPersonEffectTotal` reads the owner's cities' wonders for `RESOURCE_PER_TURN`).
- Big Ben's "multiplies treasury by 50" is +50% of the treasury (the Civ VI value), kind `TREASURY_PERCENT`.
- `Game::wonderCompleted` gives scenarios and tests a wonder's completion effects.
