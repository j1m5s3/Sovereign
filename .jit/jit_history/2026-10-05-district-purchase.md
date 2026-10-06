# Plan: district purchase (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-barbarian-scouts.md`. Chosen by Claude under James's standing consent: Reyna's Contractor and Moksha's Divine Architect promotions from 08, left out of `2026-10-05-governor-promotions.md` because nothing could buy a district. One milestone.

Specs: 08-diplomacy-city-states-governors (Governors: Reyna, Moksha); 02-cities (Purchasing).

## Milestone

1. **Done** (`Game::districtPurchaseCost`; the `Purchase` command now takes a District item; no save change):
   - **Who can buy.** A city whose governor holds Contractor buys a placed, unfinished district with Gold; one whose governor holds Divine Architect buys it with Faith (`purchaseWithFaith`).
   - **Price.** The usual gold price: 4× its production cost, rounded down to a multiple of 5, whichever the currency.
   - **What happens.** The district completes at once and leaves the queue; any production already put into it carries over.
   - **AI.** When a placed district heads a city's queue, the AI buys it with Faith if it would keep 100 Faith, else with Gold above its usual reserve. It no longer tries to buy districts it cannot.
   - **Unreal.** The production chooser offers "Buy the <district> for N gold/faith" when allowed.

## Decisions (Claude's recommendations; James gave standing consent)

- Civ VI prices districts bought with Faith differently from Gold; the extracted data has no Faith price, so both use the gold formula (Sovereign reading). The district must already be placed (placement happens when it goes into production), so a purchase never chooses a plot.
