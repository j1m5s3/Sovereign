# Sovereign live battles and the trained battle AI

The real-time battle a leader fights in (leader doc §9) and the battle AI that commands
the other side (leader doc §10, layer 3; §11). Plain C++17 with no Unreal and no rules
core: the trainer and the tests run it headless, and Unreal compiles the same sources in
the `SovereignCore` module (`tools/check_unreal_core_module.py`) and draws it.

A live battle is an input to the core, never part of its deterministic simulation (engine
doc, Core foundations), so this code may use floats. The core computes the Civ result,
and whatever the battle reports is clamped to the ±25% band, so tactics shift results but
cannot overturn the numbers.

## Build, test, train

`core/CMakeLists.txt` adds this directory, so the core build commands in
[core/README.md](../core/README.md) also build `battle_tests` and `battle_train`, and
`ctest` runs the battle tests.

```bash
core/build/battle/battle_train --out data/battle_ai/commander.txt   # about 7 minutes on 16 threads
```

Training is deterministic for a seed and thread count and uses no third-party code.

## How a battle works

| Piece | Where |
|---|---|
| Soldiers follow the unit's HP (one per 10 HP) in three squads (left, centre, right); an escorted leader rides behind, an unescorted leader fights alone | `Sim::start` |
| Blows follow the Civ strength difference (e^(0.04 x diff)); flank +6 and rear +10, a braced holding line −5 to its attacker, the leader's aura +3 within 6 m | `Sim::swing`, `Sim::bonusAgainst` |
| Orders per squad: advance, hold (stand on an anchor and meet whoever enters the zone), flank left or right (go round the enemy's widest man, then attack), fall back (give ground, striking back), hunt the leader | `Sim::step` |
| A side routs below a quarter of its men; a lone leader that falls ends it; the time cap settles a stalemate | `Sim::step` |
| The result as HP lost by each unit and the escorted leader's wound | `Sim::result` |

## The battle AI

- **Commander:** gives each of its three squads one order every second (`commander.h`). `Commander::fixed` is a scripted baseline; `Commander::trained` runs the network; a temperature above 0 samples orders instead of taking the best, for weaker difficulty levels.
- **Network:** 49 observations (each squad's strength, health, position, engagement; the enemy squads' positions and movement; both leaders; strength gap, time, totals) in the side's own frame, so one network plays either side; 32 tanh hidden units; 18 outputs (six orders for each squad). About 2,200 parameters, our own forward pass. ONNX Runtime can replace the forward pass behind the same interface if the network grows.
- **Training (`tools/battle_train.cpp`):** evolution strategies with antithetic pairs, rank-normalised fitness and Adam. Each generation plays every perturbation through the same random matchups (`randomScenario`) on both sides, against the five scripted styles (advance, hold, flank left, flank right, hunt the leader) and earlier versions of itself. It keeps the version with the best worst case against the scripted styles: the goal is a commander no simple plan beats, not one that farms a single baseline.
- **Shipping:** `data/battle_ai/commander.txt` (plain text with a versioned header). If it is missing or does not match the build, the enemy advances (the scripted baseline) and Unreal logs a warning.
- **Player modelling** (leader doc §10, layer 2): `Sim::habits(side)` measures how a side fought (shares of flanking, falling back and hunting the leader, and of time its leader spent in front); the core keeps the human's numbers in its play profile. `Counter` adjusts the commander's orders against those habits (hold the flanks, hunt a leader who fights in front, press a retreater) without retraining.

## Tests (`tests/`)

The simulation: soldiers and squads follow HP, numbers decide most fights, the same seed repeats, a lone leader fights alone, flanking beats a frontal attack on a braced line, falling back saves men, the human's leader moves by hand, each side's habits are measured. The commander: counters adjust the orders, the policy file round-trips, no weights falls back to the baseline, the trained commander ships, its worst result over the scripted styles beats every scripted commander's worst, it punishes a passive enemy, and it rarely wins against a much stronger force.
