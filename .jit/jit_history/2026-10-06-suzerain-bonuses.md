# Plan: city-state suzerain bonuses (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-alliance-effects.md`. Chosen by Claude under James's standing consent: of the 48 city-states' suzerain bonuses (08: Suzerain), only Cardiff's is applied; the rest are kept as text (`suzerainText` in `citystates.json`). James can redirect at any point.

Specs: 08-diplomacy-city-states-governors (Suzerain); data/city-states (every city-state's suzerain bonus).

## Milestones (one PR each)

1. **Done: Generated suzerain modifiers.** A new modifier source, `ModSource::CityState`, applies to the city-state's suzerain while they are at peace, and to the suzerain's level-3 Economic allies (08: alliance levels). Suzerainty moves into a free function the modifier code can call. The generator reads each bonus with the policy parser plus a few city-state forms:
   - wonder and project production;
   - Geneva's peace condition; Auckland's coast production and era condition;
   - Mitla's district condition, Taruga's improved-resource condition, Muscat's amenities.
2. **Done: Bonuses in code:** Akkad (walls), Kabul (XP), Ayutthaya (Culture on buildings), Anshan (Great Works' Science), Antananarivo (Culture per great person), Hattusa (unimproved strategics), Hunza (route length), Johannesburg (resources), Singapore (trade partners), Valletta (walls and Faith purchases), Mohenjo-Daro (fresh water housing), Venice (luxuries at the destination), Kumasi (routes to city-states).

## Decisions (Claude's recommendations; James gave standing consent)

- Not planned:
  - the city-states' unique improvements ("can build ...": Monastery, Batey, Mounds, Alcázar, Colossal Head, Mahavihara, Moai, Nazca Line, Trading Dome);
  - Zanzibar's imported luxuries;
  - Jerusalem's Holy City;
  - Vatican City's pressure;
  - Mexico City's regional range;
  - Bandar Brunei's Trading Posts;
  - Buenos Aires's bonus-resource amenities;
  - Lahore's Nihang;
  - Yerevan and Wolin's promotions.
- Milestone 1 as built: modifiers for 11 city-states (Auckland, Bologna, Brussels, Geneva, Hong Kong, Mogadishu, Muscat, Mitla, Preslav, Taruga, Wolin; Cardiff stays in code). `envoysAt`, `suzerainOf` and `enjoysSuzerainBonus` are free functions in `modifiers.cpp`; the `Game` methods call them, and Cardiff now uses `enjoysSuzerainBonus` too. New requirements: `PLAYER_AT_PEACE`, `WORLD_MIN_ERA` (Auckland's Industrial-era line reads the world era), `CITY_HAS_IMPROVED_RESOURCE`, and a district form of `city has`. The new item production scope `PROJECTS` covers every project. Mogadishu's and Wolin's abilities are granted but their effects are text.
- Milestone 2 as built (`Game::suzerainBonus`). Akkad: melee and anti-cavalry units deal full damage to walls. Kabul: double XP from attacking. Ayutthaya: Culture of a tenth of each building's cost when it is done. Anshan: +2 Science from writing, +1 from artifacts and relics. Antananarivo: +2% Culture per great person recruited. Hattusa: +2 a turn of each revealed strategic resource with no improved source. Hunza: +0.2 Gold per plot between a route's cities. Johannesburg: +1 Production per kind of improved resource in the city, +2 after Industrialization. Singapore: +2 Production per major trade partner. Valletta: City Center buildings for Faith at their Gold price, and walls at half price (walls are not purchasable in the data, so that part waits). Mohenjo-Daro: every city houses as if on a river. Venice: +1 Gold per improved luxury at an international destination. Kumasi: +2 Culture and +1 Gold per district on routes to city-states. Not modelled: Chinguetti's Faith per follower (the data's rate would swamp Faith), and Fez.

## Follow-up: the city-states' unique improvements

- The nine improvements (Armagh's Monastery, Caguana's Batey, Cahokia's Mounds, Granada's Alcázar, La Venta's Colossal Head, Nalanda's Mahavihara, Rapa Nui's Moai, Nazca's Line, Samarkand's Trading Dome) are generated with `cityState`. A player's Builders may build one while it enjoys that city-state's suzerain bonus, and what is built stays. Improvement adjacency can now also count districts (a type, or any, the City Center included), features (woods, rainforest) or resource classes (bonus, luxury). The Alcázar's defence of 4 is read. Not modelled: their Modifiers column (the Moai's coast Culture, the Monastery's housing with Colonialism, the Mounds' amenities, the Nazca Line's yields to plots within range), Nalanda's free tech, the Mahavihara's Lavra and Observatory adjacencies (civ-unique districts), and Samarkand's Trading Dome route Gold.
