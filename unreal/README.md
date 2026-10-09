# Sovereign Unreal project

The Unreal Engine 5.8 front end (build-plan step 2 of
[engine-and-architecture.md](../specs/sovereign/engine-and-architecture.md)): a plain
hex map driven by the rules core in [../core](../core). The core decides everything;
Unreal draws what seat 0 knows and sends player orders back as `sov::Command`.

## Modules

| Module | What it is |
|---|---|
| `SovereignCore` | The rules core. `Private/Core/SovCore_*.cpp` are generated one-line wrappers that `#include` each `core/src/*.cpp`, so `core/` holds no Unreal file. Regenerate them with `python tools/check_unreal_core_module.py --write` after adding a core source (CTest fails until you do). |
| `SovereignBridge` | The primary game module: `FSovSession` / `USovGameSubsystem` own the `sov::Game` and step AI seats with `sov::ai::playTurn`; `BuildMirror` flattens seat 0's knowledge; `ASovMapActor` draws it; `ASovPlayerController` turns input into commands; `ASovHUD` draws the canvas HUD. |

The editor loads each module as a DLL, so the core's public API is marked `SOV_API`
(`core/include/sovereign/api.h`, empty in the standalone build). Add it to new public
classes and free functions in core headers; the Unreal link step names any you miss.

There are no `.uasset` or `.umap` files: the game runs on the engine's empty
`/Engine/Maps/Entry` map, and `ASovGameMode` spawns the light, sky and map from C++
using engine basic shapes.

## Build

Needs Unreal Engine 5.8 and Visual Studio 2026 (or 2022) with the Game development with
C++ workload.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" SovereignEditor Win64 Development -Project="C:/source/games/Sovereign/unreal/Sovereign.uproject" -WaitMutex
```

Or right-click `Sovereign.uproject` > Generate Visual Studio project files and build
`SovereignEditor` from the IDE.

## Run

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/source/games/Sovereign/unreal/Sovereign.uproject" -game -windowed -ResX=1600 -ResY=900
```

Options: `-SovMonopolies` (the Monopolies and Corporations mode: a Builder on an improved luxury founds an Industry after Economics and grows it into a Corporation after Electricity, from `B`; most of a luxury's sources make a Monopoly), `-SovClans` (the Barbarian Clans mode: from the diplomacy chooser `N`, bribe a camp you have seen, hire its best unit, or incite it against a civ; camps grow into city-states), `-SovBattleDemo` (developer start: your warrior on your leader's plot and an enemy warrior next to it, at war, to try a live battle at once), `-SovNavalDemo` (developer start: Shipbuilding, a galley and an embarked warrior on the coast nearest your leader), `-SovSeed=N` (default 7), `-SovPlayers=N` (default 4), `-SovSize=MAPSIZE_TINY`,
`-SovSpectate` (the AI plays every seat while you watch seat 0's view). Rules are read
from `../data/rules`.

## Game screen

Over the map (`SovGameUI`, styled by `SovStyle`, icons in `Content/Slate/Icons` drawn by `tools/ui/icons.py`):

- **Top bar:** science, culture, gold, faith, favor and tourism per turn (click science or culture for research or civics, favor for the World Congress). Research and civic in progress with turns left and a progress line (click to choose). Government (F2), the turn and era, and the menu.
- **Unit panel** (bottom left, for the selected unit; an outline on the map shows where it can go this turn, and resting the cursor on a plot shows the path there with the turn each stretch ends; enemies it can strike this turn are ringed in red): name, health bar, moves, strength, level and XP, and a button for each action (found, build, trade, religion, great person, promote, gear, escort, streets, fortify or sleep, skip). Each button presses the action's key.
- **City panel** (bottom left, for the selected city; its plots show their yields, the worked ones edged in gold): population, health and loyalty, yields per turn, growth with housing and amenities, what it builds (click to choose) with buy buttons for gold or faith, the citizens' focus, its buildings, districts, religion and timers, and buttons for production, Benevolence, Fear, governors and plot yields (F3; Shift+click a plot to lock a citizen there).
- **Tech and civic trees** (T, C, or the top bar): the whole tree by era with prerequisite lines, turns, boosts and unlocks. Click an open node to start it, or a later one to make it your goal: the path toward it is started step by step.
- **Choosers:** every list (production, research, civics, government, promotions, great people, governors, diplomacy...) opens as a clickable, scrolling panel on the right. The number keys still pick, and Esc or X closes it.
- **Live battle:** both armies' strength and the clock at the top, your leader's health, and buttons for each squad and order, charge, strike and settle (the keys still work). `-SovBattleDemo -SovBattleNow` starts one at once for testing (press B).
- **Government** (`F2`, or the top bar): the governments with their slots; your policy slots as cards; click a slot, then a card. Dedications are chosen here too.
- **Hover:** resting the cursor on the map shows the plot: terrain, resource, improvement, district, owner, what it yields its city, and the units you see there.
- **Empire panel** (`F8`, or the top bar's era button): your cities and units (click one to go to it), your civ, faith, era and age, diplomacy, the world's contests, climate, governors and leader.
- **Notifications** (above end turn): cities waiting for production and anything else waiting on you (an offer, the throne, a pantheon, dedications, governor titles, envoys), then news from the last two turns. Click one to open its screen (diplomacy, great people, World Congress, the chronicle), or X to dismiss it. Hover the top bar's yields for a city-by-city breakdown.
- **Lenses and minimap** (bottom right): the lens buttons recolour the map for religion, loyalty, appeal, where a city can be founded, or your trade routes, with a legend; click again (or `F7` to step through them) to clear. The minimap shows what you have revealed, in owners' colours or the lens's; click or drag on it to move the camera.
- **End of game:** Victory or Defeat with who won and how, the scores and your chronicle; look at the map, write the chronicle up, or go to the menu.
- **End turn** (bottom right): names what stands in the way (a unit needing orders, production, research, a civic, a successor). Clicking it does what Space does: ends the turn, or opens what is needed.

## Menu screens

- **New game** (the first button on the menu): pick a civ (or Random) and read its abilities, uniques and agenda; set the map size, number of civs, difficulty, length, start era, natural disasters, Barbarian Clans, Monopolies and rival memory. `-SovSetup` opens it at launch, `-SovCiv=CIVILIZATION_EGYPT` picks the civ for a command-line game.
- **Settings:** graphics quality, window mode, resolution, vsync, frame limit and interface scale, and the controls: click an order key, then press its new key (an action already on that key swaps to the old one; movement, digits, Esc, Enter and Space stay fixed). Saved in GameUserSettings. `-SovSettings` opens it at launch.

## Controls

| Input | Action |
|---|---|
| Left click | Select your unit or city (click again to cycle units, then the city) |
| Right click | Selected unit: move, or attack an enemy unit, city or Encampment (ranged if it can); an aircraft rebases, a land unit on an Aerodrome with an Airport airlifts to another (Rapid Deployment), a Spec Ops paradrops up to 3 plots from your land. Selected city: city strike |
| Shift | With one of your cities selected: Shift+left-click a plot locks a citizen to it (or frees it); Shift when picking from the `P` list adds the item to the queue |
| Alt+right click | Launch the strongest nuclear device you hold from one of your Missile Silos in range of the plot |
| `F` | Found a city with the selected settler (opens the production chooser) |
| `K` / `G` | Skip the unit this turn / fortify (military) or sleep (civilian) |
| `B` | Builder: choose an improvement or harvest here |
| `E` | Leader (in your city): change weapon, armor or mount (costs gold and the leader's turn) |
| `L` | Link the leader and the military unit on its plot as escort (they move together), or release it |
| `H` | The throne: choose a successor after the leader falls (an heir may keep one promotion), or abandon a captured leader |
| `U` | Promote the selected unit or leader (the leader has three branches; only one can be finished per reign) |
| `Q` | Leader in one of your cities: walk its City Center at street level (autosaves first). WASD walk, hold right mouse or Q/E to look, `F` talks to the herald (Benevolence) or the captain of the guard (Fear), `Esc` returns to the map |
| `V` / `X` | City panel with the leader in that city: Benevolence / Fear without walking (classic control) |
| `B` / `R` | A melee involving your leader's stack waits for you (even on an AI's turn): `B` fights it as a live medieval battle (autosaves first), `R` auto-resolves it. In battle: WASD move the leader, left click or `F` strike, `Tab` charge or hold your men, `1`-`6` order your squads (advance, hold, flank left, flank right, fall back, hunt their leader; `7` `8` `9` pick the left, centre or right squad, `0` all), hold right mouse or Q/E to look, `Esc` settles it now. The trained battle AI (`data/battle_ai`) leads the enemy. The field result goes to the core, which keeps it within 25% of the expected Civ result |
| `J` | Agents. Assassins: send an idle one after a rival ruler (shows the odds when that ruler is in sight), or recall one. Spies (train them once Diplomatic Service and later civics give spy capacity): pick one, then an operation in a city you have seen, each offensive one with its chance of success; the spy travels there (3 turns), works the operation and reports. The news lines tell of your spies' successes and captures, and of spies caught in your lands |
| `I` | Choose a pantheon once you have 25 Faith. A selected Great Prophet: `F` on a Holy Site founds a religion (pick a Founder, then a Follower belief). A selected Missionary or Apostle: `F` spreads its religion in the city whose land it stands on; an unused Apostle may add a belief instead. Right-click a foe's religious unit for theological combat. Religious units and worship buildings are bought with Faith from the `P` list |
| `P` wonders | World wonders show in the production list with the plot they would take; built wonders stand on the map as temples, wonders being built as monuments |
| `F` (Trader) | Start a trade route from the Trader's city: destinations in range with what each pays per turn. Roads appear on the map along routes |
| HUD | The world era, your age and era score against its thresholds, tourism with visiting and home tourists; historic moments and new ages appear in the news lines |
| `O` | City-states you have met, with their kind, your envoys and their suzerain; pick one to send an envoy (the HUD shows envoys waiting) |
| `N` | Diplomacy: pick a leader you have met (with how they feel about you, and whether an offer of theirs waits), then talk. The screen shows their relationship, agenda, the reasons behind their opinion, what each side could offer and your past talks. Type and press Enter: a local model (a `llama-server` on this machine, port 8080 or `-SovLlmPort=`) or, without one, the scripted leader replies. Anything you propose shows with the rules' verdict; `Put the proposal forward` sends it (the AI answers by the rules alone), and their own offers can be accepted or rejected. `Leave` records a summary of the talk as the leader's memory. `-SovDiploDemo` starts with every civ met and an offer waiting. In the Barbarian Clans mode (`-SovClans`) the list also offers bribes, hires and incitements for the camps you have seen |
| Menu | Launched without `-Sov` start options, the game opens a menu: single player, hot seat, host on your network, join by address, host for Steam friends, join a Steam friend, quit. `Rivals remember you` (on by default; `-SovNoRivals` turns it off) lets AI leaders keep a memory of you between games: wars, betrayals, rulers captured or slain, friendships. It shapes their opinion and whom they attack, and they bring it up in talks. `Forget your rivals` clears it. `Length` picks the game speed, Short Reign being a 100-turn game at a fifth of the costs (`-SovSpeed=GAMESPEED_SHORT_REIGN`). `Begin in` starts in a later era with the earlier eras' techs and civics, more units, gold and faith, and larger new cities (`-SovEra=ERA_MEDIEVAL`). `Mods` lists the installed data mods (the repo's `mods/` and `Saved/Sovereign/Mods/`, see `mods/README.md`) to turn on for new games (`-SovMods=a,b`); a save loads with the mods it was made with. `Weekly challenge` starts this week's game, the same for everyone: map, civ, Short Reign, start era and a goal, with your last result beside it. At its end the result is kept in `Saved/Sovereign/Challenges.txt` and the game is saved as `challenge week N`, whose log the board checks by replay. `Achievements (N of 9)` lists what you have earned. Each unlocks a colour for your ruler's figure, chosen with `Ruler's colour` (cosmetic, on your screen only). `Battle replays` plays back your latest live battles, each kept in `Saved/Sovereign/Battles/` as a `.sovbattle` file; `-SovReplay=<file>` opens one at start, so a battle can be shared |
| Turn line | Beside the turn hint, your leader's next one or two goals: an assassin reported close, a restless city to visit, a rival leader within reach, a promotion waiting or close |
| Steam | Host for Steam friends (menu, or `-SovSteam -SovHost`): a friends-only lobby opens under your Steam name; `F` in the lobby opens the overlay to invite friends. A friend running Sovereign chooses "Join a Steam friend" (or `-SovSteam`, or `-SovSteamLobby=<id>`) and accepts the invite in the overlay (Shift+Tab); the game then runs over Steam's networking with the lobby's owner as host. Development uses App ID 480 (set through the `SteamAppId` variable; nothing is written beside the engine). The Steam client must be running and signed in |
| Online | Host with `-SovHost` (`-SovHumans=N` human seats, default 2; `-SovPort=`, default 7777; `-SovAutoStart=N` starts once N players have joined), join with `-SovJoin=<address>` (and `-SovPort=`); both take `-SovName=`. The lobby lists the seats; the host presses `Enter` (or `Space`) to start, and open seats go to the AI. In the game each machine plays its own seat with its own fog of war; the host plays the AI seats and city-states. `M` opens the chat line. A machine whose game drifts is resent the host's game automatically; a player who drops can rejoin their seat, which the AI plays meanwhile. Same rules data on every machine (the host refuses others) |
| Online battles | A melee involving a human's leader stack is fought live on that player's machine (as offline). When the other side is another human, they see "BATTLE! ... fights your army live": `B` opens the battle on their machine, showing the field ten times a second as the battle's host runs it, and their squads take their orders (`Tab`, `1`-`6`, `7` `8` `9` `0`) instead of the trained AI's; `Esc` hands their men back to the AI; `R` settles it by the numbers. Only the host's one `BattleResult` command enters the game, clamped to the band. A battle left waiting five minutes is settled by the numbers. `-SovBattleDemo` also works when hosting |
| Hot seat | `-SovHotSeat=N`: the first N seats are human on this machine. When the turn passes to another human the screen goes dark until they press `Enter`, so nobody sees another player's fog of war |
| `,` | World Congress (once a civ reaches the Medieval era, every 30 turns): vote on each resolution in session, option A or B and a target, with a free vote and more bought with Diplomatic Favor ("Buy another vote" first). The HUD shows favor and its rate, Diplomatic Victory points, when the Congress meets and the resolutions in force; the news lines announce sessions and what passed |
| `Z` | Governors: appoint one with a title (titles come from civics such as State Workforce and Early Empire), promote one along its tree, or, with a city selected, send one there (5 turns to establish, Victor 3; an established governor gives +8 loyalty and its promotions work in that city). Amani can serve in a city-state you have met, where she counts as 2 envoys. The HUD lists your governors and titles |
| `Y` | Great people: each class's current individual with your points, the cost and points per turn; pick one to buy it now with gold (or faith), pass on one, or move a Great Work to another of your slots (for theming). A selected great person: `F` uses it where it stands (on its district, or a Great Work in a city with a free slot) |
| `P` / `T` / `C` / `F2` | Production / research / civics / government, policy cards and dedications chooser; `1`-`9` picks, `0` next page, `Esc` closes |
| `Esc` | Closes a chooser, then clears the selection, then opens the menu over the game: resume, save the game (local games, saved as `turn N`), continue a save, start a new game, quit. After a victory or your elimination this is the way out |
| `.` | Next unit that needs orders |
| `Space` / `Enter` | End turn. If the core refuses, the HUD shows why and opens what is needed |
| `WASD` / arrows, wheel, `Home` | Pan, zoom, back to your capital |
| `F1` / `F3` | How to play (the main keys) / yields on your territory's plots, `*` on the ones worked |
| `F7` | Step through the map lenses (religion, loyalty, appeal, settler, trade, none) |
| `F8` | The Empire panel |
| `F4` / `F6` | The chronicle of your reign so far (wars, assassinations, rulers captured or slain, successions, rebellions, historic moments, new ages) / have the court historian write it up: the local model (as for talks) or, without one, the scripted chronicle, saved to `Saved/Sovereign/Chronicles/`. When the game ends for you the chronicle is written and the reign enters the Hall of Sovereigns (`Saved/Sovereign/Hall.txt`, shown from the menu) |
| `F5` / `F9` | Quicksave / quickload (local games). Saves live in `Saved/Sovereign/*.sov`; the main menu offers to continue the four latest (the autosave a live battle writes among them) |
| Map | Territory borders in the owner's colour, rivers in blue along plot edges, resources you can see as small balls (green bonus, violet luxury, red strategic), improvements as a flat tile (dark red when pillaged). Resting the cursor on a plot shows its terrain, resource, improvement, district, owner and what it yields its city. The map wraps east-west: panning past one edge carries on into the other, drawn as a copy on each side |

## Tests

Headless automation tests (no window):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/source/games/Sovereign/unreal/Sovereign.uproject" -ExecCmds="Automation RunTests Sovereign; Quit" -nullrhi -unattended -nosplash
```

- `Sovereign.Bridge.HexLayoutRoundTrip`: world positions and picking match `sov::HexGrid`.
- `Sovereign.Bridge.MirrorFollowsCore`: after 20 all-AI turns, the mirror's tiles, units and cities equal what seat 0 knows.
- `Sovereign.Bridge.LeaderInMirrorAndCommands`: seat 0's leader appears as a leader marker with its ruler's name, and the escort link goes through as a command.
- `Sovereign.Street.CityCenterFromGameState`: the generated City Center has a landmark per building (the Palace included), houses and crowd by population, six streets, no walls without wall buildings, and the same layout for the same hex.
- `Sovereign.Battle.NumbersDecideMostFights`: the battle simulation hurts both sides in even fights, lets a much stronger side win and lose less in at least 10 of 12 seeds, repeats itself with no input, and handles an unescorted leader.
- `Sovereign.Battle.TrainedCommanderLeads`: the trained battle AI loads from `data/battle_ai/commander.txt`, leads the enemy, the human orders one squad without touching the others, and the battle ends within HP bounds.
- `Sovereign.Bridge.DiplomacyTalkFallsBackToScript`: with no model server, a talk with an AI leader gets a scripted reply off the game thread and ends with a `RecordTalk` summary command.
- `Sovereign.Bridge.HostAndJoinStayInLockstep`: a hosting session and a joining one (engine sockets on localhost) claim seats, start, play 8 turns each with its own seat, and end with the same state hash.
- `Sovereign.Bridge.OnlineLiveBattleSettlesEverywhere`: a hosted battle demo; the host attacks the guest's warrior with its leader's escort, both machines see the battle wait, field and orders cross between them, and the host's one result settles it on both with the same state hash.
- `Sovereign.Battle.OnlineSnapshotsAndRemoteOrders`: the remote side's order holds against the AI, a snapshot round-trips and draws the same field (positions within a hundredth), a truncated one is refused.
- `Sovereign.Bridge.HotSeatHandsOver`: with two human seats, the end of the first player's turn hides the screen until the second takes over, and the view then follows the second seat.
- `Sovereign.Bridge.HumanSeatPlaysThroughCommands`: a scripted seat 0 founds a city and plays 10 turns through commands with AI opponents; the log replays to the same state hash.
- `Sovereign.Bridge.HumanSeatPlaysLongGame`: a newcomer at seat 0 (the first fitting choice the controller offers, settlers to good sites, builders to empty plots, traders on routes, a pantheon, governments, cards, envoys and governors) plays two games to turn 250, answering every End Turn refusal through the chooser the controller opens, saving and resuming every 50 turns; any refusal with no way out fails it.
- `Sovereign.Bridge.ChronicleWrittenWithoutAModel`: with no model server, the chronicle writer saves the scripted chronicle off the game thread and hands the HUD a note.
- `Sovereign.Bridge.ModsLayerOverTheRules`: the example mod is found, a game with it has its rules, its save loads it again, and a missing mod is refused by name.
- `Sovereign.Bridge.WeeklyChallengeIsTheWeeksGame`: this week's challenge starts from the week's setup whatever the options and mods; its save is still the challenge and passes the board's check.
- `Sovereign.Battle.RecordedBattleReplays`: a battle recorded ten times a second round-trips its file, and playing its frames through the remote view ends on the field the battle ended on (the recording is kept in `Saved/Automation/replay-test.sovbattle` to watch).
- `Sovereign.Bridge.SaveAndLoadResume`: an all-AI game saved after a few turns resumes in a fresh session with the same state hash and plays on; a truncated save is refused and the game in hand stays.

GitHub CI has no Unreal; it builds the core standalone and checks the wrapper list.

## Not yet

- Sound: there is none yet (music, ambience and effects wait on a direction).
- City centres follow the six architectural styles only until the Industrial era (then every civ shares the Industrial and Modern kits); districts, improvements and units are one look for every civ; each wonder and natural wonder is drawn by kind rather than as itself.
- Live battles draw each side's men as its unit's figure only on foot (spearman, archer, musketeer, rifleman); horsemen, engines and vehicles fight as spearmen there.
