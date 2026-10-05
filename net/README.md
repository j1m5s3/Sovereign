# net/: online play

The lockstep session over the core's one command log (engine-and-architecture, Core foundations; `specs/civ6/10-ai-ui-implementation.md`, Multiplayer).

- **Every machine runs the full rules core.**
- **The host orders commands.** A client sends its command to the host. The host applies it to its own core and, if the core accepts it, broadcasts it as the next numbered entry of the log. Every machine applies the same stream in the same order. The host is the authority on order, not on rules: each core still judges each command, so a cheating host can drop or delay commands but not invent state.
- **AI seats run on the host** and go out as commands like anyone's. So do city-states, and human seats whose player has dropped. The host also settles a live battle that waits for a seat nobody holds.
- **Desync repair.** When a world turn begins, each client sends its state hash. If it differs from the host's hash for that turn, the host sends its save and the client reloads it. A client whose core refuses a command the host accepted, or that sees a gap in the numbering, asks for the save at once.
- **Joining.** Players claim seats in the lobby. Seats nobody claimed are the AI's for good. After the start, a free human seat (a dropped player's) can be rejoined: the host sends its save.
- **Join checks.** A join needs the same protocol version, the same rules data (checksum) and the same mod list.
- **Not yet.** Turns are sequential, as the core plays them. Simultaneous and dynamic turn modes come later (see `.jit/JIT_PLAN.md`).

Plain C++17 over the core, like `battle/` and `diplomacy/`. CMake builds it from `core/CMakeLists.txt`. The Unreal `SovereignCore` module compiles `sovereign_net`, but not the TCP code, through generated wrappers.

## Files

| File | What |
|---|---|
| `include/sovereign_net/session.h` | `Link` and `Listener` (the transport interface), `LoopbackListener`, the messages, `Host`, `Client`, `aiCommands` |
| `src/protocol.cpp` | Message encoding (commands in the save format's encoding), loopback links, `aiCommands` (the AI's turn worked out on a copy, so a seat's orders can be sent rather than applied) |
| `src/host.cpp` | Seats and joins, ordering and broadcasting, AI seats, per-turn hashes and resyncs, drops |
| `src/client.cpp` | Joining, applying the stream, hash reports, resyncs, refusals, chat |
| `src/tcp.cpp`, `include/.../tcp.h` | `TcpListener` and `tcpConnect`: non-blocking sockets with length-prefixed frames (`sovereign_net_tcp`) |
| `tools/sovnet.cpp` | A networked game between processes, each human seat played by the AI standing in for its person |
| `tests/test_session.cpp` | Lobby and start, 40 turns in lockstep, city-states, refused orders, a forced desync repaired, a drop played by the AI and rejoined, mod checks, chat, TCP |

## In the game

`unreal/README.md` (Controls, "Online" and "Hot seat") covers hosting and joining from the Unreal game. The game's own links (`unreal/Source/SovereignBridge/Private/SovNetLink.*`, engine sockets) use the same framing as `sovereign_net_tcp`, so `sovnet` and the game can share a session.

## Try it

Run the host and two joiners, each from its own shell:

```
build/net/sovnet host --rules data/rules --port 7777 --players 4 --humans 3 --turns 60
build/net/sovnet join --rules data/rules --port 7777 --name Ann --turns 60
build/net/sovnet join --rules data/rules --port 7777 --name Bob --turns 60
```

Each prints its state hash at the end. In a 2026-10-05 run, all three ended turn 60 with 1,797 commands and the same hash, with no resyncs.
