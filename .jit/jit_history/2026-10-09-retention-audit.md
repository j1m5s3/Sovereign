# Record: player-retention rules the audit found off (part 1)

Status: done, 2026-10-09. An audit of `specs/sovereign/world-scale-and-generation.md` and `specs/sovereign/player-retention.md` against the core found 8 gaps. This part fixes the six in player retention; the two in world scale are listed below for later. Chosen by Claude under James's standing consent.

## Change

- **Rival memory counts assassinated rulers** (player-retention §1: "captured or assassinated leaders"). Before, only a ruler taken in battle (`LeaderLost`) counted; one killed by an assassin (`AssassinKilledLeader`) now counts the same way, as a trophy for the sender and a loss for the victim.
- **Rival memory counts broken promises** (§1: "broken promises"). `RivalMemory::promisesBroken`: each promise the human made the AI civ and broke. It weighs 4 in `Game::rivalGrudge` (Sovereign tuning, between a war's 2 and a surprise war's 5). The rivals text file carries it as `promisesBroken`; older files load with 0. Save version 95.
- **Alliances count toward respect** (§1: "long alliances carry over as a friendlier opening stance"). `friendTurns` now also counts turns allied; before, an alliance renewed past its friendship stopped counting.
- **The chronicle has ransoms and failed assassins** (§2: "assassination attempts, captures and ransoms"). `Game::ransomRuler` pushes `EventKind::RulerRansomed` (the captor, the ruler's civ); it and `AssassinKilled` (an assassin dying in the attempt) are now chronicle-worthy with their own lines.
- **The Hall of Sovereigns says how the ruler fell** (§2: "victory or fall, cause of death"). With no ruler on the throne and none held captive, the entry names the last fall: in battle against a civ, or killed by an assassin from one.
- **Short Reign scales the Science victory** (§4: "a 100-turn game with scaled costs and victory conditions"). Before, its expedition still needed Standard's 50 light-years, about half the game at one a turn. `GameSpeedType::scienceVictoryPercent` (20 for Short Reign, 100 otherwise) scales it to 10 (`Game::lightYearsRequired`). Other speeds keep Civ VI's 50.

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against #291): every game identical; all-AI games have no human for rival memory, and none ransomed a ruler or ran Short Reign.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; 4 of 8 end states differ from #291 (the new chronicle events). Time per game unchanged (two alternating pairs).
- Tests: `rivals_remember_assassinations_broken_promises_and_alliances`, `the_chronicle_and_hall_tell_how_a_ruler_fell`, `a_short_reign_expedition_arrives_sooner`, and a ransom line check in `a_captured_ruler_is_ransomed_home`. Mutation: 14 mutants, 13 caught (two after the tests were tightened: a carried memory's broken promises, a capture that is no death). Missed: dropping the check that the promise was the human's; in these two-civ tests a promise to the AI can only come from the human.

## Not done yet (the audit's other findings)

- **A mount changes map moves** (world-scale: "a mount speeds up walking inside a hex but does not change map moves"). Here a mount adds its `moves` (2) to the leader's map moves (`Game::maxMoves`, `data/rules/leader.json`). The leader doc (§8) gives mounts only their cost, so the world doc's rule stands; changing it takes the mount's only map effect away, which also changes what the AI buys. Left for a part of its own.
- **Fogged plots show live state** (world-scale: "revealed hexes use their last-seen state"). The core keeps only each player's visibility, no last-seen record, so the Unreal map draws cities, roads and improvements built in the fog. Needs a per-player last-seen record in the core and the map reading it.
- The Unreal status line still shows the unscaled light-years (`SovStatus.cpp` reads the global); `Game::lightYearsRequired` is there for it.
