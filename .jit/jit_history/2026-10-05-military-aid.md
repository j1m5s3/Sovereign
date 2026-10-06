# Plan: Military Aid Requests and special sessions (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-trained-corps.md`. Chosen by Claude under James's standing consent: the Military Aid Request and the spacing of special sessions from 08, which the core did not have. One milestone.

Specs: 08-diplomacy-city-states-governors (Special sessions, Scored Competitions, Diplomatic Victory); data/world-congress-emergencies (Military Aid Request rows); data/global-parameters (WORLD_CONGRESS_MIN_TIME_BETWEEN_SPECIAL_SESSIONS, WORLD_CONGRESS_REQUEST_FOR_MILITARY_AID_GRIEVANCES_MIN).

## Milestone

1. **Done** (`CompetitionKind::MilitaryAidRequest`, `GameState::lastSpecialSession`, save version 55; `Game::specialSessionDue`, `checkMilitaryAid`, `requestAid(victim, military)`):
   - **Special sessions.** Emergencies, Aid Requests and Military Aid Requests are special sessions of the World Congress. Each needs WORLD_CONGRESS_MIN_TIME_BETWEEN_SPECIAL_SESSIONS (15) turns since the last. An emergency that comes too soon is not called.
   - **Military Aid Request.** Each world turn, the first major at war with a major it holds at least WORLD_CONGRESS_REQUEST_FOR_MILITARY_AID_GRIEVANCES_MIN (200) grievances against asks for one, if no competition runs and a session is due.
   - **Sending aid.** It runs like the Aid Request: Send Aid sends 200 Gold and scores 200, the winner gains 2 Diplomatic Victory points, and the high and low tiers gain 100 and 50 Favor.
   - **Who may send.** A civ at war with the asker can't send aid to either kind of request.
   - **Unreal.** The HUD names the new competition.

## Decisions (Claude's recommendations; James gave standing consent)

- Asking costs nothing (Civ VI's FAVOR_COST_FOR_REQUEST is for a human's own call, which the Congress screen does not have yet).
- Score penalties for sending aid while at war are left out: such civs cannot send at all.
- Special sessions do not wait for the World Congress to convene, matching the earlier emergencies milestone.
- With the session gap, emergencies come less often: about half as many in a 6-AI soak.
