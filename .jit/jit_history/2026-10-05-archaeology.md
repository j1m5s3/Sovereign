# Plan: archaeology (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-ui-gaps.md` (and Aid Requests, recorded in `jit_history/2026-10-05-competitions.md`). Chosen by Claude under James's standing consent: antiquity sites, shipwrecks and the Archaeologist from 07, which the core did not have. One milestone, one PR.

Specs: 07-great-people-great-works-tourism (Great Works slots, Archaeology); data/global-parameters (ARCHAEOLOGY_*); data/units (Archaeologist).

## Milestone

1. **Done** (`src/archaeology.cpp`; `Plot::antiquity`, `GameState::battleSites`, `antiquityPlaced`, save version 51; command `Excavate` 58): every unit fight remembers its plot while the world era is at most ARCHAEOLOGY_MAX_ERA. On the first world turn when any major civ knows Natural History, ARCHAEOLOGY_SITES_PER_CIV_LAND (6) antiquity sites and ARCHAEOLOGY_SITES_PER_CIV_SEA (2) shipwrecks per major civ appear, on those battle plots first and then on open plots (shipwrecks on coastal water). Only civs that know Natural History see them. The Archaeologist (generated `excavations` 3, used as its charges) digs a site in its civ's land, unowned land, or land whose owner opened its borders to it, putting an Artifact into a free artifact slot (the Archaeological Museum's) of one of its cities. AI: one Archaeologist while open sites remain in its land or unowned land; it digs where it stands or walks to the nearest site. Unreal: sites show as pale stones for a civ that knows Natural History; the build chooser (B) offers Excavate.

## Decisions (Claude's recommendations; James gave standing consent)

- Not modelled: Landmarks (no data rows), artifacts' eras and civilizations (and so the Archaeological Museum's theming bonus), the 400-production cost's gold purchase rules beyond the usual.
