# Record: leader character rules the audit found off (part 1)

Status: done, 2026-10-09. An audit of `specs/sovereign/leader-character-brainstorm.md`'s decided rules against the core found 12 gaps (listed below). This part fixes the three that are clear and contained: the aura stacking with a Great General's, grievances against a leader's killer and an assassin's sender (and for Fear), and the leader's XP from the civ's deeds. Chosen by Claude under James's standing consent.

## Change

- **The leader's aura does not stack with a Great General's or Admiral's** (doc section 1: "does not stack with a Great General: the higher one applies"). Before, a unit near both got both (+3 and +5). `Game::combatStrength` now adds the larger of the leader's aura (with Marshal's +2) and the great person's.
- **Grievances** (doc sections 5 and 6, numbers Sovereign tuning in `data/rules/leader.json`):
  - a leader killed in battle or by an assassin gives its civ `LEADER_KILLED_GRIEVANCES` (100, a razed city's weight) against the killer; a capture gives none (the ransom covers it);
  - an assassin that strikes, or is captured, reveals its sender: `ASSASSIN_SENDER_GRIEVANCES` (50, twice a caught spy's 25). One killed in the attempt names no one. These sit beside the existing -15 opinion memory;
  - a Fear stance (doc section 4 table) gives `STANCE_FEAR_GRIEVANCES` (10) from each major civ sharing the ruler's religion (`civReligion`) or allied with the city's original owner. The original owner itself is not counted, as the doc names only its allies.
- **Leader XP from the civ's deeds** (doc section 3 and decision 7, "quests stay civ-level, but reward the leader"): `LEADER_XP_PER_ERA_SCORE` (1) per era score of each historic moment, `LEADER_XP_QUEST` (5) per city-state quest completed, `LEADER_XP_FOUND_CITY` (5) per city founded (`Game::leaderXp`). Completed wonders earn theirs through their historic moment, so they get no separate amount. XP still stops at the next level until a promotion is taken, and the leader has at most four promotions in a reign, so these cannot level it past what combat could.

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against #271): about a quarter of the games differ, with every measure within noise (the largest, civics +0.04 at turn 200, t 1.7). Inferred, not traced: the leader levels sooner from moments, and a higher level widens its aura.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; all 8 end states differ from #271. Time per game within 3% (two alternating pairs).
- Tests: `presence_aura_does_not_stack_with_a_great_general`, `fear_angers_coreligionists_and_the_old_owners_allies`, `the_leader_gains_xp_from_the_civs_deeds`, and grievance checks added to `fallen_leader_starts_an_interregnum_and_a_succession` and `assassins_strike_exposed_leaders`. Mutation: 13 mutants; 12 caught (3 by the warnings-as-errors build), the assassin-hit grievance only after the killed-leader check was tightened; one was equivalent (a check that the original owner is not its own ally, which `alliance()` already rules out), so that check was removed.

## Not done yet (the audit's other findings)

Larger design work, for later parts:
- Bodyguards (doc :157) and body doubles (doc :164), new units.
- Successor pool lacks governors and Great Generals/Admirals; a unit successor keeps none of its promotions or level (`leader.cpp` successor code).
- XP for the first visit to each district and city needs saved per-leader state; left out here.
- Promotion branch effects only partly built: Warlord's weapon-type bonus and leading a Corps/Army, Statesman's better stance outcomes.
- In-person diplomacy (doc :158-162) and a duel's war score (doc :163).

Judgement calls, not changed:
- Counterspies, governors and the capital do not enter the assassin's odds (doc section 6 lists them as defenses).
- Reputation has no city-state or agenda hooks.
