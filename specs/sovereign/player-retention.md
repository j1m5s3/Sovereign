# Sovereign: keeping players coming back

Status: decided feature list (James, 2026-10-04), details proposed. Companion to [leader-character-brainstorm.md](leader-character-brainstorm.md) and [world-scale-and-generation.md](world-scale-and-generation.md).

Principle: build on Civ's own "one more turn" hook and on what only Sovereign has (the playable leader, rivals that learn, a generated world). Retention through good play, never through manipulation.

## 1. Rivals who remember you (nemesis system)

Builds on the AI player-modeling layer (leader doc, section 10).

- Each AI leader keeps a **relationship history with each human player** that persists across games: wars won and lost, betrayals, captured or assassinated leaders, broken promises, duels.
- That history shapes the next game:
  - **Grudges:** a leader you defeated starts hostile toward you, more likely to target you, and says so.
  - **Trophies and taunts:** a leader who once captured or killed your leader remembers it in diplomacy (via the diplomacy language model).
  - **Adaptation:** a leader beaten several times by the same tactic counters it sooner (more anti-cavalry, more assassins against a roaming leader).
  - **Respect:** long alliances carry over as a friendlier opening stance.
- Persistence stays inside Civ rules: history only sets starting opinions, agenda weights and AI tactics, never free units or yields.
- Players can reset a rival's memory, or turn the system off, in game setup.

## 2. Your reign becomes a story

- **Chronicle:** every game records key events (wars, wonders, assassination attempts, captures and ransoms, duels, rebellions, successions). At the end, and on demand, the local language model writes it up as a chronicle of the reign in a historical voice. Exportable as text or image.
- **Hall of Sovereigns:** a persistent gallery of past reigns: leader, loadout and skins, level and promotions, victory or fall, cause of death, rivals made.
- **Battle replays:** the world and battles are generated from game state and a seed, so replays can be stored as compact event logs and replayed or shared.

## 3. Weekly challenge map

- Every week, everyone gets the same map seed, civ, leader and rule set.
- Leaderboards for score, turns to victory, and challenge-specific goals (e.g. "win without your leader leaving the capital").
- Cheap to run: the map and world are fully generated from the seed. Needs a light anti-cheat (submit the game's event log, validate it on the server by replay).
- **[decided, James 2026-10-04, gap review]** A server cannot re-run a live battle, so live-battle results in a submitted log are accepted only when they fall inside the battle result band (leader doc section 9); anything outside it fails validation.

## 4. A short-term goal for the leader every turn

Civ hooks players by overlapping goals that finish on different turns (tech, wonder, city growth). The leader adds a personal layer:
- A promotion close by.
- An assassin reported near the leader.
- A city near revolt that needs a visit.
- A rival leader nearby for a possible duel.
The UI should surface the next one or two of these at turn end, next to Civ's usual "X turns to Y".

## 5. Shorter game modes

- **Short Reign:** a 100-turn game with scaled costs and victory conditions.
- **Era starts:** begin in a later era with a developed empire (Civ VI already supports this; see specs/civ6/data/game-setup.md, "Advanced start eras").
- Online play: battle zone lock (leader doc, section 9) already keeps uninvolved players moving.

## 6. Mod support

- The game's rules already live in data tables (the specs/civ6/data model), so expose them to modders: civs, leaders, units, gear, buildings, model sets, AI personalities.
- Workshop-style sharing from day one.
- **[decided, James 2026-10-04, gap review]** **Launch scope:** data and modifier mods (new rules, balance, civs that reuse existing art). Asset mods (new models) come later through Unreal's plugin packaging. A scripting language (Lua is the common choice) is added only if scenarios need it.
- Mods respect multiplayer by matching mod lists on join.

## 7. Cosmetic unlocks through achievements

- Achievements unlock skins for weapons, armor, mounts and leaders (loadout skins, leader doc section 2), plus banners and city styles.
- Unlocks are cosmetic only.

## 8. Online services **[decided, James 2026-10-04, gap review]**

One service layer for accounts, leaderboards, the workshop, matchmaking and optional cloud sync: Steamworks if Steam is the only store, or Epic Online Services (free, cross-store, works with Steam). Which one is still to pick. Nemesis memory and the Hall of Sovereigns are stored locally, with optional cloud sync.

## What we will not do

- No loot boxes, energy timers or pay-to-win. Skins never change stats.
- No daily-login pressure mechanics.
