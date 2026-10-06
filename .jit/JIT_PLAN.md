# Plan: policy card effects (step 6, "everything else")

Status: active, 2026-10-06. Previous: `jit_history/2026-10-06-dark-age-policies.md`. Chosen by Claude under James's standing consent: of the 140 policy cards, about 110 can be slotted but do nothing. Only the Ancient and Classical cards and the Dark Age cards have hand-written modifiers. James can redirect at any point.

Specs: 04-tech-civics-government (Policies); data/governments-policies (every card's effect text).

## Milestones (one PR each)

1. **Done: Generated policy modifiers.** The generator reads each card's effect text into modifiers (`policies.json`, per card `modifiers`; text it cannot read stays in `untrackedEffects`). It skips cards that already have hand-written modifiers. The core loads them like `modifiers.json`. This milestone covers the forms the modifier model already has, plus the requirements they need:
   - unit production by class and era (new `minEra` filter) or by unit;
   - combat XP; unit upkeep; district adjacency; strength against barbarians;
   - yields from a building; flat and percent city yields;
   - amenities, housing, loyalty and growth, with `where` conditions (new requirements: `CITY_HAS_GARRISON`, `CITY_MIN_SPECIALTY_DISTRICTS`, `CITY_HAS_GOVERNOR`);
   - granted abilities; builder charges; war weariness; plot cost; yield per citizen.
2. **Done: New effect kinds:** trade route yields (all, domestic, international, to allies and city-states), production toward wonders by era, specific buildings and districts and their buildings, great person points (player and per building), Diplomatic Favor and influence points per turn.
3. **Effects in code:** Bastions; Logistics and Integrated Attack Logistics; National Identity; Native Conquest; upgrade discounts (Professional Army, Retinues, Force Modernization); Raid and Total War; Second Strike Capability; Heritage Tourism, Satellite Broadcasts and Online Communities; spy cards (Cryptography, Machiavellianism, Nuclear Espionage, Non-State Actors); envoy cards (Containment, Diplomatic League, Merchant Confederation, Gunboat Diplomacy); per-suzerainty yields (Collective Activism, International Space Agency, Raj); colonial cards (not on the capital's continent); Communications Office; Space Tourism; Rabblerousing; Hallyu; Music Censorship; Aerospace Contractors; the "+50% from buildings in a district" cards (Free Market, Grand Opera, Rationalism, Simultaneum); resource accumulation cards.

## Decisions (Claude's recommendations; James gave standing consent)

- Specialty districts are those that count toward the population limit (`DistrictType::needsPopulation`).
- Milestone 1 as built: 117 modifiers from 60 cards. Unit production by class and era is merged into one modifier for each unbroken run of eras. The data's replacement cards (Chivalry, Grande Armée...) also list the earlier eras, so they cover them. `CITY_HAS_GOVERNOR` with a value counts the governor's titles (`promotions.size()`). AI: a card's modifiers count up to five, since one line of text can become many modifiers.
- Milestone 2 as built: 182 modifiers in all. New effects: `ADJUST_TRADE_ROUTE_YIELD` (scope ALL, DOMESTIC, INTERNATIONAL, ALLY, CITY_STATE, SUZERAIN; on the origin's yields, `tradeRouteModifierYields`); `ADJUST_ITEM_PRODUCTION_PERCENT` (WONDERS by the wonder's unlock era, BUILDING, DISTRICT, DISTRICT_BUILDINGS, SPACE_RACE; applied after the item's own bonuses, so it multiplies them); `ADJUST_GREAT_PERSON_POINTS` and `ADJUST_CITY_GREAT_PERSON_POINTS`; `ADJUST_FAVOR_PER_TURN` and `ADJUST_CITY_FAVOR_PER_TURN`; `ADJUST_INFLUENCE_PER_TURN`. Isolationism's domestic route bonus is now one of these generated modifiers, replacing its code. Policy text naming a civ's unique building or district (Tsikhe) is left untracked, since unique rows are not generated. Not modelled: route yields "to destination" (the partner's share: Wisselbanken, Democratic Legacy) and alliance points per turn.
