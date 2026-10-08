# Plans: city sites, then playtest readiness (step 6, "everything else")

Status: active, 2026-10-08. Previous: `jit_history/2026-10-07-ai-planner.md`. James asked for the playtest work to be added to the plans and the work to keep going. Plan A is done; plan B is active.

## Plan A: city sites and expansion (done)

At turn 200 the AI held about 7.7 cities a civ on a Small map, where Civ VI usually reaches 10–12.

**Diagnosis** (`sites` scratch tool, seeds 1–2):
- Land is not short: half the passable land is still unowned at turn 200, and 170 plots are still legal city sites.
- The settle scorer's threshold is not the limit: nearly every revealed legal site scores 150 or more.
- Civs reveal little of the map: 0–60 legal sites each at turn 50.
- Some civs see dozens of good sites and still train no Settler after turn 120, because Settlers lost weight then (600 → 400).

**Experiments** (8 seeds, turn 200, against main after #197):

| Build | cities | pop | techs | era | science | culture | prod |
|---|---|---|---|---|---|---|---|
| main | 7.7 | 45.6 | 28.9 | 2.9 | 69.2 | 49.7 | 95.8 |
| S1 Scouts, wider exploring | 6.8 | 40.7 | 28.5 | 2.8 | 62.4 | 45.9 | 85.5 |
| S1b Scouts only explore wider | 6.9 | 40.9 | 28.5 | 2.7 | 66.3 | 48.8 | 89.0 |
| S2 expansion weight to turn 200 (kept) | 8.3 | 46.3 | 29.0 | 2.9 | 67.2 | 49.0 | 95.7 |
| S1b + S2 | 7.6 | 42.9 | 28.4 | 2.7 | 65.1 | 47.3 | 91.0 |

**Kept:** S2. Settlers keep their expansion weight until turn 200 (`kEarlyTurns` 120 → 200), giving +0.6 cities.

**Dropped:** more scouting. Building a Scout early delays the first Settlers, and wider exploring lost cities in every variant.

## Plan B: ready for a playtest (active)

The game can be played end to end from the Unreal menu, but the map and controls make a playtest hard (unreal/README.md "Not yet", checked against the code on 2026-10-07).

1. **Done: map.** Borders, rivers, resources and improvements are drawn (roads already were), with a tile tooltip that includes yields.
2. **Done: saves.** `F5` and `F9` quicksave and quickload in local games, and the main menu can continue the four latest saves (`FSovSession::LoadLocal`).
3. **Done: panels.** The unit and city panels already existed, and `U` already offered promotions. The unit panel now lists promotions held; the city panel shows growth (turns to grow), housing, amenities and its buildings.
4. **Done: yields and help.** `F3` toggles yields on the viewer's plots; `F1` opens a how-to-play screen, hinted at in the status line.
5. **Playtest pass.**
   - **Done: automated playthrough.** `Sovereign.Bridge.HumanSeatPlaysLongGame` plays seat 0 as a newcomer for two games to turn 250 through what the controller offers, saving and resuming every 50 turns. It found no End Turn dead end. It did find that a game had no way out: there was no in-game menu, so after a victory or elimination the only exit was closing the window. `Esc` with nothing selected now opens the menu over the game (resume, save, continue, new game, quit). `F2` was missing from the README.
   - Seed 7's newcomer, who fortifies every unit, lost its only city before turn 250 on Prince. Noted for the hands-on pass, not changed.
   - **Next: James plays.** A hands-on game from the menu; fix what he reports.
6. **Next: east-west wrap drawn.** The map wraps in the rules but is drawn once (unreal/README.md "Not yet"); draw the wrapped edge so units crossing it stay in view.
