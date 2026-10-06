# Plan: city-state suzerain bonuses (step 6, "everything else")

Status: active, 2026-10-06. Previous: `jit_history/2026-10-06-alliance-effects.md`. Chosen by Claude under James's standing consent: of the 48 city-states' suzerain bonuses (08: Suzerain), only Cardiff's is applied; the rest are kept as text (`suzerainText` in `citystates.json`). James can redirect at any point.

Specs: 08-diplomacy-city-states-governors (Suzerain); data/city-states (every city-state's suzerain bonus).

## Milestones (one PR each)

1. **Done: Generated suzerain modifiers.** A new modifier source, `ModSource::CityState`, applies to the city-state's suzerain while they are at peace, and to the suzerain's level-3 Economic allies (08: alliance levels). Suzerainty moves into a free function the modifier code can call. The generator reads each bonus with the policy parser plus a few city-state forms:
   - wonder and project production;
   - Geneva's peace condition; Auckland's coast production and era condition;
   - Mitla's district condition, Taruga's improved-resource condition, Muscat's amenities.
2. **Bonuses in code:** Akkad (walls), Kabul (XP), Ayutthaya (Culture on buildings), Anshan (Great Works' Science), Antananarivo (Culture per great person), Hattusa (unimproved strategics), Hunza (route length), Johannesburg (resources), Singapore (trade partners), Valletta (walls and Faith purchases), Mohenjo-Daro (fresh water housing), Venice (luxuries at the destination), Kumasi (routes to city-states).

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
