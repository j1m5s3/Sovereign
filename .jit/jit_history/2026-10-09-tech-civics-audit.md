# Record: tech, civic and government rules the spec audit found off (04, part 1)

Status: done, 2026-10-09. Previous: `2026-10-09-eras-audit.md`. A read-only audit compared specs/civ6/04-tech-civics-government.md with the core (techs, civics, boosts, costs, governments, policy cards and slots), leaving out what earlier records settle (`2026-10-06-spec-audit-part-2.md`, `2026-10-06-policy-cards.md`, `2026-10-06-dark-age-policies.md`, `2026-10-09-government-bonuses.md` and others), and confirmed 12 rules that differ in code. This first part fixes the 5 about boosts, costs and policy slots; tourism, movement and government intolerance come in part 2, Steel's urban defenses and Cultural Heritage's Shipwrecks in part 3. Chosen by Claude under James's standing consent.

## Change

- **A policy slot gained opens a free change window** (04: "Policies can be swapped for free on any turn a civic completes or a new slot is gained"). Before, a slot from the Alhambra, Big Ben, the Potala Palace, the Forbidden City or World Ideology stayed empty until the next civic unless the player paid Gold. Now `Game::syncPolicySlots` opens the window when the slots grow, outside anarchy and an interregnum (the window opens anyway when those end).
- **Each Neighborhood counts toward Sanitation** (04: "build 2 Neighborhoods"). District boosts counted each city once, so two Neighborhoods in one city made one. Now a repeatable district (Neighborhoods, Canals) counts each time; other districts still count by city.
- **Any Neighborhood may be Conservation's Breathtaking one** (04: "have a Neighborhood with Breathtaking appeal"). Before, only a city's first Neighborhood was looked at. Now every one is (`BoostKind::DistrictAppeal`).
- **Boosts from other sources are earned like the rest** (04: boosts; 09: Free Inquiry, Pen, Brush and Voice; 07: lifetime culture; 08: quests). Tribal villages, the Great Library, Great Scientists, Steal Tech Boost and the Nobel Prize in Physics set the boost themselves, so they missed the dedications' +10% in a Golden Age and era score, an Inspiration's lifetime culture and city-state boost quests. They now call `Game::grantBoost`.
- **Civic costs read the civic parameters** (04: the world-era ±20% is "same for civics", `CIVIC_COST_PERCENT_CHANGE_*`). Before, civics read the tech ones. The values are equal, so only a mod sees a difference; there is no test for it.

Results:
- Pace (128 AI games, 6 civs, against #262): 115 of 128 games changed; techs +0.00 ± 0.04, civics -0.05 ± 0.03, cities -0.05 ± 0.03, population -0.44 ± 0.18, science -0.93 ± 0.60, culture -0.52 ± 0.34, production -1.42 ± 0.72, Gold -6.6 ± 3.76. All small; population and production sit just past two standard errors, read as the stricter Eureka paths (Sanitation, Conservation) and fewer free policy changes.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; all 8 end states differ from #262. Time per game unchanged (three alternating pairs).
- Tests: `wonders_add_policy_slots` now checks the window opens with the Alhambra's slot and not during anarchy; `boosts_from_wonders_places_and_continents` checks a city's two Neighborhoods for Sanitation and its second for Conservation; `the_nobel_prize_eureka_counts_free_inquiry` (40% in a Normal Age, 50% in a Golden one). Mutation: 7 mutants, 5 caught; the 2 missed change nothing a game can reach (counting a non-repeatable district twice in one city, and swapping the civic cost parameters for the tech ones, whose values are equal).

## Unsure, not changed

- Changing government empties every slot (Civ VI keeps the cards that still fit; the spec does not say).
- The Education, Humanism and Mercantilism boosts count Great People owned, so one lost for want of a plot never fires them ("earn" in the spec).
- Medina Quarter leaves with Suffrage even for a civ that is no Democracy and so cannot slot New Deal.
