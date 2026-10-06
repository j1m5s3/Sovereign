# Plan: the last great person and policy effects (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-belief-effects.md`. Chosen by Claude under James's standing consent. After the great person plan (`jit_history/2026-10-06-great-person-effects.md`), 28 great people still had text-only lines, and 7 policy cards had lines with no code. James can redirect at any point.

Specs: 07-economy-trade-great-people (Great People), 04-tech-civics-government (Policies); data: `greatpeople.json` and `policies.json` `untrackedEffects`.

## Milestones (one PR each)

1. **Done: Great people in code (15).** Their ids are looked up once (`Game::Gp`, `usedHere`, `usedBy`); they can be activated now (`codedGreatPerson`).
   - Trade routes:
     - Zheng He, Zhang Qian, Marco Polo: +2 Gold per foreign route to the city, and +2 Gold on those routes;
     - Ibn Fadlan: +2 Faith on routes to city-states;
     - Raja Todar Mal: +0.5 Gold per specialty district on domestic routes;
     - John Rockefeller: +2 Gold per strategic resource at the destination.
   - Mimar Sinan: an Industrial Zone claims the unowned plots around it.
   - Marcus Licinius Crassus: claims an unowned plot beside the player's land.
   - Hildegard of Bingen: Science equal to the Holy Site's Faith adjacency.
   - John Roebling and Jane Drew: Amenities and Housing.
   - Abu al-Qasim al-Zahrawi: +5 healing for land units.
   - Ibn Khaldun: +4% (Ecstatic) or +2% (Happy) to every yield but Food.
   - Kenzo Tange: tourism from the city's district adjacency.
   - Marina Raskova: +1 air slot at her Aerodrome.
2. **Done: The policy cards' last lines:**
   - Wisselbanken and Democratic Legacy: +1 alliance point a turn each;
   - Gunboat Diplomacy: city-states the player has an envoy with open their borders (`grantsOpenBorders`);
   - Second Strike Capability: nuclear devices cost 50% more to keep, as the data says;
   - Non-State Actors needs nothing: spies already choose any promotion they lack (`PromoteSpy`).
   - Hallyu: Rock Bands' promotions are still drawn at random, since there is no choice to open.

## Decisions (Claude's recommendations; James gave standing consent)

- Not planned:
  - Tesla's and Paxton's regional ranges;
  - Shah Jahan's purchase;
  - Raffles's city transfer;
  - Magellan's and Colaeus's tile resource;
  - Limes (the Tsikhe belongs to no civ in the game).
- Effects written "(one-time)" in the data that read as lasting (route yields, Amenities, Housing, healing) are kept while the great person's city or player stands.

## Follow-up: the luxury corporations

The product luxuries exist in the data (Toys, Cosmetics, Jeans, Perfume; no map frequency). John Spilsbury (1 Toys) and Helena Rubinstein, Levi Strauss and Estée Lauder (2 Cosmetics, Jeans or Perfume) add copies of their product to the player whose city they were used in (`luxuryCopies`). The copies give Amenities like any luxury and can be traded.

Also since: James Young lets his player see Oil before its tech (`resourceVisible`), and Mary Katherine Goddard adds a level of diplomatic access with every civ (`accessLevel`).

Also since: Giovanni de' Medici gives each of his player's Banks 2 Palace-type slots (`extraPalaceSlots`), counted in `greatWorkSlots`, `freeGreatWorkSlot` and Great Work moves.
