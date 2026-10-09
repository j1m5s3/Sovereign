# Record: fogged plots keep their last-seen state (retention and world-scale audit, part 3)

Status: done, 2026-10-09. Previous: `2026-10-09-retention-audit-2.md` (part 2, mounts). Part 1 (`2026-10-09-retention-audit.md`) lists the audit's findings; this part does the last one in the core. Chosen by Claude under James's standing consent.

## Change

- **Revealed plots show what the player last saw** (world-scale doc: "revealed hexes use their last-seen state"). Before, a revealed plot out of sight showed its live state: a rival city's name and size, new or removed improvements and districts, all through the fog.
- When a plot leaves a human player's sight (Visible to Revealed in `Game::refreshVisibility`), the core snapshots it as a `PlotMemory` in `Player::seen`: terrain, feature, improvement, owner, route and pillage state, village, antiquity, district or wonder and their state, and a city center's owner, name, population and capital flag.
- `Game::lastSeen(player, hex)` returns the snapshot for a revealed plot, and nullptr when the plot is in sight, unrevealed, or was revealed without being seen (map trades, for instance); then draw the live plot.
- Human players only: AI players keep no memory, so AI games and their pace and speed are unchanged.
- The memory is saved (save version 96), only for plots with a snapshot.

## Checks

- Test `fogged_plots_keep_their_last_seen_state`: a rival city's name and size, a farm and a campus keep their old state after changing out of sight; nothing for unrevealed plots or AI players; the memory survives save and load; seen again the plot is live, and leaving sight again takes a fresh snapshot.
- Soak (8 long games with save/reload cuts): no crash, replay mismatch or reload drift; the games play out exactly as before (same turns, cities, units and commands), only state hashes differ, since saves now hold the memory. Timing unchanged.
- Mutation: 10 mutants, all caught (one after the test gained turns spent out of sight).

## For the Unreal map (PC)

- `SovMirror.cpp` still reads live plots for revealed hexes. It should use `Game::lastSeen` where it returns a snapshot, for terrain dressing, improvements, districts, wonders, borders and city banners.
