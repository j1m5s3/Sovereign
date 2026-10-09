# Record: tech, civic and government rules the spec audit found off (04, part 2)

Status: done, 2026-10-09. Previous: `2026-10-09-tech-civics-audit.md` (part 1, which lists the audit's 12 findings). This part fixes the 5 about tourism, naval movement and governments' intolerance; Steel's urban defenses and Cultural Heritage's Shipwrecks come in part 3. Chosen by Claude under James's standing consent.

## Change

- **Mathematics gives naval units +1 movement** (04: "+1 movement for naval units"; data: "+1 Movement for naval units"). Before, the tech gave nothing. The generator now writes `navalMoves` on the tech, and `Game::maxMoves` adds it to every unit of the Sea domain (`Rules::navalMoveTechs`, found at load since maxMoves runs often). Embarked land units are not naval units and gain nothing.
- **Printing doubles Writing's tourism** (04: "Writing Great Works give double Tourism"; data: "Tourism from Writing scaled 200%"). `writingTourismPercent` on the tech; `Game::tourismBase` scales Great Works of Writing by it, after the other scales.
- **Computers and Environmentalism add 25% Tourism each** (04; data: "+25% Tourism"). `tourismPercent` on the nodes. `tourismBase` now adds percents: 100, plus 25 for each, minus 10 for Synthetic Technocracy (before, Technocracy's -10% was the only one), so a civ with all three has 140%.
- **Conservation gives tourism for walls and the Arena** (04: "walls and Arena give Tourism"; data: +1/+2/+3 for Ancient/Medieval/Renaissance Walls in the City Center, +1 for an Entertainment Complex with an Arena). `buildingTourism` on the civic; each of a city's walls counts, so a city with all three earns 6. A building idle in a pillaged district earns nothing. The data's last line (+3 for Georgia's Tsikhe in a Golden Age) has a further condition and no civ here builds a Tsikhe, so the generator leaves it out.
- **Tier 3 and 4 governments dislike other governments** (04: "Tier 3 and 4 governments have OtherGovernmentIntolerance = -20 (diplomatic penalty toward civs with a different government)"). A new opinion reason, `OpinionReasonKind::OtherGovernment` ("Their government differs from ours"), held by a civ in a tier 3 or 4 government toward a civ in any other government. Sovereign reading: the -20 is in Civ VI's units, and this core's opinion scale is smaller (at war -10, declared friends +12, a denouncement -6), so it counts -6, a denouncement's weight.

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against #263): every game identical. Inferred, not traced: by turn 200 those AI civs have few naval units and no tier 3 government, and tourism steers no AI choice.
- Soak (8 long AI games with save/reload cuts, to turns 300-400, larger maps): no crash, replay mismatch or reload drift; all 8 end states differ from #263, so the changes do reach longer games. Time per game unchanged (three alternating pairs, within 1.5%).
- Tests: `mathematics_speeds_naval_units` (a Galley +1 with Mathematics, a Warrior not), `techs_and_civics_add_tourism` (Printing doubles a Writing work, Computers +25%, with Environmentalism +50%, with Synthetic Technocracy +15%, Conservation +1 and +2 for Ancient and Medieval Walls), `a_tier_three_government_dislikes_other_governments` (Democracy toward Monarchy -6, Digital Democracy toward Fascism -6, same government 0, Monarchy toward Democracy 0). Mutation: 9 mutants, all caught (the Technocracy one after the test was given enough tourism to show a 10% step past rounding).

## Unsure

- The -6 opinion weight is a Sovereign reading (above).
- Whether Computers' and Environmentalism's percents also apply to tourism from improvements and National Parks: here they apply to the whole of a civ's tourism, like Civ VI's player-wide tourism modifier.
