# Plan: great person activation effects (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-policy-cards.md`. Chosen by Claude under James's standing consent: 141 great person activation effects are still kept only as text (`untrackedEffects` in `greatpeople.json`). James can redirect at any point.

Specs: 07-economy-trade-great-people (Great People); data/great-people (activation and passive effects).

## Milestones (one PR each)

1. **Done: Lasting effects as generated modifiers.** The generator reads the lasting effects into modifiers, reusing the policy card parser. The core applies them once the person is used: player-wide, as `greatPeopleActivated` already does for building yields and abilities; or in the city where they were used (new `City::greatPeopleHere`, save change). Covered:
   - war weariness;
   - production toward military units and space projects;
   - tourism toward civs with our routes, and from artifacts;
   - district tourism;
   - loyalty, amenities, housing and appeal in that city.
2. **Done: One-time effects in code:**
   - envoys (and free envoys at a city-state), governor titles and trade route capacity;
   - relics, and resources per turn;
   - random techs and science bursts (Galileo, Janaki Ammal, Mary Leakey, Charles Darwin, Hildegard);
   - formations (Corps, Army) for the unit on the plot;
   - granted units (Tupac Amaru, Hanno), district capacity (Bi Sheng, Ada Lovelace);
   - Imhotep's wonder production, the XP abilities for the unit on the plot, and the rest as far as the core's systems reach.

## Decisions (Claude's recommendations; James gave standing consent)

- Milestone 1 as built: 20 modifiers from 18 individuals (`gp_modifiers`; source = the individual; new `ModSource::GreatPerson`). City effects (collection `OWNER_CITY`) apply in every city whose `greatPeopleHere` lists the individual. That is the city whose land it stood on when used (save version 65). Player effects apply once `greatPeopleActivated` lists it, which now records every individual used, not only those with building yields or abilities. New effects: `ADJUST_ROUTE_TOURISM_PERCENT` (Sarah Breedlove, Melitta Bentz); `ADJUST_DISTRICT_TOURISM` (Masaru Ibuka, Jamsetji Tata: +10 from each of the player's districts of the type); `ADJUST_CITY_APPEAL` (Alvar Aalto, Charles Correa: on every plot of the city); and a `military` filter on unit production (Eisenhower, Themistocles, Nimitz). Mary Leakey's tripled artifact tourism is read in code. Found on the way: a great person with no typed effect could not be used at all; one with modifiers now can.
- Milestone 2 as built: 20 new typed effects (`gp_more_effects`), 118 typed in all, 49 lines still text. One-time effects:
  - envoys, and envoys at the city-state it stands in (Rogue State blocks both);
  - governor titles; relics into free relic slots;
  - random techs, completed; a Corps or Army for the military unit here;
  - Tupac Amaru's unit in each district; the best naval melee unit (Hanno);
  - Science per adjacent Mountain or Rainforest (Galileo, Janaki Ammal), per artifact in the city (Mary Leakey), or next to a natural wonder (Darwin);
  - Imhotep's 350 toward an Ancient or Classical wonder being built, else 175;
  - +N% XP for good for the military unit here (`Unit::xpBonus`, save version 66);
  - Boudica's neighbouring barbarians joining; Matthew Perry's suzerainty, which removes the other civs' envoys there.

  Lasting effects, read through `greatPersonEffectTotal` / `cityGreatPersonEffectTotal`: trade route capacity; resources a turn; district capacity in the city where it was used; ocean entry (Leif Erikson); Mary Leakey's artifact tourism.
- Still text (49 lines): luxury corporations (Spilsbury, Rubinstein, Strauss, Lauder), route yields for other civs (Zheng He, Zhang Qian, Marco Polo), Kenzo Tange's and Ibn Khaldun's conditional yields, Tesla's and Paxton's regional effects, Mimar Sinan's culture bomb, Crassus's plot, Shah Jahan's purchase, Raffles, Medici's palace slots, James Young's visibility, Hildegard, Roebling and Drew (per great person), Abu al-Qasim's healing, Marina Raskova's air slot, Magellan's and Colaeus's resource, Mary Katherine Goddard, Todar Mal's routes and Rockefeller's routes.
