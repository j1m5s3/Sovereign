# Plan: Military Engineers' works: railroads and Mountain Tunnels (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-governor-promotions.md`. Chosen by Claude under James's standing consent: the routes and tunnels 01-map-and-terrain describes and the core left out ("railroads need Military Engineers, not modelled yet"; "the Mountain Tunnel waits for mountain movement"). One milestone, one PR.

Specs: 01-map-and-terrain (Routes, Mountain tunnels); data/terrain-features-resources (Routes); data/improvements (Mountain Tunnel); 05-units-and-combat (movement).

## Milestone

1. **Done: railroads and tunnels:** the Railroad route (Steam Power; laid by a Military Engineer on its plot for 1 Iron and 1 Coal; 0.25 movement along it, bridging rivers); the Mountain Tunnel (Chemistry; built by a Military Engineer into a neighbouring mountain, which becomes passable at a flat move's cost). AI builds an engineer and lays track from its capital to its other cities; the Unreal build chooser offers both.

## Decisions (Claude's recommendations; James gave standing consent)

- Data: the generator now keeps the Railroad row (`unitOnly`, `tech`, `resourceCost`) and the Mountain Tunnel (`tunnel`); roads chosen by era (`roadFor`, trade routes' roads) skip the railroad, so it is only laid by hand. Route indices stay in era order with the railroad last.
- `BuildRailroad` (command 54): a Military Engineer with moves on a land plot that is its owner's, unowned, or an enemy's, with Steam Power and the resources in stock; it takes the engineer's turn and no build charge (data: charge cost 0). Laying in a neighbour's land at peace is refused (Sovereign reading).
- A Mountain Tunnel is a `BuildImprovement` with the mountain as the target (`Command::buildTunnel`), on a neighbouring mountain that is the player's or unowned; it spends a build charge like any improvement. `isLandPassable` lets units through a tunnelled mountain, and entering it costs 1 (Sovereign reading of "passable"). The tunnelled mountain is still unworkable.
- AI: one engineer at a time once it has Steam Power, at least three cities and 4 Iron and 4 Coal in stock; the engineer walks to the nearest unlaid plot on the straight lines from the capital to each other city and lays track there. It does not tunnel. In the first 6-AI Deity test game no AI laid track: their Coal plots sat unmined and few had the Armory the engineer needs. A follow-up AI pass fixed that: Builders weigh strategic resources +30, skip plots where only engineer improvements fit (a Builder used to stand on such a plot for good), and once Steam Power is in, the first Encampment (140) and Armory (+200) are valued; the same test game then laid 37 railroad plots with 2 engineers.
- Not modelled: railroads' production bonus to adjacent districts (none in the data). Pillaging and repairing routes followed in `2026-10-06-pillage-depth.md`.

## Follow-up, 2026-10-06: charges toward districts

- A Military Engineer on its civ's Aqueduct, Canal or Dam that is still being built may spend a charge to add 20% of the district's cost to its city's progress (03; data: districts.md, build-charge production; generated `chargeProduction`). `Command::contributeCharge` (`CommandType::ContributeCharge` = 66), `Game::chargeProblem`. The AI's engineers do it first when they stand on such a district; Unreal: the engineer's build chooser offers it. Rome's Bath is in the table but not in the roster's data.
