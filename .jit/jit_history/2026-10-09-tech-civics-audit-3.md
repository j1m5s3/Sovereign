# Record: tech, civic and government rules the spec audit found off (04, part 3)

Status: done, 2026-10-09. Previous: `2026-10-09-tech-civics-audit-2.md`. The last part of the spec 04 audit (its 12 findings are listed in part 1): Steel's urban defenses and Cultural Heritage's Shipwrecks. Chosen by Claude under James's standing consent.

## Change

- **Steel gives every city urban defenses** (04: "cities gain built-in urban defenses"; data: "adjust cities has urban defenses (DefenseValue=400)"). Before, Steel gave nothing. The generator writes `urbanDefenseHp` on the tech; `Game::urbanDefenseHp` reads the player's techs (`Rules::urbanDefenseTechs`), and `Game::cityMaxWallHp` is now the larger of the walls' HP and the urban defenses, so walls do not add to them. With outer defense every city can strike, as walls allowed before.
  - When Steel is researched, each city's outer defense rises by what its maximum rose, so a city without walls has 400 at once and a damaged one keeps its damage (`Game::completeNode`).
  - Walls built later add only what they raise the maximum by: three levels (300) add nothing to urban defenses' 400.
  - A city founded or formed (a clan's city-state) starts at its full outer defense. This also gives walls from an advanced era start their HP; before, they began at 0 until repaired.
- **Shipwrecks need Cultural Heritage** (04: "Shipwrecks appear"; data: Cultural Heritage reveals Shipwreck, "adjust unit extract sea artifacts (Extract=yes)"). Sites are still placed together once a civ knows Natural History (07:78 says both kinds then; the data's `GenerateSeaAntiquities` on Cultural Heritage would place them later, but a second placement time would add saved state for nothing a player sees). A Shipwreck can be excavated, and the AI sees and heads for it, only with Cultural Heritage: `Game::seesAntiquity(player, kind)`; kind 2 is a Shipwreck.

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against part 2): every game identical. Inferred, not traced: by turn 200 those civs have neither Steel nor Cultural Heritage.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; all 8 end states differ from part 2. Time per game unchanged (three alternating pairs).
- Tests: `steel_gives_every_city_urban_defenses` (a city with two wall levels damaged to 150 of 200 goes to 350 of 400; a city without walls gets 400; Renaissance Walls built later add nothing; a city founded after Steel starts at 400), `shipwrecks_wait_for_cultural_heritage` (Natural History digs an Antiquity Site but not a Shipwreck; Cultural Heritage digs both). `an_ai_archaeologist_passes_a_shipwreck_it_cannot_dig` (an AI Archaeologist without Cultural Heritage walks past a Shipwreck to an Antiquity Site). Mutation: 9 mutants, all caught (two by the warnings-as-errors build, as they left a value unused); the AI one was missed by the tests and the soak games alike until the Archaeologist test was added.

## Unsure

- Whether urban defenses carry the Renaissance Walls' traits (melee cannot damage them, Siege Towers cannot bypass them): Civ VI's text names only the 400 outer defense, so they do not here.
- The Unreal map still marks Shipwrecks for a civ with Natural History (`SovMirror.cpp`, the `seesAntiquity(View)` line, would pass the plot's `antiquity` kind); left to the UI work, since the cloud session does not edit Unreal code.
