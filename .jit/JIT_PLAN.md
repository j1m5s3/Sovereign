# Plan: remaining Civ systems, part 2 (step 6, "everything else")

Status: active, 2026-10-05. Previous: `jit_history/2026-10-05-online-play.md`. Step 6's six named items are done; this plan takes the Civ VI systems the specs describe that the core does not have yet. James can redirect at any point.

Specs: 08-diplomacy-city-states-governors (Governors, Espionage, Diplomatic Favor and World Congress, Emergencies; War: grievances) and its data tables (governors, diplomacy-espionage, world-congress-emergencies); 09-civs-eras-victory-climate (Climate) with data/climate-disasters; the Sovereign overrides in leader-character-brainstorm (governors as successors, the leader's assassins).

## Milestones (one PR each)

1. **Done (this PR): Governors:**
   - Titles come from the fourteen title civics.
   - The seven governors, with base abilities and promotion trees from `data/governors.md` (generated into `governors.json`). Ibrahim is left out: he is Suleiman's alone, and Suleiman is not in the roster.
   - Appoint, promote and assign. Establishing takes 5 turns (Victor 3). An established governor gives +8 loyalty in its city.
   - Promotion effects go through the modifier model where they fit: a new governor source, scoped to the city where the governor is established. The rest get code hooks.
   - Amani in a city-state counts as 2 envoys.
   - The AI appoints, promotes and places governors. Unreal gets a chooser and HUD lines.
2. **Done (this PR): Espionage:**
   - Spies join the leader's assassins in the agent pool the core already has.
   - Spy capacity comes from civics. Spies travel to their target.
   - Missions with district targets: Siphon Funds, Steal Tech Boost, Sabotage Production, Neutralize Governor, Foment Unrest, Listening Post, Gain Sources, Counterspy.
   - Success uses the 3d6 ladder and spy levels, with capture and escape.
   - AI and UI.
3. **Done (this PR): Grievances, Diplomatic Favor and the World Congress:**
   - Grievances come from war declarations, captures, denunciations and broken promises. They decay by era and feed opinion.
   - Favor accrues each turn.
   - The Congress meets from the Medieval era, with the subset of resolutions whose effects the core can carry. Extra votes are bought with favor.
   - The Diplomatic Victory resolution and victory points.
   - AI voting and UI.
4. **Climate and disasters** (per data/climate-disasters):
   - CO2 from power and industry, and the eight global temperature stages.
   - Sea level rise and coastal lowlands.
   - Storms, floods, droughts and volcanic eruptions, with their yields and damage.

## Decisions (Claude's recommendations; James gave standing consent)

- Milestone 1 as built: promotions follow the tree with any one prerequisite (Civ VI draws the lines as alternatives). Carried by modifiers: Redoubt, Garrison Commander (defence, loyalty), Emissary, Bishop (pressure x2, +2 Faith per district), Groundbreaker, Surplus Logistics, Provision, Guildmaster, Zoning Commissioner, Librarian, Connoisseur, Researcher, Grants, Land Acquisition (border rate), Forestry Management, Tax Collector. In code: establishing, +8 loyalty, Amani's envoys (Messenger, Puppeteer) and Affluence (her city-state's luxuries). Not yet: Defense Logistics, Embrasure, Air Defense Initiative, Arms Race Proponent, Local Informants (espionage, milestone 2), Foreign Investor, Grand Inquisitor, Laying On Of Hands, Citadel of God, Patron Saint, Divine Architect, Industrialist, Black Marketeer, Vertical Integration, Aquaculture, Reinforced Materials (milestone 4), Water Works, Parks and Recreation, Land Acquisition's trade-route gold, Harbormaster, Contractor, Renewable Subsidizer, Curator, Space Initiative. Governors as successors to the throne (leader doc §5) wait for a pass on succession.
- Milestone 2 as built: spies are `Agent`s with `spy` set; capacity is the sum of `spies` on finished civics and techs (Diplomatic Service, Nationalism, Ideology, Cold War, Computers); assassins keep their own Encampment capacity. Travel 3 turns (speed-scaled); operations from `espionage.json`. Success: 3d6 at or above base - 2 - (level - 1) x levelChange - Gain Sources bonus, plus enemyChange + enemyLevelChange x (counterspy level - 1) when a counterspy (or Amani's Local Informants, as 3 levels) defends. Failure: escape on 3d6 >= ESPIONAGE_ESCAPE_BASE_CHANCE less levels (plus counterspy levels), else capture; the target remembers either. Success: +1 level, the spy stays; a narrow success is noticed. Siphon Funds takes the city's gold per turn x (3 + level) (Sovereign reading; the engine formula is unverified). Not yet: Great Work Heist, Recruit Partisans, Breach Dam, Disrupt Rocketry, Fabricate Scandal, spy promotions, trading captured spies back, the Intelligence Agency.
- Milestone 3 as built: grievances (Sovereign's bases where the engine's are unverified): formal war 100, surprise 150, declared friends of the victim 25% of that, denunciation 25, capture 50, razing 100, a caught spy 25, conquering a city-state 50 with every civ, a held city +3/+1 per turn (GRIEVANCES_POSSESS_*); decay by the world era (Eras grievance decay). They show as an opinion reason (-1 per 10, at most -30) and cost favor above 200 held against a civ (1 per 50, at most -10). Favor: government tier and +1 per suzerainty. The Congress first meets when a civ reaches the Medieval era, then every 30 turns (speed-scaled); a session holds over one world turn: AIs vote as it opens, humans on their turn; the Diplomatic Victory resolution comes up at every session from the Modern era, plus one or two drawn from those the core carries: Trade Policy, Patronage, Migration Treaty, Public Relations, Military Advisory, Urban Development Treaty (option A only). Ties go to A; the target with most votes among the winning option's voters. 20 Diplomatic Victory points win. Not yet: the other resolutions, special sessions, emergencies and competitions, promises, favor in trades, alliances' favor.
- Some promotion effects need systems the core doesn't have yet: nuclear projects, air defence, power plants, renewable improvements, space projects, canals and dams. They are listed in governors.json but have no effect until those systems exist; the milestone's notes list them.
