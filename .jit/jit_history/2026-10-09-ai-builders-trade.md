# Record: AI Traders fill their routes; Builders embark for land across water

Status: done, 2026-10-09. Previous: `jit_history/2026-10-09-builder-value.md` (and the sea-improvements follow-up it left open). Chosen under James's standing consent: the AI's use of Builders and trade routes was still weak.

## What was wrong

On origin/main (32 AI games, Small, 6 AI), at turn 200 each major civ ran 2.5 of 2.7 trade routes, with 1.0 idle Trader, and still held 15.5 improvable plots (3.7 of them worked) with 13.9 in reach and no Builder heading there. Causes, from the leftover diagnosis and from `sovsim --diag`:

- **Land across water.** `build` sent Builders to land plots over land only, so an island in a city's three rings stayed unimproved after Sailing (`2026-10-09-sea-improvements.md` left this open). Sea resources were already reached by embarking.
- **New civilians waited a turn.** `playTurn` ordered existing Builders and Traders before `production` and `purchases`, so a unit trained or bought this turn sat until next turn.
- **Traders beside a city on unowned land never started a route.** `approach(..., false)` treats distance 1 as done without moving. `tradeOrigin` needs the city center or owned land beside it, so a Trader on unclaimed land next to the city returned every turn without starting (`originOf` in trade.cpp).
- **Traders with nowhere to go.** A city trained a Trader from Foreign Trade's first slot before a second city or a revealed foreign city existed.
- **Capacity stayed at one** until late: Markets and Commercial Hubs were not valued once the first route was running. An earlier pace study dropped valuing them while routes were few; this waits until the slots are full.

Tried and left out: valuing a Builder by all improvable plots beyond the charges (`work - charges` instead of `workedWork - charges`). That cut leftover plots (16 to 7.4 at turn 200 on 8 seeds) but lost science and broke `ai_counters_a_cavalry_neighbour_with_pikes` (a Builder outranked the Spearman).

## Fix

- `build`: a land plot is tried over land, then by embarking; after a failed move, reach includes both ways, so land a city holds across water is in reach after Sailing.
- `playTurn`: after `purchases`, `survey` and send any Builder or Trader that still has moves (those trained or bought this turn).
- `trader`: walk onto the nearest city (then beside it if the center is blocked); if that yields an origin and moves remain, start the route the same turn.
- `tradeDestOpen` / `tradersOnHand`: no Trader while there is no destination; count those in training too.
- `production`: +100 per `tradeCapacity` on a building, and +100 on a Commercial Hub or Harbor, while every slot is filled (`needCapacity`).
- `purchases`: with spare gold, buy a Builder (no extra reserve) in the city that works the most unimproved plots, before other gold sinks, while those plots outnumber the charges on hand; buy a Trader while a slot is open, keeping the usual reserve.

`sovsim --diag N` prints per-civ Builder and trade-route counts at the pace checkpoints.

## Results

32 seeds (Small, 6 AI, turn 200), each game set against the same seed on origin/main:

| Build | cities | pop | techs | civics | era | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|---|
| main | 9.1 | 67.2 | 34.3 | 23.4 | 3.7 | 139.7 | 80.0 | 187.8 | 209 |
| this | 9.0 | 67.0 | 34.6 | 23.8 | 3.7 | 139.2 | 81.0 | 184.1 | 238 |

Turn 150 science 67.6 to 73.8. Trade at turn 200, per major civ: capacity 2.7 to 4.3, routes 2.5 to 4.0, Commercial Hubs 1.7 to 2.6, Markets 1.0 to 1.9. Worked unimproved plots 3.7 both ways; improvable plots 15.5 to 16.2 (the island fix is a small share of that leftover). Idle Traders at turn 50–100: 0.2–0.4 to 0.1–0.2.

## Tests

- `ai_builders_work_what_they_can_reach`: after Sailing the Builder embarks for the island's Wine instead of farming the mainland.
- `ai_builders_embark_for_sea_resources`: with no island it still puts Fishing Boats on Pearls; with Horses on the island (now in reach) it improves those instead.
- `ai_does_not_train_traders_with_nowhere_to_go`: Foreign Trade and one city, nobody else revealed: no Trader queued.
- `ai_traders_walk_into_the_city_to_start_a_route`: a Trader on unowned land beside the capital walks in and starts a route the same turn.
- `ai_buys_a_builder_when_plots_wait`: two spent Builders fill the training floor; with 400 Gold the city buys one, without Gold it does not.
