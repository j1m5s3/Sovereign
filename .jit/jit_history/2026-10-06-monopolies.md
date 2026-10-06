# Note: the Monopolies and Corporations mode (step 6, "everything else")

Status: done, 2026-10-06. Chosen by Claude under James's standing consent. Spec: 07-economy-trade-great-people (Monopolies & Corporations, an outline only: "after Economics, Industries are created on improved luxury tiles and later upgraded to Corporations, which produce Products (Great Works) for tourism. Keep optional."). The values are Sovereign's own.

## Built

- `GameSetup::monopolies` (save version 69; `sovsim --monopolies`; Unreal `-SovMonopolies`). Off by default.
- **Industry:** after Economics, a Builder on an improved luxury the player owns founds one (command `BuildIndustry`, one charge). Only one is allowed per luxury type per player. It is kept as `Plot::industry` beside the luxury's improvement and gives +2 Gold on its plot and +10% Gold in its city.
- **Corporation:** after Electricity, the same command grows the Industry into one. It gives +4 Gold and +2 Production on the plot and +20% Gold in its city.
- **Monopoly:** owning 60% or more of a luxury's improved sources in the world, and at least 2, gives +3 Gold (in `goldPerTurn`) and +2 Tourism a turn for each of those sources (`hasMonopoly`, `monopolySources`).
- **AI:** Builders found Industries and Corporations where they can, and walk to such plots first.
- **Unreal:** the builder chooser (`B`) offers it.

## Not built

Products, the Corporations' Great Works for tourism, and each luxury's own Industry effect from Civ VI.

## Seen

An all-AI game (seed 11, 6 civs, Small, 300 turns, `--monopolies`) founded 5 Industries and had 19 luxury sources under a monopoly. Nobody reached Electricity, so there were no Corporations.
