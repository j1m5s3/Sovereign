# Plan: the governor promotions still missing (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-espionage-depth.md`. Chosen by Claude under James's standing consent: the promotions `jit_history/2026-10-05-civ-systems-2.md` listed as "Not yet" whose effects the core can now carry. One milestone, one PR.

Specs: 08-diplomacy-city-states-governors (Governors); data/governors.

## Milestone

1. **Done: eleven more promotions,** checked through the new `Game::cityGovernorHas` (the city's own established governor holds it):
   - Victor: Embrasure (units trained in the city start with a promotion's XP), Defense Logistics (+1 strategic accumulation per source in the city), Arms Race Proponent (+30% production toward the Manhattan Project, Operation Ivy and the device projects).
   - Moksha: Citadel of God (Faith equal to a quarter of each building's cost on completion).
   - Magnus: Industrialist (+2 Production per power plant in the city, and its plants give +1 power per resource burned).
   - Liang: Reinforced Materials (the city's plots take no disaster damage), Water Works (+2 Housing per Neighborhood and Aqueduct, +1 Amenity per Canal and Dam).
   - Pingala: Curator (Great Works' tourism doubled in the city), Space Initiative (+30% production toward space race projects).
   - Reyna: Harbormaster (the Commercial Hub's and Harbor's adjacency doubled), Renewable Subsidizer (+2 Gold on renewable tiles and from the Hydroelectric Dam).

## Decisions (Claude's recommendations; James gave standing consent)

- Embrasure's extra city attack, Defense Logistics' siege protection, Citadel of God's immunity to religious pressure and combat, and Industrialist's resource power are read as stated above; the first three are not modelled.
- Still not carried: Air Defense Initiative, Foreign Investor, Grand Inquisitor, Laying On Of Hands, Patron Saint, Divine Architect, Black Marketeer, Vertical Integration, Aquaculture, Parks and Recreation (no Fishery or City Park improvements), Land Acquisition's trade-route gold, Contractor (no district purchase).
- AI: unchanged; its governor placement already promotes along the trees, and these promotions now pay where it puts them.
