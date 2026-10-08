# Sovereign mods

A mod is a folder of rules data that the game lays over its own rules (`data/rules/`). This is the launch scope decided in `specs/sovereign/player-retention.md` §6: data and modifier mods (new rules, balance changes, civs that reuse existing art). Asset mods and scripting come later.

## Where mods live

- `mods/` in this repository: the examples that ship with the game.
- `Saved/Sovereign/Mods/` beside the Unreal project: your own.

Each mod is a folder with a `mod.json` and any of the rules files the game reads (the names in `Rules::fileNames()`, such as `units.json`, `buildings.json`, `civilizations.json`, `globals.json` or `modifiers.json`).

```json
{
 "id": "swift-settlers",
 "name": "Swift Settlers",
 "version": "1.0",
 "description": "An example balance mod: Settlers cost 60 production instead of 80."
}
```

Turn mods on or off from the main menu (`Mods`). New games use the mods that are on, in the order the menu lists them. A saved game remembers its mods and loads only when they are all installed. Online, every machine needs the same mods: the host refuses a player with a different list.

## How a mod changes the rules

Rules files hold tables of rows, each with an `id`. A mod's row with the same `id`:

- **replaces** the row, when it is a whole row;
- **patches** it, with `"patch": true`: only the fields the mod gives change;
- **removes** it, with `"delete": true`.

A row with a new `id` adds to the table. Values in `globals` replace the same-named global.

```json
{
 "units": [
  {"id": "UNIT_SETTLER", "patch": true, "cost": 60}
 ]
}
```

The rules checksum covers every layer, so saves and online games notice a changed mod.
