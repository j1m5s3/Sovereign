# Sovereign

A Civilization VI-style 4X game (UE5 front end, separate C++ rules core). The repo holds the design specs (`specs/`) and the C++ rules core (`core/`, see `core/README.md`); development follows `.jit/JIT_PLAN.md`.

## Required before reading any spec

1. Read [.jit/JIT_INDEX.md](.jit/JIT_INDEX.md) first, every session. It maps each topic to the files and sections to open. Do not open, list-read or summarize files under `specs/` until you have read it.
2. If [.jit/JIT_PLAN.md](.jit/JIT_PLAN.md) exists and is active, read it next and work from it.
3. Then open only the files and sections the index points to for your task. Reading all of `specs/` (~207k tokens) or a whole `specs/civ6/data/*.md` table needs a stated reason.

## Rules from the index
- `specs/sovereign/` overrides `specs/civ6/` where they differ.
- `specs/civ6/data/*.md` are generated lookup tables: grep for the row you need, never edit by hand (change `tools/civ6_extract` instead).
- When you change a doc, update `.jit/JIT_INDEX.md` if a file, section or decision it lists moved or changed.
