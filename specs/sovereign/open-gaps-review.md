# Sovereign: open gaps review

Status: review (2026-10-04). Lists what the design docs do not yet answer, by priority, with a recommendation for each. Nothing here is decided; James picks which to pursue. Reviewed: everything in `specs/sovereign/` plus the architecture, multiplayer and persistence parts of `specs/civ6/00-overview.md` and `specs/civ6/10-ai-ui-implementation.md`.

Priorities:
- **P1:** decide before the rules core is coded, because it shapes the core's interfaces or contradicts something already decided.
- **P2:** decide before the system it touches is built.
- **P3:** already known, or can wait.

## Summary

| # | Gap | Priority | Recommendation in one line |
|---|---|---|---|
| 1 | Who is authoritative in live battles and assassin fights, especially online | P1 | Live scenes run on one host and return one result command; the core clamps it |
| 2 | How a live battle maps to Civ combat and back | P1 | Core computes the Civ outcome first; the live scene only moves it inside the ±25% band |
| 3 | Non-deterministic parts (language model, battle AI, physics, live scenes) vs. lockstep, replays and weekly challenge | P1 | Treat every one of them as recorded input, never as computation |
| 4 | Which fights go live, how often, and whose turn they interrupt | P1 | Live only for melee involving the leader's stack; auto-resolve always offered |
| 5 | Battles after the Renaissance | P1 | Leader becomes a commander, not a duelist; decide now since it sets battle scope |
| 6 | Walkable world shows true state, leaking fog of war | P1 | Generate each hex from what the viewing player has seen, not the true state |
| 7 | Leader ability on succession: the two docs contradict each other | P1 | The ability belongs to the throne and passes to any successor |
| 8 | Game start and early-game leader safety | P1 | Leader starts beside the Settler; barbarians can wound but not kill or capture; no assassins before the Classical era |
| 9 | Core foundations not yet pinned (numbers, modifiers, commands, saves) | P1 | Fixed-point math, Civ's modifier system, one command log, versioned saves |
| 10 | Sovereign build plan and art pipeline | P1 | One roadmap: headless Civ core first, then the leader in classic control, then one street scene, then one medieval battle |
| 11 | Heirs and the 12 civ abilities/uniques | P2 | Known open items; heirs as a 3-person historical dynasty, then pool successors |
| 12 | Difficulty with a learning, non-cheating AI | P2 | Difficulty scales AI skill and planning depth first, small Civ-style bonuses only at the top |
| 13 | Leader at sea and in the air | P2 | Leader embarks like a land unit; naval fights with the leader auto-resolve |
| 14 | Crossing 1 km hexes on foot | P2 | Free travel to any owned district or city; walking only inside a hex |
| 15 | Modding scope (art and scripting) | P2 | Launch with data + modifier mods; art mods later via Unreal plugins |
| 16 | Language model content safety with real historical figures | P2 | Filter input and output, keep leaders in character, plan the age rating early |
| 17 | Saving during live scenes | P2 | Save at scene start; loading restarts the scene |
| 18 | Target platforms and controls | P2 | PC (Windows) first with mouse and controller; Steam Deck as a test target |
| 19 | Online services (accounts, leaderboards, workshop, cross-game memory) | P2 | Pick one service layer (Steamworks or Epic Online Services) |
| 20 | Unverified Civ rules and assassin open questions | P3 | Already tracked; spy odds block assassin tuning |

## P1: decide before coding the rules core

### 1. Authority in live scenes

Live battles and assassin fights run in Unreal in real time. Lockstep (the multiplayer model in 10 and engine doc) syncs turn commands, not real-time action, and Unreal's movement and hit detection are not deterministic across machines. Nobody is named as the judge of a live fight in multiplayer, or when an absent player's units are run by the battle AI.

**Recommendation:** a live scene is a short real-time session hosted by one machine (the attacker's, or a dedicated host if one exists), using Unreal's own networking for that scene only. When it ends, the host sends one result command (casualties, HP, wounds, captures, XP) into the lockstep stream, and every machine's core applies it. The core checks the result against the bounds in gap 2, so a tampered result cannot exceed what the rules allow.

### 2. The battle result contract

The doc says Civ math is the backbone and player skill shifts the outcome by about ±25%, but not how. Open: how a Civ unit (one abstract formation with 100 HP) becomes soldiers on the field, what "skill" is measured as, and how the field result becomes Civ HP.

**Recommendation:** before the scene starts, the core computes the normal Civ outcome (05 combat formula) as an expected result. Each unit's soldier count in the scene follows its HP. During the scene, casualties are tracked as each side's share of losses. At the end, the core blends: final result = Civ expectation shifted toward the field result, capped at the band. This makes the scene unable to overturn the numbers, and the same function auto-resolves when nobody plays.

### 3. Non-deterministic parts versus lockstep

The language model, the ONNX battle AI, Chaos physics and live scenes all give different results on different machines. That matters in three places the docs rely on determinism: multiplayer sync, replays stored as event logs, and the weekly challenge's server-side replay check (player-retention section 3). A server cannot re-run a live battle, so a submitted log could carry invented battle results.

**Recommendation:** one rule: anything non-deterministic happens on one machine and enters the core as a recorded command (a deal proposal from the language model, a battle result, an AI unit order). Replays then replay the commands. For the weekly challenge, either accept results inside the clamp from gap 2 or run challenges in classic control only. The diplomacy memory summary is also generated once and stored as data.

### 4. How often fights go live, and whose turn they interrupt

The rule "any battle a leader joins goes live" leaves open: ranged attacks and bombardment on the leader's tile, a city under siege with the leader inside (a live battle every turn), and the AI attacking the player's leader during the AI's turn in single player. Too many live battles stalls the game; too few makes the feature rare.

**Recommendation:** live only when the leader's own stack attacks or is attacked in melee, or a city holding the leader is assaulted (not bombarded). Ranged, air and naval strikes resolve with Civ math. The player can always choose auto-resolve before a fight (as in Total War), which the fairness rule from leader doc section 6 already makes safe. AI attacks on the player's leader pause the AI turn and offer the fight.

### 5. Battles after the Renaissance

Leader doc section 9 notes Mount & Blade combat fits only up to about the Renaissance; rifles, tanks and aircraft need a second design. That is more than half the tech tree, and it decides how big the battle system is.

**Recommendation:** decide the shape now: from the Industrial era the leader commands from the field (Total War view, with the avatar present and targetable but not dueling), and from the Atomic era leader-involved battles auto-resolve with an optional commander view. Prototype the medieval battle first as planned.

### 6. The walkable world leaks fog of war

The generator builds every hex from game state, and in lockstep every machine holds the full state. Walking up to an enemy city, or zooming in on one, would show its real buildings and defenders even if you never saw them. Civ shows only last-seen state.

**Recommendation:** the generator's input is the viewing player's map knowledge (visible hexes live, revealed hexes as last seen, unrevealed hexes not built at all), never the true state. Accept that lockstep makes map hacks possible, as in Civ.

### 7. Leader ability on succession: the docs disagree

`leaders-and-art-style.md` says the leader ability is lost when a successor takes over. The leader doc (section 7 and still-open item 2) proposes a pool successor keeps it. With one leader per civ, losing the ability removes the civ's identity mid-game, which runs against "the civ comes first".

**Recommendation:** the ability belongs to the throne: any successor keeps it. The heir or pool successor brings a small personal trait on top. The cost of losing a leader stays where it is now (interregnum, loyalty loss, era score, lost levels).

### 8. Game start and early-game safety

Not specified: whether the leader is an extra starting unit, whether it can found the capital, and how it survives turn 1 with a club and hide armor next to barbarian scouts. An early death or capture would be a lost game before it starts.

**Recommendation:** the leader starts on the Settler's tile in addition to Civ's normal starting units. Barbarians can wound the leader but not kill or capture it (it retreats to the capital at low HP). Assassins unlock in the Classical era (already proposed via Political Philosophy). If the capital is captured while the leader is elsewhere, the leader stays free and the Palace moves as in Civ.

### 9. Core foundations

The engine doc fixes "deterministic, headless, data-driven" but leaves the foundations that every later system depends on.

**Recommendation:** pin these in the engine doc before the first commit:
- **Numbers:** fixed-point or integer math for all rules values, no floats in the core.
- **Modifiers:** adopt Civ VI's modifier system (collection, effect, requirement sets; 00 "Architecture recommendations") as the one way to express abilities, policies, gear, promotions, reputation and difficulty. Most Sovereign mechanics fit it.
- **Commands:** every change to game state is a command (player, AI, network, live-scene result, language-model output) written to one log. This one path serves multiplayer, replays, tests and the weekly challenge.
- **RNG:** separate seeded streams for map generation, combat, AI and world visuals, so a cosmetic draw never shifts a combat roll.
- **Saves:** versioned format from day one, with golden-file round-trip tests (10 "Testing strategy").

### 10. A Sovereign build plan

Civ's build order (10, "Recommended build order") covers the clone; the Sovereign features have scattered build notes but no single plan, and the art needs (30 kits, hero models, battle animation) are far larger than the code. The project plan is for an AI to implement from the spec; code can be done that way, art largely cannot.

**Recommendation:** write one roadmap as the first `.jit/JIT_PLAN.md` when coding starts: (1) headless Civ core through MVP-7 with tests; (2) Unreal bridge with a plain hex map; (3) the leader in classic control with auto-resolved assassins; (4) one street scene (one style, City Center); (5) one medieval live battle; (6) everything else. Decide early who makes the art (hired artists, asset store, or AI-assisted tools) since it sets the pace.

## P2: decide before the related system

### 11. Heirs and civ abilities

Already open in `leaders-and-art-style.md`. Recommendation stands: a short historical dynasty per civ (starting leader plus 2 successors) as hand-made heirs, then the pool successors (governor, Great General/Admiral, level 4+ unit) once the dynasty runs out. The 12 civ abilities and uniques need their own design pass, following Civ VI's pattern of one civ ability, one unique unit and one unique building, district or improvement.

### 12. Difficulty

Civ VI's difficulty is mostly AI bonuses (extra yields, units, combat strength). Sovereign's AI aims to be hard without cheating and learns the player. Nothing says what changes between difficulty levels, or how a learned profile interacts with an easy setting.

**Recommendation:** levels scale the AI's skill first (planning depth, how much of the player profile it uses, battle AI quality), with small Civ-style bonuses only at the top two levels. Low levels should ignore most of the player profile.

### 13. The leader at sea and in the air

Embarking, ships carrying the leader, naval battles and air strikes on the leader are not covered.

**Recommendation:** the leader embarks like a land unit (and is vulnerable when embarked, as land units are in Civ, so naval escorts matter). Naval and air fights involving the leader auto-resolve. A sunk transport with the leader aboard means death, which gives navies a real role in protecting the leader.

### 14. Crossing 1 km hexes on foot

At about 1 km per hex, walking between districts or cities in direct control takes real minutes. Nothing says how the player moves around quickly.

**Recommendation:** walking is for inside a hex. Moving between hexes uses the map (normal unit moves), with an instant camera jump down into any hex the leader is in. Riding a mount speeds up walking inside a hex but does not change map moves.

### 15. Modding scope

The retention doc promises mods from day one and the engine doc says mods change data. New civs, units and gear also need models, and Unreal asset mods are harder than data mods. There is also no decision on a scripting language (Civ VI uses Lua for UI and scenarios).

**Recommendation:** launch with data and modifier mods only (new rules, balance, civs that reuse existing art). Add asset mods later through Unreal's plugin packaging. Choose a scripting language (Lua is the common choice) only if scenarios need it.

### 16. Language model safety

Players can type anything to AI leaders who are real historical figures. Without guardrails a leader can be led into offensive or out-of-character speech, which affects the age rating and store approval.

**Recommendation:** filter player input and model output, keep a fixed persona prompt per leader that refuses out-of-game topics, cap response length, and keep the scripted fallback. Plan the rating (ESRB/PEGI) assuming user-generated chat.

### 17. Saving during live scenes

Saves serialize core state, but live battles, assassin fights and street scenes hold real-time state the core does not have.

**Recommendation:** autosave when a scene starts; saving is disabled inside a scene, and loading that save restarts the scene from the beginning.

### 18. Target platforms and controls

Consoles are mentioned (engine doc, hardware section), but no target is set. Direct control wants a controller; the 4X screens want a mouse.

**Recommendation:** Windows PC first, mouse/keyboard plus controller for direct control, Steam Deck as a test device for low-end hardware. Consoles later.

### 19. Online services

Weekly challenges need leaderboards and a validation server, mods need a workshop, nemesis memory and the Hall of Sovereigns need storage, and multiplayer needs matchmaking.

**Recommendation:** pick one service layer: Steamworks (simplest if Steam is the store) or Epic Online Services (free, cross-store, has a Steam integration). Keep cross-game memory local with optional cloud sync.

## P3: known or can wait

### 20. Unverified rules and assassin questions

Already tracked: policy swap gold cost, world era triggers, spy mission odds (which block assassin odds), the Deity AI combat bonus (+3 data vs. +4 guides), and the leader doc's still-open items 3 and 4 (assassins as their own unit or a spy mission; other spy missions against the leader). Resolve the spy odds before tuning assassins.

### Smaller notes

- **Victory conditions:** none of Civ's victories change except in Regicide mode. Worth stating in the leader doc so nobody assumes otherwise.
- **Tutorial:** Civ plus an action layer is a lot to learn; plan a classic-control tutorial first and introduce direct control later.
- **Licensing of numbers:** game rules are not protected, but text, names and art are (10 "Notes on fidelity"). Sovereign's own numbers will drift from Civ's through balancing anyway.
