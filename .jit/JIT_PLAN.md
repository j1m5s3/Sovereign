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
2. **Espionage:**
   - Spies join the leader's assassins in the agent pool the core already has.
   - Spy capacity comes from civics. Spies travel to their target.
   - Missions with district targets: Siphon Funds, Steal Tech Boost, Sabotage Production, Neutralize Governor, Foment Unrest, Listening Post, Gain Sources, Counterspy.
   - Success uses the 3d6 ladder and spy levels, with capture and escape.
   - AI and UI.
3. **Grievances, Diplomatic Favor and the World Congress:**
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
- Some promotion effects need systems the core doesn't have yet: nuclear projects, air defence, power plants, renewable improvements, space projects, canals and dams. They are listed in governors.json but have no effect until those systems exist; the milestone's notes list them.
