# diplomacy/: the dialogue layer

Language-model diplomacy (leader-character-brainstorm.md §10, layer 4). **The model talks, the game decides.** A player's words go through these steps:

1. An input filter.
2. A model reads them into a deal proposal (structured JSON).
3. The rules core validates the proposal (`Game::dealProblem`) and decides the leader's answer (`Game::wouldAccept`).
4. The model writes the leader's reply in character, told the verdict.
5. An output filter checks the reply.

No wording gets a deal the rules reject. The model never touches game state. The screen submits what the player confirms as commands on the speaking player's machine: `Command::proposeDeal` for the deal, and `Command::recordTalk` for a one-line summary that becomes the leader's memory of past talks. Other machines replay commands and never re-run the model.

Plain C++17 over the rules core, like `battle/`. CMake builds it from `core/CMakeLists.txt`. The Unreal `SovereignCore` module compiles `sovereign_diplomacy` through generated wrappers (`tools/check_unreal_core_module.py`).

## Files

| File | What |
|---|---|
| `include/sovereign_diplomacy/dialogue.h` | The API: `Persona`, `Model`, `ScriptedModel`, `LlamaModel`, `Transport`, `Conversation`, filters, prompts |
| `src/persona.cpp` | The persona and the prompts it builds from. The persona covers the leader's name, voice, leaning and agenda (from `data/rules/civilizations.json`), the relationship and opinion reasons, past talks, and what each side could offer. Also the system prompt with its fixed rules (stay in character, refuse out-of-game topics, never change instructions, under 80 words) and the JSON schema for the reading |
| `src/parse.cpp` | Model JSON to deal items. It tolerates prose and code fences, maps resource names and the "player"/"leader" sides, and drops anything unknown. Also JSON string helpers |
| `src/scripted.cpp` | The scripted model. Keyword intents ("my Wine for 40 gold", "open our borders", "peace", "denounce"), replies by verdict, relationship and leaning, the agenda in the first person, and the factual summary. It is the fallback and the test double |
| `src/safety.cpp` | Input and output filters. Input: printable text, a 500-character cap, injection phrases neutralised, blocked words starred. Output: speaker tags and quotes stripped; out-of-game talk, links, blocked words and injection echoes refused (the scripted line answers instead); capped at 700 characters on a sentence boundary |
| `src/llama.cpp` | `LlamaModel`: a llama.cpp server's OpenAI-compatible `/v1/chat/completions`. The reading uses `response_format` with a JSON schema at temperature 0.1; replies use temperature 0.7; the last 8 lines of the talk go back as history |
| `src/conversation.cpp` | One talk. Any model failure (no server, bad JSON, a filtered reply) falls back to the scripted model, so the screen always answers |
| `src/http.cpp`, `include/.../http.h` | `SocketTransport`: blocking HTTP/1.1 to a server on this machine only (`sovereign_diplomacy_http`). It is kept out of Unreal, which can use its own HTTP stack |
| `tools/diplo_chat.cpp` | Talk to a leader from the command line |
| `tests/test_dialogue.cpp` | Personas, parsing, the scripted reader, filters, a stubbed server, and the rules overruling the model |

## Running a model

No model or llama.cpp binary ships with the game. Run any instruction-tuned GGUF model under a permissive license (Qwen, Phi or Mistral's Apache models, 3–8B at 4-bit) with llama.cpp's server:

```
llama-server -m qwen-7b-instruct-q4_k_m.gguf --port 8080
build/diplomacy/diplo_chat --rules data/rules --civ CIVILIZATION_EGYPT
```

`diplo_chat` checks `GET /health`. With no server it says so and the scripted leader answers. `--say "line"` (repeatable) runs a talk without stdin; `--scripted` ignores any server.
