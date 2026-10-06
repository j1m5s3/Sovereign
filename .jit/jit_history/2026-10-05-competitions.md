# Plan: scored competitions (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-engineers.md`. Chosen by Claude under James's standing consent: the World Congress competitions [GS] the specs describe ("Not yet: ... special sessions, emergencies and competitions"), a Diplomatic Victory points source the core lacked. One milestone, one PR.

Specs: 08-diplomacy-city-states-governors (Scored Competitions, Diplomatic Victory); data/world-congress-emergencies (Emergencies and competitions, Emergency rewards, Emergency / competition score sources).

## Milestone

1. **Done: seven competitions** (`src/competitions.cpp`; `GameState::competitions`, save version 43): World's Fair, World Games, Nobel Prizes in Literature, Peace and Physics, Climate Accords and the International Space Station, called at a World Congress session, scored over 29 turns, rewarded at the end. The HUD shows the running one with our score and the leader's.

## Decisions (Claude's recommendations; James gave standing consent)

- Calling: when a session opens and no competition is running, one of those the world era allows is drawn (not the one just held): World's Fair from the Industrial era; World Games and the Literature and Physics prizes from the Modern; the Peace prize from the Atomic; the Climate Accords from the Atomic or once the climate has warmed; the Space Station from the Information era (Sovereign's era gates; the data gives none). Every major civ takes part (Civ's "eligible" rules are not modelled).
- Scores, from the data's sources: World's Fair, great person points earned (Prophets aside); World Games, Stadiums and Aquatics Centers held at the end (the project source has no project in the core); Literature, Writers, Artists and Musicians recruited; Physics, Scientists, Engineers and Merchants recruited; Peace, Diplomatic Favor gained (net of spending); Climate Accords, the least CO2 emitted (decommissioning plants is not modelled); Space Station, 30 per space race project completed.
- Rewards: first place takes the Diplomatic Victory points the data lists (World's Fair, World Games, Peace prize, Space Station +1; Climate Accords +2) and, at the World's Fair, +100 points toward every great person class. Tiers are Sovereign's reading: the rest of the top half is "high", the others with any score "low". High tier +50 Diplomatic Favor (Climate Accords +100), low tier at the Climate Accords +50, and every placed civ in the Physics prize gets a Eureka toward an Industrial-or-later tech. Not carried: Rock Band rewards (Literature), free great people (Peace), tourism on districts (World Games), inspirations (World's Fair tiers), strategic accumulation, the Space Station's light-years and production bonuses.
- Aid Requests and Military Aid Requests (the Send Aid project) and special sessions are still not modelled.
