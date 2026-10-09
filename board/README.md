# board/: weekly challenge server

The server side of the weekly challenge (player-retention §3). Everyone plays the same week's
setup; a finished game is saved as `challenge week N` and submitted here. The board checks the
save with `checkChallenge` (the week's setup, a replay of the log, live-battle results inside the
band) and keeps a ranked list per week on disk.

Valid games rank by the goal met, then fewer turns, then a higher score. A later submit under the
same name keeps the better result.

Plain C++17 over the rules core, like `net/`. CMake builds it from `core/CMakeLists.txt`. The
TCP code is a separate library (`sovereign_board_tcp`) so Unreal can later call the client API
with its own sockets. This folder is not compiled into Unreal yet.

## Files

| File | What |
|---|---|
| `include/sovereign_board/board.h` | `Board` (check and disk ranking), `Link`/`Listener`, messages, `Server`, `Client` |
| `src/board.cpp` | `checkChallenge`, `week-<n>.txt` on disk |
| `src/protocol.cpp` | Message encoding, loopback links, request-response server and client |
| `src/tcp.cpp`, `include/.../tcp.h` | `TcpListener` and `tcpConnect`; `submitSave` / `fetchRanking` (`sovereign_board_tcp`) |
| `tools/sovboard.cpp` | Serve the board, submit a save, print a week |
| `tests/test_board.cpp` | Accept, reject (wrong week or tampered), ranking, TCP |

## On disk

Each week is a text file in `--data` (default `board-data`):

```
sovereign-board 1
week 7
Alice 1 12 450
Bob 0 12 300
```

A player name is 1–32 characters: letters, digits, `_`, `-`, `.`.

## Try it

```
build/board/sovboard serve --rules data/rules --port 7788 --data board-data
build/board/sovboard submit --save "challenge week 7" --week 7 --name Alice --port 7788
build/board/sovboard list --week 7 --port 7788
```

On Windows with Visual Studio the tools sit in `build/board/Release/`. `Client` and `submitSave`
are the calls a game can make later without this CLI.
