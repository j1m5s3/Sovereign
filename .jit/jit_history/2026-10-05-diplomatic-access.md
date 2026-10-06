# Plan: diplomatic access (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-promises.md`. Chosen by Claude under James's standing consent: delegations, resident embassies, access levels and gossip from 08, which the core did not have. One milestone.

Specs: 08-diplomacy-city-states-governors (Meeting and relationship states: Access level; Diplomatic actions: Send Delegation, Resident Embassy); data/diplomacy-espionage (Diplomatic actions, Diplomatic visibility sources).

## Milestone

1. **Done** (`Relation::delegation`, save version 53; command `SendDelegation` 60; `Game::accessLevel`, `accessName`, `gossipLevel`, `hearsOf`, `delegationProblem`, `wouldReceive`, `sendDelegation`):
   - **Delegations and embassies.** A major sends a delegation (25 Gold, until it has Diplomatic Service) or a resident embassy (50 Gold, from Diplomatic Service; it replaces the delegation) to a major it has met and is at peace with. An AI turns one away while it denounces or dislikes the sender (opinion -20 or worse); a delegation it accepts gives +3 opinion. War sends both home.
   - **Diplomatic Quarter favor [GS].** With a completed Diplomatic Quarter, a civ gains +1 Favor a turn for each delegation and embassy it keeps.
   - **Access levels.** Meeting gives Limited. One level each comes from the Printing tech, a trade route into the civ, a delegation or embassy, an alliance, and a spy in one of its cities; the spy adds two if it is level 3 or better. The cap is Top Secret.
   - **What access shows.** Secret shows the civ's capital and the ring around it, and Top Secret every city of theirs (`refreshVisibility`).
   - **Gossip.** Every civ that has met a civ hears its wars, peace, denunciations, friendships, assassinations and rebellions. Its deals and new ages need Open, its great people and historic moments need Secret, and its spies' successes need Top Secret.
   - **AI.** With 150 Gold it sends an embassy, or else a delegation, to every met major that would receive it.
   - **Unreal.**
     - The diplomacy chooser (N) offers delegations and embassies and shows the current access.
     - The diplomacy screen's header shows the access, and shows the agenda only from Open.
     - The HUD's news lines include gossip (at most 6 a turn), worded for others' events.

## Decisions (Claude's recommendations; James gave standing consent)

- The delegation's land path to the capital is not checked, a human always receives delegations, and the gossip tiers above are Sovereign readings: the extracted data names the sources, not what each level reveals.
- Not modelled: Mary Katherine Goddard, civ-trait sources (no civ has them yet), and Listening Post's extra gossip.
- The AI keeps its settling promises even for a settler already on its way, or one with no better site.
