# The leader in classic control, build-plan step 3

status: ACTIVE · slice: leader-classic · base: a6d1eef · created: 2026-10-05 · updated: 2026-10-05

Build-plan step 3 of `specs/sovereign/engine-and-architecture.md`: each player's leader (the Sovereign) as a map unit in classic control, with gear, escorts, capture, succession and auto-resolved assassins, in the rules core first and then in the Unreal bridge. Source: `specs/sovereign/leader-character-brainstorm.md` (§1, §2, §5, §6, §8.8, §12) and the dynasty table in `specs/sovereign/leaders-and-art-style.md`. Only **[decided]** rules are built; proposals wait for James (Open questions). Each milestone is one PR from `claude/project-thread-zkdmmz`; James merges. Out of scope: direct control, street scenes and Benevolence/Fear (step 4), live battles (step 5), loyalty, era score, governors, Great Generals, deals.

## Decisions (do not reopen)
- Data: new hand-written `data/rules/leader.json` (loaded after `civilizations.json`). It adds the leader unit to the `units` table (`UNIT_SOVEREIGN`, class `LEADER`, layer `LEADER`, cost 0, not trainable, no maintenance, combat from gear), the `gear` table, the `dynasties` table and `LEADER_*` globals. Everything numeric is data.
- Layer: the leader has its own 1UPT layer (`UnitLayer::Leader`). It shares a plot with one military unit (the escort) and one civilian, so it can start on the Settler's tile (§1, decided). The doc says "civilian layer like a Great Person"; this separate layer is the smallest change that keeps that decision. Noted as a deviation.
- Start: every major civ's leader spawns on its start plot with the normal starting units (§1).
- Gear (§2, §8.8): three slots, weapon, armor and mount. Each item has a tech unlock (none = start), melee `combat`, `ranged`/`range` (weapons), `defense` (armor, added only when defending), `moves` (mounts add, armor may subtract), a one-time strategic cost, `goldCost` (scaled by game speed) and, for mounts, `upkeepAs` (a unit whose gold and GS resource maintenance the leader pays twice). `EquipGear` (id = leader, arg = gear, or -1 with arg2 = slot to take a mount off): only in one of your cities, needs moves left, ends the leader's turn, pays gold and the strategic cost. The ladder follows the doc's example table and sets a fully geared leader near the era's best melee unit (weapon + armor ≈ that unit's strength). No skins in the core (cosmetic).
- Leader combat: base melee = weapon combat; defending adds armor defense; ranged/range come from a ranged weapon. Every other modifier (terrain, fortify, wounds, flanking/support) applies as for units. The leader may attack (duels §8.5: classic control resolves them as normal combat). It exerts no ZOC and gains no XP until levelling is decided.
- Escort (§1, §5): attacks on a plot hit its military unit first. The leader defends only when no military unit is there. The attacker does not advance onto a plot that still holds an enemy leader. An enemy melee win over an unescorted leader **captures** it; a ranged kill **kills** it; a leader killed or captured in a city capture follows the same rules (captured). **Barbarians** can wound the leader but never kill or capture it: HP stops at 1, and at or below `LEADER_RETREAT_HP` it moves to its capital (§1, decided).
- Escort link: `LinkEscort` (id = military unit, arg = leader id or -1) on the same plot. A linked pair moves together on the leader's order at the slower unit's pace; a move order to the escort moves the pair; the link ends when they stop sharing a plot. A linked escort does not need orders.
- Succession (§5): a dead or abandoned leader starts an **interregnum** of `LEADER_INTERREGNUM_TURNS`: policies are emptied and cannot be set; free changes open when it ends. The player must `ChooseSuccessor` before ending a turn (`LeaderNeeded`): arg 0 = the next heir of the civ's dynasty (2 heirs), arg 1 = a level-4+ military unit (id; the unit becomes the leader), arg 2 = a regent (Sovereign stand-in, only when no heir and no eligible unit exist, until governors and Great Generals arrive). The successor appears in the capital (or where the unit stood), at full HP, with the fallen leader's loadout. The civ's leader ability is unaffected (no leader abilities exist yet). Loyalty and era-score losses wait for those systems.
- Capture (§5): the leader leaves the map and the player is in interregnum while it is held (`Player.captor`). Ransom needs the deal screen (later). `AbandonLeader` crowns a successor (succession as above). Loyalty cost waits.
- Regicide (§5, §12): `GameSetup.regicide`; losing the leader (killed or captured) eliminates the player. Its cities pass to whoever killed or captured the leader. Regicide is a GameSetup switch, like the other victory switches.
- Saves: `kSaveVersion` 9. Units gain `gear[3]` and `escorting`; players gain `leaderName`, `dynastyNext`, `successionPending`, `interregnumTurns`, `captor`, `savedGear[3]`; GameSetup gains `regicide`.
- AI (step 3 minimum): keep the leader in the capital, asleep and guarded by the garrison. Equip the best affordable weapon and armor while in a city. Choose the heir, else the highest-level eligible unit, else a regent. Attack enemy leaders by preview like any target. The random bot chooses successors too.
- Bridge: a distinct leader marker (gold-trimmed cylinder in the owner's colour); `E` gear chooser when the leader is in a city; `L` link/unlink the escort; a succession chooser opened by a `LeaderNeeded` refusal; HUD lines for the leader's name, gear, captivity and interregnum.

## Pointers
- [specs/sovereign/leader-character-brainstorm.md : L24-L58] — map presence, start, gear ladder
- [specs/sovereign/leader-character-brainstorm.md : L90-L120] — succession, capture, regicide, assassins
- [specs/sovereign/leaders-and-art-style.md : L128-L145] — dynasty table (heirs)
- [core/src/combat.cpp] — defender choice, capture, city capture, healing
- [core/src/game.cpp] — create (starting units), movement (advanceUnit), turn begin
- [core/src/rules.cpp : L230-L237, L482-L533] — file list, unit parsing
- [core/src/ai.cpp], [core/tools/random_bot.h] — players that must handle the leader
- [unreal/Source/SovereignBridge/Private/SovPlayerController.cpp] — input and choosers

## Micro-steps
<!-- [ ] pending · [>] active · [x] done · [-] dropped (reason) · [!] blocked -->
1. [x] **Build** leader unit, gear and escort combat (core): `leader.json`, `UnitLayer::Leader`, start spawn, gear and `EquipGear`, mount upkeep, leader strength, escort-first defence, capture/kill/barbarian rules, city capture, `LinkEscort` movement, AI/bot handling, save v9, tests. PR "Leader milestone 1". — done 2026-10-05: 11 new tests (113 total, local MSVC build); AI equips Spear/Bronze scale/Plate in a 150-turn all-AI game; bot and AI games replay to the same hash; Unreal still links.
2. [>] **Build** succession, captivity and regicide (core): interregnum, `ChooseSuccessor`, `AbandonLeader`, `LeaderNeeded`, dynasties, regicide elimination, AI/bot, tests. PR "Leader milestone 2".
3. [!] **Build** assassins (core): blocked on James's answers to Open questions 1-2. PR "Leader milestone 3".
4. [ ] **Build** leader UI (bridge): marker, gear and succession choosers, escort link, HUD, automation test. PR "Leader milestone 4".
5. [ ] **Close** slice: docs (`core/README.md`, `unreal/README.md`, JIT index, leader doc status), review against Acceptance, archive plan.

## Acceptance
- Every major civ starts with its leader; the leader fights with tech-gated gear, is protected by its escort, is captured or killed as decided, and barbarians never kill or capture it.
- Losing the leader triggers interregnum and a successor choice (heir, level-4+ unit, regent); Regicide eliminates.
- Assassins (once specified) are sent by the AI and resolved automatically from rules data.
- All of it is commands through `Game::submit`; replays and saves reproduce the state hash; CI green on GCC, Clang and MSVC; the Unreal build links and its automation tests pass; the seat-0 player can equip, escort and choose a successor in the UI.

## Open questions & risks (asked 2026-10-05; recommendations in brackets)
1. Assassins: own unit with own capacity, or a spy mission? [own unit, Encampment city, Political Philosophy, capacity 1 per Encampment]
2. Assassin odds (Civ spy odds unverified). [Sovereign model on the core's combat math: power from level and the sender's era vs gear plus guards on or next to the plot; outcomes killed / wounded / assassin killed (leader XP) / assassin captured (sender revealed); numbers in data]
3. Levelling (§3) and the presence aura (§1) are proposals. [build the SOVEREIGN tree and a 2-tile aura now with effects the core can model]
4. Benevolence/Fear panel (§4) needs loyalty. [step 4, with loyalty]
- Captured leaders can only be abandoned until the deal screen exists (ransom).
- Duels between leaders give no special war score yet (no war score system).

## Changelog
- 2026-10-05 STEP 1 DONE — leader unit, gear, escorts and capture in the core; `sovsim --cities` prints each leader's gear. Until step 2 lands, a lost leader is simply gone (no successor yet).
- 2026-10-05 CREATED — 5 steps from build-plan step 3; four open questions sent to James; step 1 started.
