# Plan: trading captured spies back (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-encampment-combat.md`. Chosen by Claude under James's standing consent: "trading captured spies back" was still not modelled after `2026-10-05-espionage-depth.md`. One milestone, one PR.

Specs: 08-diplomacy-city-states-governors (Espionage, Outcomes: "Captured spies can be traded back (Captive deal item)").

## Milestone

1. **Done: captives:** a spy captured on a failed operation is kept by the civ that caught it (`GameState::capturedSpies`, `CapturedSpy{spy, captor}`, save version 64). It is no longer the owner's agent, so it does not count against spy capacity and cannot act. The deal item `DealItemKind::Captive` (amount = the spy's id) lets the captor give it back to its owner, once per deal. Like every other item it needs peace, or comes with a peace deal. Traded back, the spy comes home idle with its level and promotions. Deal values (Sovereign's): 40 + 40 x level to the owner, 20 + 20 x level to the captor. AI: offers 30 + 30 x level gold for its own spies when it has the gold. Unreal: the deal screen lists it through `offerableItems` and `describeDealItem` ("returns a captured spy (level N)").

## Decisions (Claude's recommendations; James gave standing consent)

- Captives are held in their own list rather than flagged in `GameState::agents`, so the spy and assassin code that walks the agent list stays as it was.
- A captive whose owner or captor is eliminated simply stays held; it no longer matters.
- Not modelled: the language-model parser does not read a "captive" item yet (the rules side and scripted replies cover it).
