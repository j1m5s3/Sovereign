# civ6_extract

Builds the reference tables in `specs/civ6/data/` from a **local** install of
Sid Meier's Civilization VI. Nothing from the game is stored in this repository:
the scripts read the install at run time, rebuild the rules database in a local
SQLite file, and write Markdown tables whose effect text is generated from the
game's modifier/requirement data in our own wording.

## Requirements

- Python 3.10+ (standard library only).
- Civilization VI with Rise and Fall and Gathering Storm installed. Other
  installed DLC (civ packs, leader packs) is picked up automatically.

## Usage

```bash
python build_db.py "C:\Program Files (x86)\Steam\steamapps\common\Sid Meier's Civilization VI" civ6_gs.sqlite
python extract.py civ6_gs.sqlite ../../specs/civ6/data
```

`build_db.py` writes `civ6_gs.sqlite` plus a `.log` listing every row the game
data could not apply (for example rows referring to content that is not
installed). Both are build artefacts and are git-ignored; do not commit them.

## How the database is rebuilt

1. Runs `Base/Assets/Gameplay/Data/Schema/*.sql`, then every base gameplay XML
   file, inside one transaction with deferred foreign keys (the game validates
   references only after loading).
2. Reads every DLC `.modinfo`, keeps `<UpdateDatabase>` / `<UpdateText>`
   actions whose criteria hold for a Gathering Storm game (`RULESET_EXPANSION_2`
   / `GameCoreInUse Expansion2` / `Expansion2_Players` leaders / installed
   mods), and skips optional game modes (`ConfigurationValueMatches`) and
   scenarios. Gathering Storm ships its own copy of the Rise and Fall core data;
   from `DLC/Expansion1` only the actions whose criteria also match Gathering
   Storm (the R&F civs and leaders) are applied.
3. Applies XML `<Row>`, `<Replace>`, `<Update>` and `<Delete>` operations and
   SQL files in load order (Gathering Storm before other packs, then each
   action's `LoadOrder` and file `Priority`), so later layers override earlier
   ones exactly as in game.
4. Adds the effect/collection types the engine registers itself, then drops
   any rows whose references remain unsatisfied (logged).

## Output

`extract.py` writes one Markdown file per system plus `README.md` (index) to the
output folder. `effects.py` turns each `Modifier` into a sentence such as
`+1 Production in all your cities where city has a garrison`; effect types
without a template fall back to a readable form of the effect name and its
arguments. Proper names (units, buildings, civs...) are the only strings taken
from the game's English text.
