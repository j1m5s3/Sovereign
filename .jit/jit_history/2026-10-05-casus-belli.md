# Plan: casus belli, and war and peace in Unreal (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-natural-wonders.md`. Chosen by Claude under James's standing consent: the war types of 08 (only formal and surprise wars existed), and the missing Unreal controls to declare war or offer peace at all. One milestone, one PR.

Specs: 08-diplomacy-city-states-governors (War: war types and their grievance percents); data/global-parameters (DIPLOMACY_ADJACENT_EMPIRE_*).

## Milestone

1. **Done:** `CasusBelli` (Holy War, Liberation, Reconquest, Protectorate, Colonial, Territorial Expansion, Ideological) as `DeclareWar`'s arg2 (`Command::declareWarFor`), each with its civic and condition and, except Protectorate, the usual 5 turns of denouncement: Holy War (Diplomatic Service; they converted one of our cities) 50%, Liberation (Diplomatic Service; they hold a city of a friend or ally) 0%, Reconquest (Defensive Tactics; they hold a city we founded) 0%, Protectorate (Defensive Tactics; they are at war with a city-state we are suzerain of) 0%, Colonial (Nationalism; two eras behind us) 50%, Territorial Expansion (Mobilization; two of our cities within 10 of two of theirs) 75%, Ideological (Ideology; both in different tier-3+ governments) 50%. A war with a casus belli counts as formal and its grievances are a formal war's times its percent (shared with allies and friends the same way). AI: declares with the cheapest casus belli it holds before falling back to its formal or surprise war. Unreal: the diplomacy chooser (N) now lists, for each met civ, declaring a formal or surprise war, every casus belli held with its grievance percent, and offering peace.

## Decisions (Claude's recommendations; James gave standing consent)

- Not modelled: Joint War invitations (allies join through the call to arms instead). The War of Retribution followed in `2026-10-05-promises.md`, the Golden Age War in `2026-10-05-dedications.md`.
