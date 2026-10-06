# Plan: rules the spec audit found missing (step 6, "everything else")

Status: active, 2026-10-06. Previous: `jit_history/2026-10-06-ai-pace.md`. Two read-only audits compared specs/civ6 01, 02, 03 and 05 against the core and confirmed 22 gaps by grep. Chosen by Claude under James's standing consent.

## Milestones (one PR each)

1. **Done: the small rules:**
   - **City spacing:** cities on another landmass may stand one plot closer (02: Founding).
   - **Occupation:** a city taken from a civ still at war with the owner does not grow (`CITY_GROWTH_OCCUPATION_MULTIPLIER`).
   - **Tile price:** it rises with research, up to 3× with the whole tech or civic tree (02: Tile purchase; Sovereign reading of the curve).
   - **Theocracy:** buys land combat units with Faith, 15% under their Gold price (02: Faith purchase).
   - **Loyalty:** each civ's citizen pressure is scaled by its age, ×1.5 Golden or Heroic and ×0.5 Dark (02: Loyalty). This replaces the earlier ±population/2 on the city's own loyalty (`ageLoyalty`, removed).
   - **Unit abilities:**
     - Great Lighthouse: +1 Movement for naval units.
     - Heavy Chariot: +1 Movement on open ground, for the Heavy Chariot only; the data puts the ability on the whole Heavy Cavalry line.
     - Observation Balloon and Drone: +1 range for siege units beside one.
     - Drone: +5 bombard strength beside one (the `NEXT_TO_FRIENDLY_CLASS` condition).
     - Helicopters: no river-crossing cost.
     - Giant Death Robot: may attack while embarked.
2. **Done: Liberating cities and wonders' building prerequisites:**
   - **Liberation** (`LiberateCity`, `canLiberateCity`): on the turn of capture, a city goes back to its original owner if that civ is alive and at peace with the player.
     - The city arrives at 100 loyalty, and the liberator gains 100 Diplomatic Favor (`FAVOR_FOR_LIBERATE_*`) and a good memory with the liberated civ.
     - The Unreal production chooser offers it beside razing.
     - The AI returns captured cities that belonged to city-states.
   - **Wonder prerequisites:** the extractor's wonder table gains a Requires column (BuildingPrereqs, read from the local Civ VI install). The generator emits `requiresAny`, and `canPlaceWonder` asks for one of those buildings in the city. 14 wonders have one, e.g. the Great Library needs a Library, and Alhambra and Terracotta Army need a Barracks or Stable.
3. **Then: stealth** (naval raiders hidden unless adjacent; `ABILITY_STEALTH`, `ABILITY_REVEAL_STEALTH`), **paradrop** (Spec Ops), **airlift** (Rapid Deployment between Airports) **and amenity rebellion** (`REBELLION_*`).

## Not planned

- Map scripts and setup options: sea level, rainfall, temperature, world age, resource amount, start balance.
- Start biases and start guarantees.
- Cliffs.
- One Dam per river: the map has no river identity.
- The Giant Death Robot's automatic promotions.
- Appeal from lakes: the map has no lakes apart from Coast.

## Decisions (Claude's recommendations; James gave standing consent)

- City strength from districts: 02 (+2 per specialty district) and 03 (+2 for Encampment, Government Plaza and Diplomatic Quarter only) disagree. The core keeps 03's list.
- 05 says the Amphibious promotion removes the river-crossing cost; Civ VI does not, so the core leaves it.
