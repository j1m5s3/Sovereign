# Record: weekly challenge server board (player-retention §3)

Status: done, 2026-10-09. Previous: Plan C item 6 in `.jit/JIT_PLAN.md` built the local parts (`weeklyChallenge`, `checkChallenge`, the menu save to submit) and left the server board unbuilt. Spec: `specs/sovereign/player-retention.md` §3.

## Built

- **`board/`**, plain C++17 over the core, like `net/`. `Board::submit` runs `checkChallenge` on a save for a week and keeps a ranked list per week on disk (`week-<n>.txt`: goal met, then fewer turns, then a higher score). The same name keeps its best result.
- **Sockets in `sovereign_board_tcp`**, kept out of Unreal: length-prefixed TCP, `Link`/`Listener` in the library, loopback for tests. `Client` and `submitSave` / `fetchRanking` are the calls a game can make later.
- **`sovboard`**: `serve`, `submit` a save, `list` a week.

## Tests

- `the_board_accepts_a_valid_challenge_save`
- `the_board_rejects_a_wrong_week_save`
- `the_board_rejects_a_tampered_save`
- `the_board_ranks_by_goal_then_turns_then_score`
- `the_board_keeps_the_ranking_on_disk`
- `tcp_submit_reaches_the_board`

## Left open

- Unreal: the menu still writes `challenge week N` locally; it does not call this board yet (this work does not edit Unreal).
