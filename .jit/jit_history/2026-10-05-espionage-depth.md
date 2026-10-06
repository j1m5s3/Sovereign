# Plan: espionage depth: the remaining operations and spy promotions (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-diplomacy-depth.md`. Chosen by Claude under James's standing consent: the espionage the specs describe and the core still lacked after `jit_history/2026-10-05-civ-systems-2.md` ("Not yet: Great Work Heist, Recruit Partisans, Breach Dam, Disrupt Rocketry, Fabricate Scandal, spy promotions..."). One milestone, one PR.

Specs: 08-diplomacy-city-states-governors (Espionage); data/diplomacy-espionage (Spy operations); data/promotions (Espionage).

## Milestone

1. **Done: the remaining operations and spy promotions:** Great Work Heist, Recruit Partisans, Breach Dam, Disrupt Rocketry and Fabricate Scandal; a promotion chosen for every level a spy gains, with the effects the core carries. AI uses the new operations and picks promotions; the Unreal spy chooser offers both.

## Decisions (Claude's recommendations; James gave standing consent)

- Operations (`SpyMission` 9-13, save versions unchanged by the enum): Great Work Heist (Theater Square, base 15) moves the target city's first Great Work that one of the thief's cities has a free slot for (no slot: the success takes nothing); Recruit Partisans (Neighborhood, base 16) raises two barbarian rebels of the strongest generic melee unit the target can field within 2 tiles of the city; Breach Dam (Dam, base 15) pillages the city's floodplain improvements for 5 turns and takes 30 HP (never below 1) from units on them; Disrupt Rocketry (Spaceport, base 15) zeroes the progress of the city's space race projects; Fabricate Scandal (16 turns, base 13) works in a city-state's city whose suzerain is another met civ, removes 1 + the spy's level of that suzerain's envoys there, and wrongs the suzerain (grievances, memory). Sovereign readings where the engine is unverified: the partisans' unit and count, the flood's damage and pillage turns, the envoys removed.
- Spy promotions (`spyPromotions` in espionage.json, generated from data/promotions.md's Espionage table; `Agent::promotions`, `promotionsPending`; save version 42; command `PromoteSpy` 53): every level gained (to ESPIONAGE_MAX_LEVEL 4) gives a promotion to choose, each once. Carried: +2 levels on named operations (Cat Burglar, Demolitions, Con Artist, Guerrilla Leader, Rocket Scientist, Smear Campaign, Covert Action, License to Kill, Satchel Charges, Seduction on six), +1 level on all (Quartermaster, Polygraph; also when counterspying), Ace Driver's escape (+4 off the 3d6 need), Disguise's establishing 100% faster (travel halved), Linguist's operations 25% faster, Surveillance's +1 counterspy level. The Technologist is not in the data.
- AI: spends pending promotions in a fixed order (Seduction, Quartermaster, Polygraph, Con Artist, Ace Driver, Linguist, then the first it lacks); adds Disrupt Rocketry (worth most against a city building space race projects), Great Work Heist (only with a free slot at home), and, at war with a major, Recruit Partisans and Breach Dam to its operation choices.
- Unreal: the operation names cover all thirteen; the spy's operation chooser lists its pending promotions first.
- Still not modelled: trading captured spies back (since added: `2026-10-06-captured-spies.md`), and Police State (not in the extracted policies).

## Follow-up (2026-10-05)

- **Listening Post.** A spy running one in a civ's city hears all that civ's gossip (`hearsOf`).
- **Intelligence Agency.** One more spy (`spyCapacity`), and spies trained by its owner start at level 2.
- **Chancery.** Catching an enemy spy gives Science toward current research: 50 per level of the spy caught (`Game::buildingsOwned` helps count buildings).
