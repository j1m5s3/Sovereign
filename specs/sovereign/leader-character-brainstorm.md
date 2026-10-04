# Sovereign twist brainstorm: the leader as a playable character

Status: brainstorm, not spec. Nothing here changes `specs/civ6/`. Section refs like "05" point at the Civ VI spec files in `specs/civ6/`.

Revision 2 (2026-10-04): folds in James's answers to the four open questions and his assassin design. Items marked **[decided]** are James's calls; everything else is a proposal to react to.

## The pitch

Each player's leader exists on the map as a single unique unit (the **Sovereign**). You move it like any unit, it levels through XP and promotions, it fights with gear unlocked by your tech, and when it steps into one of your cities or districts you can walk around inside at street level and deal with your citizens. Foreign nations send assassins after it. The game can be played two ways **[decided]**:

- **Direct control:** you play the leader as a character. Street-level scenes and assassin fights are hands-on action.
- **Classic control:** the game plays like normal Civ. The leader is a map unit, and street-level actions and assassin fights resolve automatically.

Both modes use the same rules underneath, and in both you must keep the leader guarded when it roams.

## Decisions so far

1. **When the leader falls [decided]:** all of succession, capture, an optional regicide mode, and assassination by foreign nations. On succession the player can take the heir **or** choose another leader from people already in their empire.
2. **City and district interiors are a real gameplay layer [decided]:** you interact with citizens to raise happiness, or to impose order through fear. Both have pros and cons.
3. **The leader can fight [decided]:** a customizable loadout of armor and weapons. What's available depends on the civ's tech. Each item can have cosmetic skins.
4. **Leaders don't age [decided]:** no natural succession.
5. **Assassins [decided]:** foreign nations send them. In direct control an attempt becomes an encounter where the leader fights for their life. Assassins usually outmatch the leader, and higher-level assassins hit harder, so going unguarded is dangerous. In classic control the same danger applies and the player must keep the leader guarded.

## 1. The leader on the map

This fits Civ VI's Great General model (05: a person on the map with a 2-tile aura and a one-time retire effect).

- **Stacking (05, 1UPT):** the Sovereign uses the civilian layer like a Great Person, so one military escort can share its tile, linked so they move together. Because the leader can now fight (decision 3), it defends itself when attacked rather than being captured instantly. An enemy military unit only captures it after beating it in combat while it has no escort (section 5).
- **No cost:** the leader cannot be built and has no maintenance.
- **Presence aura (2 tiles, grows with level):** +combat strength to nearby friendly units (does not stack with a Great General: the higher one applies), and loyalty per turn to a city whose tiles it stands on.
- **Leader abilities stay empire-wide (09):** each Civ VI leader's ability is unchanged. The unit adds a separate layer on top.

## 2. Loadout: armor and weapons gated by tech [decided]

Proposal: the leader's combat stats come mostly from gear, not from its unit type. That keeps the leader relevant all game without needing a separate unit upgrade line (05 "Upgrades").

- **Weapon slot** sets melee strength or ranged strength and range. **Armor slot** sets defense and can cost movement. Third **mount slot** (section 8): horse, chariot, later vehicles, gated by the civ's tech, at double the matching mounted unit's upkeep.
- **Unlocks follow the tech tree** (`data/technologies.md`), mirroring Civ VI's unit lines so gear power tracks the era's soldiers. Example ladder:

  | Tech | Weapon | Armor |
  |---|---|---|
  | (start) | Club | Hide |
  | Bronze Working | Spear | Bronze scale |
  | Archery | Bow (ranged) | |
  | Iron Working | Sword | Iron mail |
  | Machinery | Crossbow (ranged) | |
  | Apprenticeship / Military Engineering | | Plate |
  | Gunpowder | Musket / pistol | Cuirass |
  | Rifling | Rifle | |
  | Replaceable Parts / Ballistics | Machine pistol | Light body armor |
  | Composites / Robotics | Modern rifle | Ceramic / powered armor |

- **Strategic resources (GS):** gear that matches a resource-hungry unit (iron weapons, gunpowder, modern armor) costs a small one-time amount of Iron, Niter, Oil or Aluminum, like Civ VI units do (05 `strategic_cost`). That stops the leader running ahead of the army's economy.
- **Changing gear:** only in your own city, costing the leader its turn (like upgrading a unit in Civ VI). Gold cost for each new item, scaled like unit upgrades.
- **Skins [decided]:** purely cosmetic, per item. Unlocked by civ style, era, achievements or (if it ever ships commercially) the store. A skin never changes stats.
- **Balance rule:** a fully geared leader should be roughly equal to the era's best melee unit, not a one-person army. Promotions (section 3) push past that slightly.

## 3. Levelling with the Civ VI XP and promotion system

Reuses 05 "XP and promotions" directly: 15 × current level XP to the next level, excess lost, promotion heals 50 HP, 7-node trees in 4 tiers.

- **XP sources:** combat (normal formula, which now matters more since the leader fights), surviving or killing an assassin, the civ completing city-state quests and historic moments (09 era score), first visit to each of your districts and cities, founding cities, completing wonders.
- **Barbarian cap (05: level 2)** still applies to XP from barbarians.
- **Own promotion class `SOVEREIGN`**, three branches; only one can be finished per reign:
  - **Warlord:** better weapon handling (+strength with the current weapon type), stronger aura, bonus vs assassins in melee, can lead a Corps/Army with its escort.
  - **Statesman:** more loyalty pressure, better outcomes from citizen interactions (section 4), harder to assassinate (raises the assassin's difficulty, see section 6).
  - **Builder-King:** production and amenities in the city it visits, faster governor establishment (08: normally 5 turns), extra Great Person points while present.

## 4. Interiors as a real gameplay layer [decided]

Entering a city center or district tile opens a street-level scene of that place: its buildings and wonders (03), citizens and specialists, Great Works in their buildings (07). In direct control you walk it; in classic control the same actions appear as a city-screen panel.

**Citizen interactions: Benevolence vs. Fear [decided, details proposed].** Each visit lets the leader take one stance toward the city, with a cooldown per city (proposal: 10 turns) so it can't be repeated every turn.

| | Benevolence (win them over) | Fear (impose order) |
|---|---|---|
| What you do | Hear petitions, give alms, hold a feast | Public punishments, show of force, curfews |
| Immediate effect | +2 amenities in the city for 10 turns (02 "Amenities") | Instant +20 loyalty and the city stops counting as in Unrest/Revolt for 10 turns (02 loyalty and mood tables) |
| Cost | Gold (scaled by city size), or a chunk of the leader's turn | Free, but needs a military unit in the city (like Civ VI martial law) |
| Upside | Helps growth and yields; stacks well with Golden Ages | Fixes a crisis fast, stops a city flipping to a Free City |
| Downside | Slow, does little for a city already in revolt | −1 amenity for 20 turns after it ends, small grievance with civs that share your religion or are allied with the city's original owner, and raises assassin success chances in that city (resentful locals) |
| Fits Civ VI | Like a temporary Entertainment district | Like the Police State / garrison loyalty effects |

Other interior actions (front door to existing systems): appoint or move a governor, activate a Great Person in its required district, recruit or patronize Great People, and a small one-time reward for the first visit to each district.

**Scope warning (unchanged):** this needs art for every district, building and wonder per era and civ style, plus a street-level camera and movement system. Suggest the first build covers the City Center plus one district, with everything else as a panel until art exists.

## 5. When the leader falls [decided]

Three outcomes, plus an optional game mode:

- **Assassinated or killed in battle → succession.** The empire takes a hit: an interregnum where policy slots are empty for a few turns (like Civ VI's government-change anarchy), a loyalty drop in every city (02), and lost era score that can tip you toward a Dark Age (09). The killer's civ takes grievances (08) if it's identified.
- **Choosing the successor [decided]:** the player picks either
  - **the heir:** starts at level 1, keeps one promotion of the dead leader's choice, inherits the loadout; or
  - **another person from the empire.** Proposed pool:
    - **a governor (08):** becomes leader with a promotion branch seeded by their governor specialty (Victor → Warlord, Amani → Statesman, Magnus/Liang/Reyna → Builder-King, Pingala and Moksha → Statesman). The governor leaves their post, so you lose them as a governor.
    - **a Great General or Admiral you hold (05/07):** starts at a higher level and with its aura, but you lose the Great Person.
    - **a high-level military unit** (level 4+): starts with combat-heavy promotions; the unit is gone.

    Pool candidates trade a stronger start against losing something you already had. The heir is the "free" option.
- **Captured.** An enemy that beats the leader in combat while it has no escort takes it prisoner instead of killing it. The captive becomes a deal-screen item (07 "Diplomatic deals", like Civ VI's captured spies): ransom for gold, cities or peace. While held, your empire is in interregnum. You may also abandon the captive and crown a successor (above) at a heavier loyalty cost.
- **Regicide (optional game mode):** losing the leader eliminates you.

## 6. Assassins [decided, mechanics proposed]

Assassins are a new kind of espionage unit, built on Civ VI's spy system (08 "Espionage").

- **Recruiting:** unlocked by a Classical or Medieval civic (proposal: Political Philosophy, before Civ VI's first spy at Diplomatic Service). Built in a city with an Encampment or, later, the Intelligence Agency; uses spy capacity or has its own small capacity.
- **Levels [decided: higher level, more power]:** same 4-level ladder as Civ VI spies (Recruit, Agent, Secret Agent, Master). Each level raises the assassin's combat power and success chance. Their power is pegged to the sender's era, so a Master assassin from a Renaissance civ is very dangerous to a lightly armored leader.
- **Mission:** the assassin travels to the target's territory like a spy, then waits for an opening. An opening is any turn the target leader is **outside a city** or **in a city without a military unit on or next to its tile**. Fear-stance cities (section 4) give extra openings.
- **Resolution [decided]:**
  - **Direct control:** the attempt triggers an encounter scene. The assassin attacks; the leader fights for their life with their current loadout. Escort units on the same or adjacent tiles appear as guards and join the fight. With guards, the assassin is usually beaten or flees; alone, a leader typically loses unless well promoted and geared.
  - **Classic control:** the same encounter is auto-resolved from the same numbers: assassin power vs. leader power plus guards. Being unguarded is just as dangerous.
  - **Fairness rule:** both modes should give similar odds, so direct control isn't a way to cheese assassins and classic players aren't punished for not playing the action game. Proposal: direct control can at best improve the auto-resolve outcome by one step (escape instead of death), not turn a hopeless fight into a win.
- **Outcomes:** leader killed → succession (section 5); leader wounded → heavy HP loss and the leader must heal; assassin killed → leader gets XP; assassin captured → can be traded back like a spy, and the sender is revealed and takes grievances.
- **Defenses:** escorts (the main one), counterspies in the city (08), Statesman promotions, Victor the Castellan's defensive promotions, staying in the capital. Strong armor helps you survive, but the design intent is that no loadout makes an unguarded leader safe.
- **Open dependency:** Civ VI's spy success odds are still unverified in our spec (an inferred 3d6 model), so the assassin odds should be built after that research.

## 7. Where it clashes with Civ VI (and suggested answers)

| Clash | Why it matters | Suggestion |
|---|---|---|
| Great General / Admiral overlap | Two auras doing the same job | Auras don't stack; Generals keep retire effects and can become successors |
| Governors overlap (08) | Governors already are "a person in a city with promotions" | Governors are fixed, the leader is mobile. Governors can also become successors |
| Leader abilities are empire-wide (09) | Tying them to the unit means redesigning every leader | Keep them empire-wide. A successor from the pool keeps the civ's leader ability (proposal) |
| Leader can fight | A single unit with no maintenance could dominate | Gear capped around the era's best melee unit; losing it is very costly |
| Domination victory (09) | About capitals, not leaders | Unchanged, except in Regicide mode |
| AI (10) | Civ AI is careless with units | AI rule: never end a turn with the leader unguarded where an assassin or enemy can reach; send its own assassins at unguarded rivals |
| Multiplayer pacing | Street scenes and fights take real time | Street scenes pause nothing and the turn timer keeps running. Assassin encounters happen during the target's turn; if the target is in classic control or AFK, auto-resolve |
| Engine scope | Sovereign now needs a 4X map, a street-level 3D scene and an action combat system | Build order: map leader + loadouts + auto-resolved assassins first, then street scenes, then direct-control fights |

## Precedents

- **Great Generals / Admirals** and **spies with levels** (in our spec, 05 and 08).
- **Civ VI Heroes & Legends mode** (not in our spec; from memory, unverified): hero units that can die and be recalled for Faith.
- **Mount & Blade / Total War:** a campaign map plus a hands-on battle scene for the same encounter, with auto-resolve as an option.
- **Crusader Kings:** succession and assassination plots.

## 8. Enhancements (decided by James 2026-10-04; details proposed)

Guiding principle **[decided]:** stay grounded in core Civ mechanics, and the civ comes first, the leader second. No legendary gear and no personal leader quests.

1. **Reputation: Beloved vs. Feared [decided].** A meter driven by Benevolence/Fear choices (section 4) and deeds (sparing or razing cities). It feeds **happiness** (02 amenities):
   - Beloved: + amenities empire-wide at high reputation, better loyalty, better standing with city-states.
   - Feared: cities rarely flip from loyalty pressure, but amenities fall and assassin openings rise.
   - AI agendas (08) can like or dislike either reputation.
2. **Rebellion from too much iron fist [decided].** If heavy Fear rule pushes a city's happiness low enough (proposal: Unrest or worse on the 02 mood table while the reputation is Feared), the city can rebel: a domestic pretender appears with rebel units, like Civ VI's Free City revolt but violent. Put it down by force or win the city back with Benevolence.
3. **Personal guard [decided].** Bodyguards are named companions with levels and traits who can die permanently. Drawn from units, Great Generals and governors. They are the main defence against assassins (section 6) and join assassin encounters.
4. **Diplomacy in person [decided].** The leader can travel to a foreign capital or city-state:
   - vs. AI civs and city-states: improves acceptance odds and relationship gains (e.g. better AI deal acceptance, extra envoys or influence with a city-state).
   - vs. human players: no mechanical bonus.
   - Sending a diplomat (Civ VI's delegations and embassies, 08) stays available, so you never have to risk the leader to do diplomacy.
   - The trip exposes the leader to assassins, and to capture if war breaks out.
5. **Duels [decided: only on the battlefield].** No challenge system. A duel happens only when one leader deliberately attacks another leader's unit on the map. In direct control it plays as a leader-vs-leader encounter; in classic control it resolves as normal combat. Winning gives XP and a large war score; the loser is killed or captured (section 5).
6. **Body doubles [decided: expensive].** A unit unlocked by a Renaissance-era civic (proposal: Diplomatic Service, alongside spies), costing a lot of production (proposal: about 2x a spy) plus upkeep. An assassin who strikes while a double is active has a chance to hit the double instead.
7. **Quests stay civ-level, but reward the leader [decided].** Quests work as in Civ VI: city-state quests (08) and civ-level goals (historic moments, dedications, 09). When the civ completes one, the leader gets bonus XP. No leader-only quests.
8. **Mounts [decided].** Mounts are civ-level: they come from the civ's tech and resources the same way its mounted units do (Horseback Riding, Stirrups, later vehicles). The leader can equip one in the loadout's mount slot. A leader's mount costs **double the upkeep** (gold and, in GS, strategic resource) of the matching mounted unit.
9. ~~Legendary gear~~ **[rejected]**: no gear outside the tech ladder.

## 9. Combat with player-controlled leaders (revised 2026-10-04)

**Decided by James:** Mount & Blade / Total War style live battles for any battle a leader takes part in. Civ math stays the backbone: the units you bring, promotions, terrain and the other Civ combat factors must weigh heavily, and skill or tactics alone must not let a player overmatch a stronger force. Putting the leader in a fight is a big risk and should feel like it. Players not in a battle are not slowed down by it, and war penalties stay as in Civ (war weariness, grievances, amenities).

Proposed rules:

- **What goes live:** a fight that involves a leader becomes a real-time battle on the actual terrain of the continuous world. You fight as the leader (Mount & Blade) and command the other units in the battle (Total War). Fights without a leader, and AI vs AI, resolve with Civ's formula (05). Classic control auto-resolves everything.
- **Who joins:** all units of both sides within about 1 tile of the clash. Civ's Corps/Army formations map to larger regiments.
- **Civ math as backbone:** each unit's battle stats come from its Civ strength, promotions, HP, terrain and modifiers (Great General aura, support, flanking). Expected outcome follows the formula; player skill shifts it within a bounded band (proposal: about ±25%), so numbers and quality decide most battles. Results return to the map as HP lost, units destroyed or captured, and XP.
- **Escort rule:** the leader is on the civilian layer; an enemy must get through the escort to reach the leader.
- **Ranged/siege/air/naval:** take part as battle units, or as support called in from outside the battle area in later eras.
- **Turns: battle zone lock.** Tiles and units in a battle freeze until it ends. The rest of the world keeps taking turns, including the battling players' own cities (following their build queues), so uninvolved players never wait. The cost of fighting live is that you are not running your empire meanwhile.
- **Real-time cap:** after a set time (proposal: one turn timer) the battle auto-resolves from its current state, so a stalemate can't freeze a region.
- **Multiplayer:** a human vs human battle is live for both if both are online; otherwise the absent side's units are commanded by the battle AI.
- **Cities:** a leader in a city adds to its defense like a garrison. If the city is captured, the leader is captured unless it leaves before the city falls.
- **Eras:** Mount & Blade-style combat fits up to about the Renaissance. Rifles, armor and air power need a second battle design. Prototype a medieval battle first.

## 10. AI opponents (layers decided by James 2026-10-04)

James's goal: AI as smart as possible that learns what the player does. All four layers below are **[decided]**:

1. **Strategy AI:** planner weighing goals (expand, defend, attack, wonder, victory path) plus influence maps of threats and opportunities. Fast, testable, hard without cheating.
2. **Player modeling:** the AI keeps a profile of each human: tech path, aggression, army composition, how exposed the leader is, live-battle habits (flanking, retreating). It counters what it sees (anti-cavalry vs cavalry players, assassins vs roaming leaders). The profile can persist between games.
3. **Battle AI trained by machine learning:** real-time unit control trained by self-play in simulated battles. Narrow, well-understood problem; addresses the weak battle AI risk.
4. **Language-model diplomacy:** AI leaders that converse, negotiate and remember.
   - **Local, lightweight, open source [decided]:** a 3–8B parameter model quantized to 4-bit (~2–5 GB), run through llama.cpp. Prefer permissive licenses for a commercial game: Qwen (Apache 2.0), Phi (MIT), Mistral's Apache models; Llama and Gemma have more restrictive terms. Benchmark current releases when building; this list reflects knowledge up to mid-2026.
   - **The model talks, the game decides:** Civ diplomacy rules (deal acceptance, agendas, grievances) decide outcomes. The model turns player text into deal-screen proposals (structured output) and writes the leader's reply in character. It cannot be talked into deals the rules reject.
   - **Personality and memory:** prompt built from the leader's agendas, relationship state and a short summary of past conversations.
   - **Performance:** runs only on the diplomacy screen; a 1–2 second response is acceptable; never during live battles.
   - **Fallback:** scripted lines on weak hardware; optional cloud model for players who opt in.

Avoid: AI that retrains itself mid-game (hard to test, learns exploits). An LLM controlling troops or every turn of every AI civ (too slow, unreliable at precise tactics, costly).

## 11. Hardware portability (decided 2026-10-04)

No dependency on Intel or Nvidia hardware.
- **Language model:** llama.cpp with its Vulkan backend (AMD, Nvidia and Intel GPUs), Metal on Mac, CPU fallback (x86 and ARM, including Apple Silicon and Snapdragon).
- **Battle AI:** trained offline on our side; the small trained network runs on any CPU via a cross-vendor runtime such as ONNX Runtime.
- **Engine:** Unreal 5 (decided; see [engine-and-architecture.md](engine-and-architecture.md)) supports AMD, Intel and Apple GPUs; current consoles are AMD-based.
- **Rule:** no vendor-only features. No CUDA-only code in the game; if upscaling is added, offer FSR and XeSS alongside DLSS.

## Still open

1. ~~Who counts as "a leader existing in the empire" for succession?~~ Confirmed by James 2026-10-04: heir, governors, Great Generals/Admirals, level 4+ units.
2. Does a successor from the pool keep the civ's leader ability, or bring a different one?
3. Are assassins their own unit with their own capacity, or a new mission for ordinary spies?
4. Can the leader be the target of other spy missions (wound, frame, kidnap), or only assassination?
