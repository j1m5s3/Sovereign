# Plan: dedications (step 6, "everything else")

Status: done, 2026-10-05. Previous: the natural wonders follow-up in `jit_history/2026-10-05-natural-wonders.md`. Chosen by Claude under James's standing consent: Rise and Fall's dedications (commemorations) from 09, which the core did not have, and with them the Golden Age War left out of `2026-10-05-casus-belli.md`. One milestone.

Specs: 09-civs-eras-victory-climate (Ages, Dedications); data/eras-moments-loyalty (Dedications); data/global-parameters (COMMEMORATE_*); 08 (War types: Golden Age War).

## Milestone

1. **Done** (generated `dedications` in `moments.json` with each dedication's era window; `Rules::dedications`, `Player::dedications`, `Player::dedicationsPending`, save version 58; command `ChooseDedication` 62; `CasusBelli::GoldenAge`; `Game::availableDedications`, `dedicationProblem`, `chooseDedication`, `dedicated`, `goldenDedication`, `dedicationScore`):
   - **Choosing.** When the world enters a new era, each major civ chooses one dedication, or three in a Heroic Age (COMMEMORATE_BASE_CHOICES_ALLOWED, COMMEMORATE_OPTIONS_MAX). The choice is among those whose era window holds the new era. Choosing never blocks the end of a turn.
   - **Normal and Dark Ages.** A dedication's deeds earn era score:
     - Free Inquiry: Eurekas and Campus buildings.
     - Pen, Brush and Voice: Inspirations and Theater buildings.
     - Monumentality: districts.
     - Exodus of the Evangelists: +2 per city converted.
     - Hic Sunt Dracones: +3 per natural wonder discovered, +1 per naval kill.
     - Reform the Coinage: trade routes completed.
     - Heartbeat of Steam: Industrial Zone buildings.
     - To Arms!: +1 per Corps and +2 per Army killed.
     - Wish You Were Here: artifacts extracted.
     - Sky and Stars: Aerodrome buildings and great people.
     - Bodyguard of Lies: successful spy missions.
     - Automaton Warfare: kills by a Giant Death Robot.
   - **Golden and Heroic Ages.** The bonus applies instead:
     - Free Inquiry: Eurekas +10 points; Commercial Hubs and Harbors add their Gold adjacency as Science.
     - Pen, Brush and Voice: Inspirations +10 points; +1 Culture per district.
     - Monumentality: Builders and Settlers 30% cheaper to buy; Builders +2 Movement.
     - Exodus: Missionaries, Apostles and Inquisitors +2 Movement and, when bought, +2 spreads; +4 Great Prophet points a turn. (Until 2026-10-08 the 2 spreads also went to other units bought with Faith, such as Naturalists.)
     - Hic Sunt Dracones: naval and embarked units +2 Movement.
     - Reform the Coinage: international routes +3 Gold per specialty district at the destination; routes immune to plunder.
     - Heartbeat of Steam: +10% Production toward Industrial-and-later wonders; Campuses add their Science adjacency as Production.
     - To Arms!: +15% Production toward military units; the Golden Age War.
     - Wish You Were Here: +50% tourism from cities with an established governor.
     - Bodyguard of Lies: spies travel twice as fast; offensive operations 25% faster.
     - Sky and Stars: air units +100% XP.
     - Automaton Warfare: one Giant Death Robot in the capital when chosen.
   - **Golden Age War** (`CasusBelli::GoldenAge`, 25% of a formal war's grievances). It needs a Golden Age with To Arms!, and is usable right after denouncing.
   - **AI.** It chooses the dedication its strategy favours (science, culture, religion, domination), else the first open.
   - **Unreal.**
     - The F2 chooser offers this era's dedications.
     - The HUD lists the chosen ones and reminds the player while choices remain.
     - The war chooser names the Golden Age War.

## Decisions (Claude's recommendations; James gave standing consent)

- Effects are coded by dedication id, like the natural wonders. The extracted modifier text names them, but not in a form the generator can turn into rules.
- Not modelled:
  - Hic Sunt Dracones' continent discoveries and its starting population and loyalty off the home continent.
  - Monumentality's Faith purchase of civilian units.
  - Wish You Were Here's National Park tourism (no National Parks).
  - Sky and Stars' free late Eurekas and Aluminum, and Automaton Warfare's Uranium.
  - Georgia's normal-age goal in a Golden Age.
