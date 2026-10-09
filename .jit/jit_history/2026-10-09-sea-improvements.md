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

- AI Builders keep to land on their way (`build` moves them overland), so they improve no resource at sea.
- `tools/civ6_extract` could read the Domain column the next time it runs on the game's files; the generator would then no longer need Fishing Boats by name.
