# Plan: Corps and Armies trained whole (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-district-purchase.md`. Chosen by Claude under James's standing consent: the direct training of Corps and Armies from 05, left out of `2026-10-05-formations.md` (which only merges units). One milestone.

Specs: 05-units-and-combat (Corps and Armies); data/global-parameters (UNIT_CORPS_COST_MODIFIER, UNIT_ARMY_COST_MODIFIER).

## Milestone

1. **Done** (`ProductionItem::formation`, `packedKind`; `Game::canTrainFormation`):
   - **Who trains them.** A city with a Military Academy trains a land unit as a Corps, or as an Army once the civ has Mobilization; a city with a Seaport does the same for ships (Fleets and Armadas). Corps need Nationalism. Civilians, support units and aircraft can't be trained whole.
   - **Cost and speed.** The production cost is ×1.5 for a Corps and ×2 for an Army (UNIT_CORPS_COST_MODIFIER, UNIT_ARMY_COST_MODIFIER), and the training building adds +25% production toward them. Gold purchase follows the production cost.
   - **The trained unit** spawns with its formation set.
   - **Commands and saves.** Commands carry the formation in the high bits of the production kind argument (`kind | formation << 4`). Saves carry it in the item's kind byte, so older saves load unchanged and the save version stays 54.
   - **Production list.** `buildableItems` lists the Corps and Army versions beside the single unit.
   - **AI.** It scores them at their unit's strength plus +10 or +17, against the higher cost.
   - **Unreal.** Production names show "Corps", "Army", "Fleet" or "Armada".

## Decisions (Claude's recommendations; James gave standing consent)

- A Corps or Army trained whole uses one unit's worth of strategic resources and counts as one unit trained (Sovereign reading; Civ VI charges more resources, which the extracted data does not give).
