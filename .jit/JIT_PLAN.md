# Plan: online play (step 6, item 6)

Status: active, 2026-10-05. Previous: `jit_history/2026-10-05-llm-diplomacy.md`.

Goal: humans play one game across machines. Specs:
- engine-and-architecture, Core foundations:
  - lockstep over the one command log;
  - non-deterministic parts are inputs;
  - live scenes online;
  - Steamworks first, Epic Online Services later.
- 10-ai-ui-implementation, Multiplayer:
  - lockstep with command broadcast;
  - desync detection by state hashing;
  - turn modes and timers;
  - hot seat.
- world-scale-and-generation: each machine builds its own world from the same state.
- open-gaps-review, gaps 1 and 3: one machine judges a live scene, and its result enters as a command.

## Milestones (one PR each)

1. **Done (this PR): The session protocol (`net/`, plain C++ like `battle/`, over the core):**
   - Messages:
     - hello: protocol version, rules checksum, mod list;
     - seat table and game start (the setup and seed, or a save);
     - commands with sequence numbers;
     - per-turn state hashes;
     - chat, leave.
   - The host orders every command. Clients send theirs to the host, and the host validates it against its own core before broadcasting it in order. Every machine applies the same stream.
   - AI seats are played on the host and broadcast as commands.
   - Desync: each machine reports its state hash when the world turn wraps. On a mismatch, the host sends its save and the others reload.
   - A dropped player's seat goes to the AI until they rejoin from the host's save.
   - Behind a `Transport` interface, with a loopback transport for tests and a TCP transport for LAN and direct IP. Tests run 2–4 sessions in one process, through whole games, a desync and a rejoin.
2. **Unreal: host and join.**
   - A lobby (direct IP or LAN): seats, civs, ready.
   - The game starts from the shared setup, and the controller plays its own seat. Other humans' turns show as "waiting for ...".
   - Chat, a desync banner with automatic resync, and disconnect handling.
   - Hot seat on one machine (several human seats; the screen hands over between turns).
3. **Steam lobbies and invites** through Unreal's OnlineSubsystemSteam (development App ID 480 until Sovereign has its own). The `net/` stream runs over Steam's networking. Epic Online Services is a follow-up.
4. **Live scenes online:**
   - The machine of the human whose leader is in the fight hosts the live battle. An opposing human joins it through Unreal replication for that scene only; otherwise the trained battle AI leads that side.
   - The host submits the one `BattleResult` command into the stream, and every core clamps it to the band.
   - The other players wait, and can auto-resolve after a timeout.

## Decisions (Claude's recommendations; James gave standing consent)

- Turns stay sequential, as the core plays them. Simultaneous and dynamic turn modes need the core to accept commands from several players at once, so they come after milestone 4.
- The host is the order authority, not the rules authority: every machine runs the full core and rejects what it rejects. A cheating host can only reorder or drop commands, not invent state.
- Milestone 1 as built: one message type set (Hello, Welcome with the save, Refuse, Seats, Submit, Apply, Refused, Hash, Resync, Chat). Commands travel in the save format's encoding (`encodeCommand`, now exported). The game always reaches a client as a save, so start, rejoin and resync are one path. Players outside the seat table (city-states) are the host's AI. `Game::stateMutForTests()` exists only to force a desync in tests. A three-process TCP run with `sovnet` matched hashes after 60 turns.
- The language model runs on the speaking player's machine. Only its deal proposal and summary travel (already commands).
