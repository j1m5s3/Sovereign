# Record: diplomacy rules the spec audit found off (08)

Status: done, 2026-10-09. Previous: `2026-10-09-economy-audit.md`. A read-only audit compared specs/civ6/08-diplomacy-city-states-governors.md with the core (diplomatic actions, war, city-states, governors, espionage, Diplomatic Favor, the World Congress, emergencies), leaving out what earlier records settle (`2026-10-05-diplomacy-depth.md`, `2026-10-05-civ-systems-2.md`, `2026-10-05-espionage-depth.md`, `2026-10-05-casus-belli.md`, `2026-10-06-spec-audit-part-2.md`, `2026-10-06-suzerain-bonuses.md`, `2026-10-06-alliance-effects.md` and others), and confirmed 5 rules that differ in code. Chosen by Claude under James's standing consent; one PR.

## Change

- **Allies renew their alliance and keep its level** (08: Alliance, "30 turns, renewable. Alliance points accumulate → levels 1–3"). Before, an Alliance could not be offered while one lasted, and a lapsed one lost its points, so the next started from nothing. One alliance earns at most 6 points a turn (9 with Democracy, its Legacy card and Wisselbanken), so over its 30 turns it never reached level 2's 320, and the level 2 and 3 effects never came in play. Now allies renew theirs, of the same type, at any time and without friendship: 30 turns from the renewal, its points kept. Another type waits for the alliance to lapse; a lapsed alliance is gone with its points, and a new one needs friendship again (`Game::dealProblem`, the Alliance deal item). An AI renews an alliance with a civ it likes once fewer than 10 turns are left (`kProposalGap` in `src/ai.cpp`). Sovereign reading: Civ VI offers the renewal as the alliance ends; here it can come any time.
- **Spies and assassins cost their upkeep** (08: Espionage, "4 Gold maintenance"). They are off the map, so `Game::goldPerTurn` left them out. Now each costs its unit's maintenance (a Spy 4, an assassin 2), less Conscription's 1, as a unit would.
- **Seasteads and Global Warming Mitigation give a Diplomatic Victory point** (08: Diplomatic Victory; data: technologies, civics). The generator dropped the "+1 Diplomatic Victory Points" in their effects; `tools/rules_gen/gen_rules.py` now sets `victoryPoints` on tech and civic nodes and `Game::completeNode` adds them.
- **Colonial War counts technology eras** (08: "target is 2 technology eras behind you"). Before, it compared the later of each civ's tech and civic eras, so Nationalism, an Industrial civic, put almost any civ in reach. Now only techs count (`Game::techEra`).
- **Allies show as Allied** (08: relationship states, "Allied (100)"). `Relationship::Allied` comes before Declared Friend; the scripted diplomacy model speaks to an ally as to a friend.

Results:
- 128 AI games (Small, 6 AI, turn 200) against main: 11 of 128 games change at all, as AI spies and alliances are rare that early (science +0.1 ± 0.1, Gold +0.3 ± 0.5). Eight 150-turn 6-AI games take the same time (11.1 to 11.6 s against 11.1 to 11.9 s, three runs each).
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; 7 play out differently.
- Tests: `allies_renew_their_alliance_and_keep_its_level` (a renewal without friendship keeps level 2 and runs 30 turns; a lapsed alliance needs friendship and starts at 0), `an_ai_renews_its_alliance_before_it_lapses`, `spies_and_assassins_cost_their_upkeep` (spies, an assassin, another civ's spy, Conscription), `seasteads_and_global_warming_mitigation_give_victory_points`, `a_colonial_war_counts_technology_eras`, `an_ally_is_greeted_as_a_friend` (diplomacy tests); `friends_with_civil_service_form_an_alliance` now refuses another type while allied and checks Allied. Mutation check: 16 mutants, all caught (the upkeep test gained a game without the other civ's spy to catch the last).

## Unsure, not changed

- Whether Gain Sources helps the civ's other spies in that city (the core: only the spy that gained them).
- Whether combat on one's own land adds war weariness (the core: none; the data names foreign and allied lands).
