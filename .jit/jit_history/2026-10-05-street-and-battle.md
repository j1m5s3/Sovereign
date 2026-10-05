# One street scene and one live battle, build-plan steps 4 and 5

status: ARCHIVED · slice: street-and-battle · base: 7d550dc · created: 2026-10-05 · updated: 2026-10-05

James (2026-10-05): "go ahead on step 4 and 5". Step 4 of `specs/sovereign/engine-and-architecture.md`: one street scene (one style, the City Center) with the Benevolence/Fear citizen actions and the loyalty system they need. Step 5: one medieval live battle. Rules go in the core as commands; scenes run in Unreal and hand results back as commands (engine doc, "Live scenes report back"; "Non-deterministic parts are inputs"). Each milestone is one PR from `claude/project-thread-zkdmmz`; James merges. Out of scope: other districts and styles, crowds with real art, direct-control assassin fights, battles in other eras, multiplayer hosting of scenes, the ML battle AI (a scripted battle AI stands in).

## Decisions (do not reopen)
- **Loyalty (02-cities.md, Loyalty [R&F]):** every city has loyalty 0-100 (`LOYALTY_START`). At the start of its owner's turn it changes by:
  - citizen pressure: cities within `CITIZEN_IDENTITY_PRESSURE_RADIUS_CUTOFF` add population × (10 − distance), domestic vs foreign, with capitals counted twice; net = 10 × (domestic − foreign) / (min + 0.5), capped at ±20. Age factors stay 1.0 until eras exist;
  - the amenity level's loyalty (generated `happinessLevels[].loyaltyPerTurn`);
  - starvation (−4);
  - `ADJUST_CITY_LOYALTY` modifiers (Monument +1, hand-written in `modifiers.json`).
  
  Loyalty levels (generated `loyaltyLevels`) scale the city's yields and growth. A captured city starts at `LOYALTY_AFTER_TRANSFERRED_BY_COMBAT`.
- **Free Cities:** a city at 0 loyalty revolts to the Free Cities player. Like the barbarian player, it is created with every game, is at war with every major, takes no turns, and its cities neither grow nor build. Its cities gain `IDENTITY_PER_TURN_FROM_FREE_CITIES` plus pressure in the world turn, and at 0 they join the civ exerting the most pressure at full loyalty. Units in a revolting city go with it.
- **Citizen stances (leader doc §4, decided; details as proposed):** `CityStance` (id = city, arg = Benevolence/Fear). The leader must stand in the city, with a cooldown per city (`STANCE_COOLDOWN_TURNS` 10).
  - Benevolence costs gold (`STANCE_BENEVOLENCE_GOLD_PER_POP` × population) and gives +2 amenities for 10 turns.
  - Fear needs an own military unit in the city. It gives +20 loyalty at once and keeps the city from counting as Unrest/Revolt for 10 turns; afterwards −1 amenity for 20 turns, and assassins gain `STANCE_FEAR_ASSASSIN_BONUS`% success against a leader in that city.
  
  All numbers live in `leader.json`.
- **Reputation (§8.1, decided):** `Player.reputation` runs −100..100. Benevolence adds and Fear subtracts `REPUTATION_PER_STANCE`; razing a city subtracts `REPUTATION_PER_RAZE`.
  - Beloved (≥ `REPUTATION_THRESHOLD`): +1 amenity in every city, +2 loyalty per turn.
  - Feared (≤ −threshold): −1 amenity in every city, loyalty losses from citizen pressure halved, +10% assassin success against its leader.
- **Rebellion (§8.2, decided):** a city in Unrest or worse (loyalty level) whose owner is Feared and whose Fear effect has ended rolls `REBELLION_PERCENT` each turn (Gameplay stream). On a hit, rebel units (the barbarian player's strongest melee unit the owner can build, `REBELLION_UNITS` of them) appear next to it.
- **Street scene (world doc, Generator stages; one style, City Center):**
  - Unreal builds the City Center from game state with a per-hex seed: plaza, main streets to the hex edges, one landmark block per building the city has (named), filler houses by population, a wall ring if it has walls, a crowd sized by population, and mood props by amenity level and Fear rule.
  - The leader walks there in third person (WASD, mouse look). Citizens wander. Walking up to the herald (Benevolence) or the captain of the guard (Fear) and pressing `F` sends `CityStance`. `Esc` returns to the map. Classic control gets the same actions in the city panel (`V` Benevolence, `X` Fear).
  - Entering a scene autosaves the game (`Saved/Sovereign/autosave.sov`, the core save format).
  - Primitives only (no art assets yet): one procedural "temperate" kit made of boxes and cylinders, coloured by the civ's palette.
- **Live battle contract (§9, decided):**
  - **What goes live:** melee where the attacker or the defender is a human's leader stack (the leader, or an escort on the leader's plot).
  - **Attacker's turn:** the attacker submits `Attack` as usual. If it goes live, the core creates `GameState.pendingBattle` and no damage is dealt yet.
  - **AI attacks:** an AI's attack on a human leader stack also creates the pending battle and pauses the AI (`ai::playTurn` returns early while one is pending; the subsystem resumes it after the result).
  - **Expected result:** the core computes it with the normal formula at the middle roll.
  - **Settling it:** `BattleResult` (either side's human; allowed out of turn) carries the field result (damage to the attacker, damage to the defender, leader wound). The core clamps each value to expected × (1 ± `LIVE_BATTLE_BAND_PERCENT`/100) (leader wound ≤ `LIVE_BATTLE_LEADER_MAX_WOUND`). `AutoResolveBattle` applies the expected result with the normal random roll.
  - **After the result:** the usual post-combat rules run (kills, capture, advance, XP, escort rules).
  - **Blocked meanwhile:** while a battle is pending, every other command is refused (`BattlePending`).
- **Battle scene (step 5):**
  - Unreal builds a battlefield patch from the two plots and their neighbours: terrain colour, hills raised, woods as tree clusters.
  - Each unit fields soldiers in proportion to its HP (`HP / 10`, at least 1). The human plays the leader in third person (move, click to strike, `Tab` to order its side to charge or hold), and a scripted battle AI moves everyone else.
  - Soldier stats come from the unit's Civ strength. Hit chance and damage follow the strength difference (the same e^(0.04 × diff) shape), so numbers decide most fights.
  - **Ending it:** the battle ends when a side is wiped out or routs (below 25% strength), or at the real-time cap (`LIVE_BATTLE_SECONDS`, 180), which settles it from the current state. The result is the share of each side's soldiers lost, converted to HP damage.
  - The player may auto-resolve before the fight starts. The battle simulation is a plain C++ class (`FSovBattleSim`) the automation tests can run without rendering.

## Pointers
- [specs/civ6/02-cities.md : L54-L77] — loyalty rules
- [specs/civ6/data/eras-moments-loyalty.md : L21-L41] — happiness and loyalty level tables (generated into rules)
- [specs/sovereign/leader-character-brainstorm.md : §4, §8.1-8.2, §9] — stances, reputation, rebellion, live battles
- [specs/sovereign/world-scale-and-generation.md : Stages] — how a City Center is generated
- [core/src/city.cpp], [core/src/combat.cpp], [core/src/leader.cpp], [core/src/ai.cpp] — where loyalty, stances and the battle contract plug in
- [unreal/Source/SovereignBridge/Private/] — scenes, controller, HUD

## Micro-steps
<!-- [ ] pending · [>] active · [x] done · [-] dropped (reason) · [!] blocked -->
1. [x] **Build** loyalty and Free Cities (core): generated happiness loyalty and loyalty levels, `City.loyalty`, pressure, per-turn change, yield and growth scaling, revolt to Free Cities and flipping back, `ADJUST_CITY_LOYALTY`, save v12, sovsim report, tests; Unreal HUD shows loyalty. PR "Street milestone 1". — done 2026-10-05: 6 new tests (133 total); a revolted city starts the Free Cities at 50 loyalty (Civ value not in our spec); Free Cities are created on the first revolt (barbarian flag + freeCity), so earlier games keep their player list; 6-player AI games see 0-1 Free Cities by turn 250.
2. [x] **Build** stances, reputation and rebellion (core): `CityStance`, timed city effects, reputation effects, Fear assassin bonus, rebellion, AI use (Fear in Unrest cities, Benevolence when rich), tests. PR "Street milestone 2". — done 2026-10-05: 7 new tests (140 total); fixed a stall when a player's last city revolts as its turn begins (the turn now moves on); Unreal city panel `V`/`X` with cooldown and effect timers, reputation line, rebellion news. Save v13.
3. [x] **Build** the City Center street scene (Unreal): generator, third-person leader, citizens, herald and captain interactions, mood, autosave, `V`/`X` panel actions, automation test of the generator. PR "Street milestone 3". — done 2026-10-05: 5/5 Unreal tests; live run walked from the street onto London's plaza past the herald and captain. Entering is `Q` with the leader selected. Scenes sit 10 km from the map at ground level (below the map the sky atmosphere goes dark). Ambient light now comes from the engine daylight cubemap (real-time sky capture gave none), which also lights the map's tile sides.
4. [x] **Build** the live battle contract (core): pending battle, expected result, `BattleResult` with band, `AutoResolveBattle`, `BattlePending` refusals, AI pause and resume, tests. PR "Battle milestone 1". — done 2026-10-05: 6 new tests (146 total); city assaults holding a leader stay Civ math for now (only unit-vs-unit melee goes live); Unreal turns live battles on for human games, pauses AI seats, offers `R` auto-resolve (`B` live arrives with milestone 2). Save v14.
5. [x] **Build** the medieval battle scene (Unreal): battlefield, soldiers, leader control, scripted battle AI, rout and time cap, result command, auto-resolve choice, autosave, automation test of `FSovBattleSim`. PR "Battle milestone 2". — done 2026-10-05: 6/6 Unreal tests; live run with `-SovBattleDemo`: warrior attack on the enemy went pending, `B` opened the battle on a wooded field, men fought and fell, the field result (attacker lost 80) was clamped by the core to 43 (expected 34 + 25%) and the game returned to the map. Blows slowed so a fight lasts about a minute. Soldier strength uses each unit's Civ strength in that fight; the escorted leader is a hero worth three soldiers.
6. [x] **Close** slice: docs, JIT index, review against Acceptance, archive plan. — done 2026-10-05.

## Acceptance
- Loyalty moves cities as Civ VI does with the systems that exist; cities revolt to Free Cities and flip back; data drives every number.
- The leader can take Benevolence or Fear stances (street scene or panel); reputation and rebellion follow; AI uses stances.
- The leader can enter its City Center as a generated street scene built from that city's state and walk it; leaving returns to the map; the game autosaves on entry.
- A melee fight involving a human leader stack can be fought as a live medieval battle or auto-resolved; the result reaches the core as one command and is clamped to the band; AI attacks pause for it.
- Core CI green on GCC, Clang and MSVC; replays hold; Unreal builds and its automation tests pass.

## Review against Acceptance (2026-10-05)
- Loyalty follows the Civ formula with the systems that exist; revolts to the Free Cities and flips back are tested; every number comes from generated or hand-written data.
- Stances work from the street scene and the city panel; reputation and rebellion follow; the AI takes Fear and Benevolence.
- The leader walks a City Center generated from that city's state (live run on James's PC); leaving returns to the map; entry autosaves.
- A melee with a human leader stack can be fought live or auto-resolved; one BattleResult reaches the core and is clamped (live run: 80 claimed, 43 applied); AI attacks pause for it.
- Core CI green on each PR (GCC, Clang, MSVC); replays hold with battles settled by commands; Unreal builds and its 6 automation tests pass.

## Follow-ups
- City assaults holding the leader should go live too (§9); only unit-vs-unit melee does now.
- Battle participants are the attacker, the defender and the human's leader; units within one plot (§9 "who joins") and terrain cover come next.
- Free City units do not act; Free Cities do not count for victories.
- Street scenes: one kit of primitives; other districts, era bands and styles, crowd behaviour and interiors follow with the art pipeline.
- The trained battle AI (§10 layer 3) replaces the scripted one; multiplayer scene hosting waits for online play.

## Open questions & risks
- Primitives stand in for art; the look is a placeholder until the art pipeline (leaders-and-art-style.md, Art toolchain).
- Free City units do not act yet (they defend in place).
- The battle AI is scripted, not trained (§10 layer 3 later).
- Multiplayer hosting of live scenes waits for online play.

## Changelog
- 2026-10-05 CLOSE — JIT index, READMEs, engine doc build plan updated; reviewed against Acceptance; archived.
- 2026-10-05 STEP 5 DONE — battle scene: FSovBattleSim, ASovBattleScene, leader control, rout and time cap, result command, `-SovBattleDemo`.
- 2026-10-05 STEP 4 DONE — battle contract: PendingBattle, BattleResult band clamp, AutoResolveBattle, BattlePending refusals, AI pause; resolveUnitFight split out of applyCombat.
- 2026-10-05 STEP 3 DONE — street scene: generator, walker, citizens, herald/captain stances, autosave; `SaveGame` added to the subsystem.
- 2026-10-05 STEP 2 DONE — stances, reputation, rebellion; AI uses Fear below 60 loyalty and Benevolence when short of amenities and rich.
- 2026-10-05 STEP 1 DONE — loyalty, levels, Free Cities; Monument +1 via ADJUST_CITY_LOYALTY; gen_rules emits loyaltyPerTurn and loyaltyLevels.
- 2026-10-05 CREATED — 6 steps for build-plan steps 4 and 5; step 1 started.
