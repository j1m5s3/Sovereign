# Record: start plots go to the players in a shuffled order (rules fix)

Status: done, 2026-10-09. Found by Claude: a survey of how all-AI games end showed the first players winning far more often than the last, whichever civs sat there.

## What was wrong

- **The first seat always had the best start.** `chooseStartPositions` (mapgen.cpp) scores every plot that may hold a start, picks the best-scoring plots far enough apart, best first, and gave the first to player 0, the second to player 1 and so on. Player 0 is the human's seat in a game with one human.
- In 64 AI games (Small, 6 AI, seeds 1-64), the start scores averaged 275 for player 0 down to 220 for player 5, and by turn 150 player 0's science was 64 against 40 for player 5 (48 games). With the civs' order rotated by three, player 0 still made 65 and the seats after it less, so the seat, not the civ, made most of the gap.

## Fix

- The chosen plots, still picked best first, go to the players in an order shuffled with the map generator's random numbers (`RngStream::MapGen`). Everything else about the map stays as it was; the city-states, natural wonders and tribal villages placed after the starts come out differently, since the shuffle draws first.
- `startScore(state, rules, plot)` (mapgen.h): the start score `chooseStartPositions` already used (the food x3, production x2 and gold of the plots within 2, 2 for each passable land plot within 3, 15 for fresh water, 6 for a coast), now its own function so a test can rank the starts.

## Sovereign readings

- The spec (01, Map generation, step 7) scores start plots but does not say which player gets which; Sovereign hands them out in a random order.

## Results

- The same 64 games with the shuffle: the seats' start scores average 241 to 248, and by turn 150 their science follows the civ sitting there (Rome 74, the other five 49 to 59) rather than the seat.
- 128 AI games (Small, 6 AI, turn 200) against main: the averages over all civs do not change past noise (science +2.6 ± 1.7, production +1.4 ± 1.9, culture +1.0 ± 0.9, cities +0.07 ± 0.07, population +0.7 ± 0.6). Each game is a different one, as the civs start elsewhere.
- Speed: only game creation changes (one shuffle of the chosen plots).

## Tests

- `map_the_best_start_goes_to_any_seat`: in 24 games (Small, 6 players), player 0 holds the best-scoring start in fewer than half, and player 5 in at least one (7 and 2 of 24 with the shuffle; 24 and 0 on main).
- `ai_soak_takes_a_capital_and_replays` now uses seed 56: on seed 83 no capital fell within 250 turns once the starts moved.
- The golden file's state hash changes; the rules checksum does not.

## Left open

- Start quality still decides a lot within a game. In the 64 games with the shuffle, by turn 150 the civ with the best start made about 16 more science than its civ and its game would suggest, and the two with the worst starts 7 to 8 less (on about 56). The spec's "guarantee minimum food/production; balanced starts add resources" (01, step 7) is not modelled.
- The civs are not even: Rome (Colonia: +1 population and a free Monument in each new city) made 74 science at turn 150 against 49-59 for the other five in the same 64 games, and won 8 of 36 games to turn 500. A balance question for the civs' design, not changed here.
- 35 of 36 all-AI games to turn 500 (24 Small at Prince, 12 Standard at Immortal) ended in a Science victory (turns 268-464), one in a Religious one. No civ came near a Culture victory: in 4 traced games, the civ with the most visiting tourists had 67 to 181 at its last trace (turn 350 or 400), against 314 to 510 domestic tourists for its strongest rival. The most Diplomatic Victory points by turn 400 were 13 of 20.
