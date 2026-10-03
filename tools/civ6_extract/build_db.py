"""Rebuild Civilization VI's gameplay database from a local install.

Replays the same steps the game performs when it loads a Gathering Storm
ruleset game: run the base schema, load every base gameplay XML file, then
apply each DLC's <UpdateDatabase> actions whose criteria are satisfied by a
Gathering Storm game with all installed civ/leader packs and no optional game
modes or scenarios. English text is loaded into a separate LocalizedText table.

The result is a local SQLite file (never committed) that the table generators
in extract.py query.
"""
from __future__ import annotations

import glob
import os
import re
import sqlite3
import sys
import xml.etree.ElementTree as ET
import zlib

DEFAULT_GAME_DIR = r"C:\Program Files (x86)\Steam\steamapps\common\Sid Meier's Civilization VI"

# DLC folders that are scenarios or optional game modes; their data is not part
# of the standard Gathering Storm ruleset.
EXCLUDED_DLC = {
    "CivRoyaleScenario",
    "PiratesScenario",
    "BarbarianClansMode",
    "TreeRandomizer",
    "ScoutCat",
}

log_lines: list[str] = []


def log(msg: str) -> None:
    log_lines.append(msg)


def make_hash(s):
    if s is None:
        return None
    v = zlib.crc32(s.encode("utf-8"))
    return v - (1 << 32) if v >= (1 << 31) else v


def convert(v: str):
    if v is None:
        return None
    lv = v.strip().lower()
    if lv == "true":
        return 1
    if lv == "false":
        return 0
    return v


# Argument tables keep the literal text so "true" stays distinguishable from 1.
RAW_TABLES = {"ModifierArguments", "RequirementArguments"}


def elem_values(el: ET.Element, raw: bool = False) -> dict:
    """Column values from attributes plus simple child elements."""
    conv = (lambda v: v) if raw else convert
    vals = {k: conv(v) for k, v in el.attrib.items()}
    for child in el:
        if isinstance(child.tag, str):
            vals[child.tag] = conv(child.text.strip() if child.text is not None else "")
    return vals


class Loader:
    def __init__(self, db: sqlite3.Connection):
        self.db = db
        self.columns: dict[str, dict[str, str]] = {}

    def table_columns(self, table: str) -> dict[str, str]:
        key = table.lower()
        if key not in self.columns:
            cols = self.db.execute(f'PRAGMA table_info("{table}")').fetchall()
            self.columns[key] = {c[1].lower(): c[1] for c in cols}
        return self.columns[key]

    def reset_cache(self):
        self.columns.clear()

    def _fix(self, table, vals):
        cols = self.table_columns(table)
        out = {}
        for k, v in vals.items():
            real = cols.get(k.lower())
            if real is None:
                log(f"  unknown column {table}.{k}")
                continue
            out[real] = v
        return out

    def insert(self, table, vals, replace=False):
        vals = self._fix(table, vals)
        if not vals:
            return
        names = ",".join(f'"{k}"' for k in vals)
        qs = ",".join("?" for _ in vals)
        verb = "INSERT OR REPLACE" if replace else "INSERT"
        try:
            self.db.execute(f'{verb} INTO "{table}" ({names}) VALUES ({qs})', list(vals.values()))
        except sqlite3.Error as e:
            if "UNIQUE" in str(e) and not replace:
                # The game aborts the file on this; we prefer to keep going.
                log(f"  dup row in {table}: {vals} ({e})")
            else:
                log(f"  insert failed {table}: {vals} ({e})")

    def where_clause(self, table, where):
        where = self._fix(table, where)
        if not where:
            return "1=1", []
        parts, args = [], []
        for k, v in where.items():
            if v is None:
                parts.append(f'"{k}" IS NULL')
            else:
                parts.append(f'"{k}" = ?')
                args.append(v)
        return " AND ".join(parts), args

    def update(self, table, where, sets):
        sets = self._fix(table, sets)
        if not sets:
            return
        wc, wargs = self.where_clause(table, where)
        sc = ",".join(f'"{k}" = ?' for k in sets)
        try:
            self.db.execute(f'UPDATE "{table}" SET {sc} WHERE {wc}', list(sets.values()) + wargs)
        except sqlite3.Error as e:
            log(f"  update failed {table}: {where} {sets} ({e})")

    def delete(self, table, where):
        wc, wargs = self.where_clause(table, where)
        try:
            self.db.execute(f'DELETE FROM "{table}" WHERE {wc}', wargs)
        except sqlite3.Error as e:
            log(f"  delete failed {table}: {where} ({e})")

    def load_xml(self, path: str, text_mode=False):
        try:
            root = ET.parse(path).getroot()
        except ET.ParseError as e:
            log(f"XML parse error {path}: {e}")
            return
        for tbl in root:
            if not isinstance(tbl.tag, str):
                continue
            table = tbl.tag
            if text_mode:
                self._load_text_table(tbl)
                continue
            if not self.table_columns(table):
                log(f"  {os.path.basename(path)}: no table {table}")
                continue
            raw = table in RAW_TABLES
            for op in tbl:
                if not isinstance(op.tag, str):
                    continue
                tag = op.tag
                if tag == "Row":
                    self.insert(table, elem_values(op, raw))
                elif tag == "Replace":
                    self.insert(table, elem_values(op, raw), replace=True)
                elif tag == "Delete":
                    self.delete(table, elem_values(op, raw))
                elif tag == "Update":
                    where, sets = {}, {}
                    for part in op:
                        if part.tag == "Where":
                            where = elem_values(part, raw)
                        elif part.tag == "Set":
                            sets = elem_values(part, raw)
                    self.update(table, where, sets)

    def _load_text_table(self, tbl):
        name = tbl.tag
        if name not in ("BaseGameText", "LocalizedText", "EnglishText"):
            return
        for op in tbl:
            if not isinstance(op.tag, str):
                continue
            vals = elem_values(op)
            tag = vals.get("Tag")
            lang = vals.get("Language", "en_US")
            if lang != "en_US" or tag is None:
                continue
            if op.tag in ("Row", "Replace"):
                self.db.execute("INSERT OR REPLACE INTO LocalizedText(Tag, Text) VALUES (?,?)",
                                (tag, vals.get("Text")))
            elif op.tag == "Delete":
                self.db.execute("DELETE FROM LocalizedText WHERE Tag = ?", (tag,))
            elif op.tag == "Update":
                where = sets = {}
                for part in op:
                    if part.tag == "Where":
                        where = elem_values(part)
                    elif part.tag == "Set":
                        sets = elem_values(part)
                if where.get("Tag") and "Text" in sets:
                    self.db.execute("UPDATE LocalizedText SET Text=? WHERE Tag=?", (sets["Text"], where["Tag"]))

    def load_sql(self, path: str):
        with open(path, encoding="utf-8-sig", errors="replace") as f:
            sql = f.read()
        # Everything runs inside one transaction with deferred foreign keys
        # (the game validates references only after all files are loaded), so
        # execute statement by statement instead of executescript().
        for stmt in split_sql(sql):
            if re.match(r"\s*PRAGMA", stmt, re.I):
                continue
            try:
                self.db.execute(stmt)
            except sqlite3.Error as e:
                log(f"  stmt failed in {os.path.basename(path)}: {stmt.strip()[:120]!r} ({e})")
        self.reset_cache()

    def load_file(self, path: str, text_mode=False):
        log(f"load {path}")
        if not os.path.exists(path):
            log(f"  missing file {path}")
            return
        if path.lower().endswith(".sql"):
            if not text_mode:
                self.load_sql(path)
        elif path.lower().endswith(".xml"):
            self.load_xml(path, text_mode=text_mode)


def split_sql(sql: str):
    stmts, buf, depth = [], [], 0
    for line in sql.splitlines():
        s = line.strip()
        if s.startswith("--"):
            continue
        buf.append(line)
        up = s.upper()
        if up.startswith("CREATE TRIGGER"):
            depth += 1
        if depth and re.search(r"\bEND\s*;\s*$", up):
            depth = 0
            stmts.append("\n".join(buf))
            buf = []
        elif not depth and s.endswith(";"):
            stmts.append("\n".join(buf))
            buf = []
    if "".join(buf).strip():
        stmts.append("\n".join(buf))
    return stmts


# ---------------------------------------------------------------- modinfo ---

def eval_criteria(crit: ET.Element, loaded_mod_ids: set[str]) -> bool:
    results = []
    for c in crit:
        if not isinstance(c.tag, str):
            continue
        text = (c.text or "").strip()
        if c.tag == "GameCoreInUse":
            results.append("Expansion2" in text.split(","))
        elif c.tag == "RuleSetInUse":
            results.append("RULESET_EXPANSION_2" in [t.strip() for t in text.split(",")])
        elif c.tag == "LeaderPlayable":
            results.append("Expansion2_Players" in text)
        elif c.tag == "ModInUse":
            results.append(text.lower() in loaded_mod_ids)
        elif c.tag in ("ConfigurationValueMatches", "ConfigurationValueContains"):
            results.append(False)  # optional game modes are off
        else:
            log(f"  unknown criterion {c.tag}; treating as false")
            results.append(False)
    if not results:
        return True
    if crit.get("any") == "1":
        return any(results)
    return all(results)


def mod_actions(modinfo_path: str, loaded_mod_ids: set[str]):
    """Yield (kind, load_order, [file paths]) for satisfied in-game actions."""
    root = ET.parse(modinfo_path).getroot()
    base = os.path.dirname(modinfo_path)
    crits = {c.get("id"): c for c in root.iter("Criteria")}
    ingame = root.find("InGameActions")
    if ingame is None:
        return
    for act in ingame:
        if act.tag not in ("UpdateDatabase", "UpdateText"):
            continue
        cid = act.get("criteria")
        if cid:
            crit = crits.get(cid)
            if crit is None or not eval_criteria(crit, loaded_mod_ids):
                continue
        lo = 0
        props = act.find("Properties")
        if props is not None and props.find("LoadOrder") is not None:
            lo = int(props.find("LoadOrder").text)
        files = []
        for i, f in enumerate(act.findall("File")):
            pri = int(f.get("Priority", "0"))
            files.append((-pri, i, os.path.join(base, f.text.strip().replace("/", os.sep))))
        files.sort()
        yield act.tag, lo, [p for _, _, p in files]


def build(game_dir: str, out_path: str) -> sqlite3.Connection:
    if os.path.exists(out_path):
        os.remove(out_path)
    db = sqlite3.connect(out_path, isolation_level=None)
    db.create_function("Make_Hash", 1, make_hash)
    db.execute("PRAGMA foreign_keys = ON")
    db.execute("BEGIN")
    db.execute("PRAGMA defer_foreign_keys = ON")
    loader = Loader(db)

    data_dir = os.path.join(game_dir, "Base", "Assets", "Gameplay", "Data")
    for sql in sorted(glob.glob(os.path.join(data_dir, "Schema", "*.sql"))):
        loader.load_file(sql)
    db.execute("CREATE TABLE IF NOT EXISTS LocalizedText(Tag TEXT PRIMARY KEY, Text TEXT)")
    loader.reset_cache()
    for xml in sorted(glob.glob(os.path.join(data_dir, "*.xml"))):
        loader.load_file(xml)
    for xml in sorted(glob.glob(os.path.join(game_dir, "Base", "Assets", "Text", "en_US", "*.xml"))):
        loader.load_file(xml, text_mode=True)

    # Collect DLC modinfos.
    mods = []
    for d in sorted(os.listdir(os.path.join(game_dir, "DLC"))):
        if d in EXCLUDED_DLC:
            continue
        for mi in glob.glob(os.path.join(game_dir, "DLC", d, "*.modinfo")):
            mid = ET.parse(mi).getroot().get("id", "").lower()
            mods.append((d, mi, mid))
    loaded_ids = {m[2] for m in mods}

    db_actions, text_actions = [], []
    for order, (d, mi, _) in enumerate(mods):
        prio = 0 if d == "Expansion2" else 1  # the expansion applies before packs
        for kind, lo, files in mod_actions(mi, loaded_ids):
            target = db_actions if kind == "UpdateDatabase" else text_actions
            target.append((lo, prio, order, d, files))
    db_actions.sort(key=lambda a: a[:3])
    text_actions.sort(key=lambda a: a[:3])
    for lo, _, _, d, files in db_actions:
        log(f"== DLC {d} (LoadOrder {lo})")
        for f in files:
            loader.load_file(f)
    for lo, _, _, d, files in text_actions:
        for f in files:
            loader.load_file(f, text_mode=True)
    remove_dangling(db)
    db.execute("COMMIT")
    return db


def remove_dangling(db: sqlite3.Connection):
    """Drop rows whose references were never satisfied (the game would log
    these as database errors and ignore them)."""
    # Effect and collection types are registered by the game engine itself,
    # not by any data file; add them so DynamicModifiers rows survive.
    for kind, col in (("KIND_EFFECT", "EffectType"), ("KIND_COLLECTION", "CollectionType")):
        db.execute("INSERT OR IGNORE INTO Kinds(Kind) VALUES (?)", (kind,))
        db.execute(f"INSERT OR IGNORE INTO Types(Type, Kind) SELECT DISTINCT {col}, ? FROM DynamicModifiers "
                   f"WHERE {col} IS NOT NULL AND {col} NOT IN (SELECT Type FROM Types)", (kind,))
    for (kind,) in db.execute("SELECT DISTINCT Kind FROM Types WHERE Kind NOT IN (SELECT Kind FROM Kinds)").fetchall():
        log(f"  adding missing kind {kind}")
        db.execute("INSERT INTO Kinds(Kind) VALUES (?)", (kind,))
    for _ in range(10):
        bad = db.execute("PRAGMA foreign_key_check").fetchall()
        if not bad:
            return
        for table, rowid, parent, _fk in bad:
            log(f"  dangling reference {table} rowid {rowid} -> {parent}")
            if rowid is not None:
                db.execute(f'DELETE FROM "{table}" WHERE rowid = ?', (rowid,))


if __name__ == "__main__":
    game = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_GAME_DIR
    out = sys.argv[2] if len(sys.argv) > 2 else "civ6_gs.sqlite"
    build(game, out)
    with open(out + ".log", "w", encoding="utf-8") as fh:
        fh.write("\n".join(log_lines))
    print(f"built {out}; {len(log_lines)} log lines")
