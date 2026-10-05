# Plan: the late game: power, carriers and airstrips, nuclear weapons (step 6, "everything else")

Status: active, 2026-10-05. Previous: `jit_history/2026-10-05-civ-identities.md`. Chosen by Claude under James's standing consent: the remaining Civ VI late-game systems the specs describe and the core lacks. James can redirect at any point (his art and UI review is open whenever he wants it).

Specs: 09-civs-eras-victory-climate (Power [GS]); data/buildings (Power [GS], Extra when powered) and data/improvements (renewables); 05-units-and-combat (Aircraft Carrier, Airstrip, WMDs) with data/units and data/projects (Manhattan Project, devices).

## Milestones (one PR each)

1. **Done: Power:** buildings that need power (Factory, Stock Exchange, Research Lab, Stadium...) and their bonus when fully powered; a city short of power loses up to 50% production. Sources: Coal, Oil and Nuclear Power Plants burn their resource for 4/4/16 power to every own city within 6 tiles (replacing today's flat burn, so CO2 follows real demand), the Hydroelectric Dam (+6), and the renewables (Solar, Wind and Offshore Wind Farms +2, Geothermal Plant +4). AI builds plants where demand goes unmet; the HUD shows a city's power.
2. **Carriers and airstrips:** the Aircraft Carrier carries aircraft (2 slots) and the Airstrip improvement (3 slots, Military Engineers) gives aircraft a base in the field.
3. **Nuclear weapons:** the Manhattan Project and Operation Ivy, building Nuclear and Thermonuclear Devices, delivery by bombers, missile silos or nuclear submarines, blast damage and fallout; the AI keeps them as deterrence and answers in kind.

## Decisions (Claude's recommendations; James gave standing consent)

- Milestone 1 as built: generated `requiredPower`, `poweredYields`, `poweredAmenities`, `burns` (resource and power per unit: Coal 4, Oil 4, Uranium 16) and `powerProvided` (Hydroelectric Dam 6; Solar, Wind and Offshore Wind Farms 2, Geothermal Plant 4) on buildings and improvements. `Game::burnPower` (start of the civ's turn) sets each city's `powerDemand` and `powerSupply` (save version 37): free sources first, then the civ's plants within 6 tiles burn just enough stockpile, nearest first, with their CO2 (so CO2 now follows real demand, replacing the flat burn of one fuel per plant). A fully powered city gets its buildings' powered yields and amenities; a short one loses production up to POWER_MAX_PRODUCTION_MODIFIER_PENALTY (-50%) in proportion to the shortfall. Not modelled: converting or decommissioning plants, Cardiff, Synthetic Technocracy, Reyna's Renewable Subsidizer, nuclear accidents, the Terrestrial Laser Station's demand. AI: plants valued by the shortfall within reach when fuel is on hand, power-hungry buildings discounted where no power will come, Builders lay renewables in short cities, Carbon Recapture only once the world is warming. Unreal: the city line shows power. A Deity all-AI game now reaches climate phase I (+0.5 degrees) before turn 400.
- Power is computed for each civ at the start of its turn: local sources first, then the nearest plant cities burn just enough of their stockpile to cover each remaining shortfall.
