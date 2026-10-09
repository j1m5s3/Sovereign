# Record: leader character rules the audit found off (part 2)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-audit.md` (part 1, which lists the audit's findings). This part widens the successor pool to governors and Great Generals and Admirals, and gives pool successors their promotions. Chosen by Claude under James's standing consent.

## Change

- **New successors** (doc section 5, "another person from the empire"): `Succession::Governor` (the command's unit is an appointed governor's type) and `Succession::GreatPerson` (a Great General or Admiral the player holds; a unit of that class with no great person in it does not count). `Game::successorGovernors` and `Game::successorGreatPeople` list them, as `successorUnits` lists veteran units.
- **What each brings** (`Game::successorPromotions`, Sovereign tuning):
  - a veteran unit: Weapon Master (level 2; the doc's "combat-heavy promotions"). Before, it started at level 1;
  - a Great General or Admiral: Weapon Master and Marshal (level 3, the aura +2; the doc's "higher level and with its aura");
  - a governor: the first promotion of the branch its specialty seeds (doc: Victor Warlord, Amani, Pingala and Moksha Statesman, Magnus, Liang and Reyna Builder-King).
- **What is lost:** the unit or the Great Person is removed; the governor leaves its post and `Player::governors`, and the titles spent on it stay spent. The governor type may be appointed again for a new title, starting over (Sovereign reading of "you lose them as a governor").
- A governor or great person on hand does not stop a regent: giving one up is the player's choice, and a regent stays open when there is no heir and no veteran unit, as before.
- **The AI** crowns the heir, else a Great General or Admiral, else its most seasoned unit, else a regent; never a governor.
- The ruler's name: "<civ> Marshal" for a great person, the governor's own name for a governor. The chronicle says "A governor" or "A great commander took the throne" (before, any successor but an heir or a unit read as a regent).

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against part 1): every game identical. Inferred: no AI civ loses its leader with its dynasty spent by turn 200.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; 1 of 8 end states differs from part 1. Timing not measured: the change runs only at a succession.
- Tests: `a_great_general_can_take_the_throne`, `a_governor_can_take_the_throne`, `an_ai_crowns_a_great_general_before_a_veteran`, and Weapon Master checked in `a_veteran_unit_can_take_the_throne`. Mutation: 13 mutants, 12 caught (one by the warnings-as-errors build). The one missed drops the check that a Great General holds a great person; the lookup that follows then reads past the great people list, so no test can safely show it.

## For the UI

- The Unreal successor list (`SovPlayerController.cpp`, the heir, unit and regent choices) does not offer governors or great people yet; it would add `chooseSuccessor(Me, Succession::Governor, type)` for each `successorGovernors` entry and `Succession::GreatPerson` for each `successorGreatPeople` unit. Left to the UI work, since the cloud session does not edit Unreal code. Nothing is stuck meanwhile: the old choices stay open.

## Still open from the audit

Bodyguards, body doubles, XP for first visits, the rest of the promotion branch effects, in-person diplomacy, duel war score (see part 1).
