# Sovereign

A Civilization VI-style 4X game (UE5 front end, separate C++ rules core). Development has not started; the repo holds the design specs.

Before reading any spec, read [.jit/JIT_INDEX.md](.jit/JIT_INDEX.md): it maps each topic to the files and sections to open, so you don't load all ~207k tokens of `specs/`. Key rules from it:
- `specs/sovereign/` overrides `specs/civ6/` where they differ.
- `specs/civ6/data/*.md` are generated lookup tables: grep for the row you need, never read one whole, never edit by hand (change `tools/civ6_extract` instead).
- When you change a doc, update `.jit/JIT_INDEX.md` if a file, section or decision it lists moved or changed.
