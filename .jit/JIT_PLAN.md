# Plans: city sites, playtest readiness, then player retention (step 6, "everything else")

Status: active, 2026-10-08. Previous: `jit_history/2026-10-07-ai-planner.md`. James asked for the playtest work to be added to the plans and the work to keep going. Plan A is done; plan B is done except James's hands-on game; plan C is active (2026-10-08: with James's order list finished, I picked the decided retention features as the next work).

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
   - Seed 7's newcomer, who fortifies every unit, lost its only city (London, population 3) to AI seat 3 on turn 129. That is the AI taking a weak one-city neighbour, not a bug.
   - **Next: James plays.** A hands-on game from the menu; fix what he reports.
6. **Done: east-west wrap drawn.** The map actor draws a copy of itself a map's width to the west and east (`ASovMapActor::SyncGhosts`). The camera's focus stays within one copy, so panning past an edge carries on into the other. `LookAt` and the HUD's labels use the copy nearest the camera (`SovHex::NearestCopy`), and rivers, borders and roads across the wrap are drawn rather than skipped. Picking already wrapped through `HexGrid::normalize`. Checked in the game at column 0 of seed 7.

## Plan C: player retention (active)

The decided feature list in `specs/sovereign/player-retention.md` (James, 2026-10-04), in this order: what a playtest feels first, then what builds on the player profiles already carried between games.

1. **Done: the leader's goals at turn end (§4).** Core: `Game::leaderGoals(player)`, the next few personal goals, most pressing first: a promotion to take or close by, an assassin reported in place, a city near revolt, a rival leader within reach. The HUD shows the first two beside the turn line. Test: `leader_goals_put_the_most_pressing_first`.
2. **Done: rivals who remember you (§1).** Each AI civ's memory of a human (`RivalMemory`: games, wars, betrayals, rulers taken each way, cities lost, turns as friends) comes in through the setup from `<name>.rivals.txt` beside the play profile. This game's part is tallied each world turn (`processRivals`; battles now record `EventKind::LeaderLost`), merged and written when the game is saved or ends.
   - Effects stay inside the rules. A grudge (up to 30) and respect (up to 15) form the `PastGames` opinion reason. A grudge lowers the margin the AI needs to declare war on that human, and favours them as a target, by up to 30%. A leader beaten twice reads that human's play as at King. The diplomacy persona lists the history, and the scripted voice taunts or remembers lost crowns.
   - The menu toggles `Rivals remember you` (`-SovNoRivals`) and has `Forget your rivals`. Tests: `rivals_*` in test_profile, `the_persona_remembers_earlier_games`. Save version 80; golden hash regenerated.
3. **Done: chronicle and Hall of Sovereigns (§2).** Chronicle-worthy events are kept all game (`GameState::chronicle`, capped at 1000) and read out as plain lines (`Game::chronicleLines`). That covers wars, peace, friendships, assassinations, rulers captured or slain in battle (`LeaderLost`), successions (new `Succession` event), rebellions, historic moments and new ages. `F4` shows them. `F6`, and the game's end, have them written up off the game thread by the local model under a chronicler prompt, or by `scriptedChronicle` (`Saved/Sovereign/Chronicles/`). At the end the reign's `hallEntry` (ruler, gear, level, promotions, outcome, rivals made) is added to `Saved/Sovereign/Hall.txt`, which the menu shows. Battle replays are left for later. Save version 81.
4. **Done: shorter games (§5).**
   - **Short Reign** is a game speed, `GAMESPEED_SHORT_REIGN` (20% costs, 100 turns; `data/rules/setup.json`). Everything that already scales with speed (costs, era score, the World Congress interval, the turn limit for Score) follows it. Bench, 4 seeds, Small, 4 civs: by turn 100 it reaches era 7.4, 65 techs and 10.8 cities. Standard reaches era 7.9, 73 techs and 12.6 cities at turn 500.
   - **Era starts:** `GameSetup::startEra` and `eraStarts` in `setup.json`, from game-setup.md: Advanced start eras, the non-AI starting units, and the City Center starting buildings. Every civ and city-state gets the earlier eras' techs and civics without their moments, plus their envoys. Majors get the era's gold, faith and units on top of the Settler and Warrior. The world starts in that era, and cities founded during the game start with the era's population and City Center buildings (`Game::applyEraStart`, `applyFoundCity`).
   - Under ASan, every era start runs 25 AI turns cleanly, and a Renaissance Short Reign plays to a turn-93 Diplomatic victory. This found and fixed a write past `momentEras` in `awardFirst` when a moment fell outside its era window.
   - The menu chooses Length and Begin in (`-SovSpeed=`, `-SovEra=`); `sovsim` takes `--speed` and `--era`. Save version 82.
5. **Mods (§6).** Data and modifier mods loaded from a mods folder over the rules; mod lists matched when joining online.
6. **Weekly challenge (§3)** and **cosmetic unlocks (§7)**: the local parts (a seeded challenge setup, log validation by replay within the battle band; achievements unlocking skins).