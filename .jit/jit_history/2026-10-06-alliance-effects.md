# Plan: alliance level effects and destination route yields (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-levies.md`. Chosen by Claude under James's standing consent: `2026-10-05-diplomacy-depth.md` left most alliance level effects, and the destination's share of route yields, not modelled. One milestone, one PR.

Specs: 08-diplomacy-city-states-governors (Alliance levels table); data/governments-policies (Wisselbanken, Democratic Legacy).

## Milestone

1. **Done.**
   - Military:
     - level 2: +15% toward military units while the civ or its ally is at war with a major (`militaryAllianceAtWar`);
     - level 3: units trained start with a promotion's XP.
   - Religious:
     - level 1: no passive religious pressure between the allies' cities;
     - level 2: +10 religious strength;
     - level 3: +1 Faith in each city per follower of the ally's religion.
   - Cultural:
     - level 1: no loyalty pressure between the allies;
     - level 2: +1 great person point from each district in a city with a trade route to the ally;
     - level 3: 20% of the ally's tourism (`tourismBase` holds a civ's own tourism, so the shares cannot loop).
   - Economic, level 2: +1 influence a turn for each city-state the ally is suzerain of.
   - Destination yields (`tradeRouteDestinationYields`), paid in the destination city:
     - level 1 alliances: +1 Science (Research), +2 Gold (Economic), +1 Culture (Cultural) or +1 Faith (Religious);
     - policy route yields "to destination" (Wisselbanken, Democratic Legacy; generated `toDestination`).

## Decisions (Claude's recommendations; James gave standing consent)

- Not modelled:
  - Research level 2's research agreement;
  - Economic level 3's shared suzerain bonuses;
  - Religious level 3's +20 pressure;
  - alliance points from Wisselbanken and Democratic Legacy.
