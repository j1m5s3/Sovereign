# Record: leader character rules the audit found off (part 8)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-audit-7.md`. This part builds the personal guard (leader doc section 8.3: "Bodyguards are named companions with levels and traits who can die permanently. Drawn from units, Great Generals and governors. They are the main defence against assassins and join assassin encounters"). With it, every item the audit listed in part 1 is done. Chosen by Claude under James's standing consent; the numbers are Sovereign tuning in `data/rules/leader.json`.

## Change

- **Swearing in** (`CommandType::AppointBodyguard`, `Game::canAppointBodyguard`). The ruler keeps up to `BODYGUARD_MAX` (2) bodyguards in `Player::bodyguards` (save version 94). A bodyguard can come from:
  - a military unit of level `BODYGUARD_MIN_LEVEL` (3) or more on the ruler's plot. It becomes a Soldier named "<unit> veteran", keeps its level, and leaves the map.
  - a Great General or Admiral on the ruler's plot. It becomes a Commander named after the great person, at level 1.
  - any appointed governor, wherever it serves. It becomes a Steward named after the governor, at level 1 plus its promotions, and leaves its post.

  Levels stop at `BODYGUARD_MAX_LEVEL` (5). The kind is the trait.
- **Defence.** Each bodyguard adds a base for its kind (Soldier 6, Commander 10, Steward 4) plus `BODYGUARD_DEFENSE_PER_LEVEL` (2) for each level above 1 to `Game::leaderDefenseVsAssassin` (`Game::bodyguardDefense`). It travels with the ruler, so it guards even where no unit stands.
- **Dying in the ruler's place.** When an assassin's strike is not taken by a body double, it falls on the lowest-level bodyguard `BODYGUARD_SHIELD_PERCENT` (50) of the time. That bodyguard dies and the sender is named: grievances, the assassin memory, `EventKind::AssassinKilledGuard`, and a chronicle line "struck down a bodyguard of the ruler of".
- **Growing.** Each bodyguard gains a level when an assassin is killed or caught.
- **Lost with the ruler.** All bodyguards are lost when the ruler is killed or captured (`Game::leaderLost`).
- **AI.** Once an assassin has come for its ruler (the same trigger as a body double), an AI swears in any eligible unit at its ruler's side in the capital. It never takes its governors from their posts.

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against part 7): every game identical. Inferred: by turn 200 these AIs keep no level-3 unit or Great General in the capital after an assassin has come.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift. All 8 end states differ from part 7: the AI swears in bodyguards in longer games. Time per game is unchanged (two alternating pairs, within 3%).
- Tests:
  - `a_ruler_swears_in_bodyguards`: a level-2 unit, a unit off the plot, another civ's unit and a Great Scientist are refused; a veteran and a Great General are sworn in; a third is refused; the bodyguards survive a save.
  - `a_governor_can_guard_the_ruler`
  - `a_bodyguard_starts_no_higher_than_the_cap`
  - `bodyguards_take_blows_and_grow`: over 30 seeds the weakest bodyguard dies in some and the others grow in others.
  - `an_ai_swears_in_a_bodyguard_once_assassins_come`
  - a check in `fallen_leader_starts_an_interregnum_and_a_succession` that the bodyguards fall with their ruler.
- Mutation: 20 mutants, all caught. Gaps closed on the way: another civ's unit, a non-military great person, the level cap, and the losses with the ruler.

## Not done yet

- The Unreal side has no button to swear in a bodyguard and no list of them (left to the UI work).
- Bodyguards are not shown in a live assassin encounter (direct control): the encounter scene is auto-resolved from the same numbers.
