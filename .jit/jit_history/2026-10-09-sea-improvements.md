# Record: sea and land improvements keep to their own kind of plot (rules fix)

Status: done, 2026-10-09. Found by Claude while looking at the plots AI Builders leave unimproved.

## What was wrong

- A plot with a visible resource took any improvement that lists the resource, whatever its land. Amber (on the coast and lakes, in rainforest and woods) and Oil (on land and at sea) each have a sea and a land improvement, so a Builder could put Fishing Boats on Amber in the woods, a Mine on Amber at sea, an Offshore Oil Rig on Oil on land or an Oil Well at sea. In 8 AI games (Small, 6 AI) to turn 300 on main, all 19 Fishing Boats stood on Amber on land; with the fix there are none of the wrong kind.

## Fix

- The extract leaves out the Improvements table's Domain, so `gen_rules.py` marks the improvements that work the sea `water`: Fishing Boats by name, and those whose land is water only (Offshore Oil Rig, Offshore Wind Farm, Seastead, the Fishery).
- `improvementFits` (improvements.cpp): an improvement that works the sea goes on water only, the others on land only.

## Tests

- `sea_and_land_resources_take_improvements_of_their_own_kind`: Amber in the woods takes a Mine, not Fishing Boats, and at sea Fishing Boats, not a Mine; Oil on land takes an Oil Well, at sea an Offshore Oil Rig. On main the four checks of the wrong kind fail.
- The golden file's rules checksum changes; the game's state hash does not. As with any change to the rules data, a save made before it does not load after it.
- Save and reload: 8 AI games in 5 setups, each saved and loaded once or twice, end on the straight game's state.

## Left open

- AI Builders kept to land on their way (`build` moved them overland), so they improved no resource at sea: done in the follow-up below.
- `tools/civ6_extract` could read the Domain column the next time it runs on the game's files; the generator would then no longer need Fishing Boats by name.

## Follow-up: AI Builders embark for sea resources (2026-10-09)

- **What was wrong.** `build` (ai.cpp) sent every Builder to its plot over land, so no AI Builder reached a resource at sea. In 8 AI games (Small, 6 AI) to turn 200 the major civs held no improvement at sea, with 214 sea resources on their cities' land (21 to 35 a game). The Builder floor in `production` still counted those plots as work.
- **Fix.** A Builder goes to a plot at sea by embarking (Builders may after Sailing; 05: Embarkation) and to a plot on land over land, as before. After a failed move it finds its reach for each way the first time a plot of that kind comes up.
- **Results** (128 AI games, Small, 6 AI, turn 200, against the sea fix above): population 54.3 to 55.4 (t 2.2), Gold held 179 to 189 (t 2.4), Culture 59.6 to 60.8 (t 1.7), Production 146.5 to 143.9 (t -1.5); Science, techs, civics and cities unchanged. In the 8 games above, 160 of the 202 sea resources on the major civs' land are improved at turn 200. The 150-turn benchmark game takes the same CPU time (1.10 s both, 4 runs each).
- **Tests.** `ai_builders_embark_for_sea_resources`: a Builder after Sailing embarks for Fish beside its city and builds Fishing Boats, also when its better plots (Horses on an island) are out of reach over land. `ai_builders_work_what_they_can_reach` now also runs after Sailing: the island's Wheat stays out of reach over land, and the Builder works the mainland. On the code before, the new test fails. Of 12 mutants of the change (each condition broken in turn), 10 fail a test and 2 fail the build (`failed` would go unread).
- **Save and reload:** 8 AI games in 5 setups, each saved and loaded once or twice, end on the straight game's state.
- **Left open:** a Builder still goes to a plot on land over land, so land a city holds across water (an island within its three rings) stays unimproved by the AI.
