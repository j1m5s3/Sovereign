"""Resolve game type identifiers (UNIT_WARRIOR, YIELD_FOOD, ...) to English names."""
from __future__ import annotations

import re
import sqlite3

_ICON = re.compile(r"\[ICON_[A-Za-z0-9_]+\]\s*")
_NEWLINE = re.compile(r"\[NEWLINE\]")
_COLOR = re.compile(r"\[(?:/?COLOR[^\]]*|ENDCOLOR)\]")

YIELD_ABBR = {
    "YIELD_FOOD": "Food", "YIELD_PRODUCTION": "Production", "YIELD_GOLD": "Gold",
    "YIELD_SCIENCE": "Science", "YIELD_CULTURE": "Culture", "YIELD_FAITH": "Faith",
}


def clean(text: str | None) -> str:
    if text is None:
        return ""
    text = _ICON.sub("", text)
    text = _NEWLINE.sub(" ", text)
    text = _COLOR.sub("", text)
    return re.sub(r"\s+", " ", text).strip()


class Names:
    def __init__(self, db: sqlite3.Connection):
        self.db = db
        self.text = {tag: txt for tag, txt in db.execute("SELECT Tag, Text FROM LocalizedText")}
        self.names: dict[str, str] = {}
        tables = [r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")]
        for t in tables:
            cols = [c[1] for c in db.execute(f'PRAGMA table_info("{t}")')]
            if "Name" not in cols or not cols:
                continue
            key = cols[0]
            if key == "Name":
                continue
            try:
                for k, n in db.execute(f'SELECT "{key}", Name FROM "{t}"'):
                    if isinstance(k, str) and isinstance(n, str) and n.startswith("LOC_") and k not in self.names:
                        resolved = self.text.get(n)
                        if resolved:
                            self.names[k] = clean(resolved)
            except sqlite3.Error:
                pass
        self.names.update(YIELD_ABBR)
        for k in [r[0] for r in db.execute("SELECT DISTINCT Value FROM ModifierArguments WHERE Name='BeliefYieldType'")]:
            self.names[k] = k.replace("BELIEF_YIELD_PER_", "").replace("BELIEF_YIELD_", "").replace("_", " ").lower()
        for k in list(self.names):
            if k.startswith("TERRAIN_") and k.endswith("_MOUNTAIN"):
                self.names[k] = "Mountain"
        self.names["NO_ERA"] = "any era"
        # Great person individuals, governors etc. share the generic mechanism.
        # Tags such as CLASS_* and unit promotion classes:
        for k, n in db.execute("SELECT PromotionClassType, Name FROM UnitPromotionClasses"):
            if n in self.text:
                self.names[k] = clean(self.text[n])

    def loc(self, tag: str | None) -> str:
        if not tag:
            return ""
        return clean(self.text.get(tag, tag))

    def __call__(self, key) -> str:
        if key is None:
            return ""
        if not isinstance(key, str):
            return str(key)
        if key in self.names:
            return self.names[key]
        return humanize(key)


_PREFIXES = (
    "BUILDING_", "DISTRICT_", "UNIT_", "TECH_", "CIVIC_", "POLICY_", "BELIEF_", "IMPROVEMENT_",
    "RESOURCE_", "FEATURE_", "TERRAIN_", "PROMOTION_CLASS_", "PROMOTION_", "YIELD_", "ERA_",
    "GREAT_PERSON_CLASS_", "GREAT_PERSON_INDIVIDUAL_", "GOVERNMENT_", "SLOT_", "CLASS_",
    "ABILITY_", "PROJECT_", "GREATWORKOBJECT_", "GREATWORKSLOT_", "DOMAIN_", "LEADER_",
    "CIVILIZATION_", "TRAIT_", "COMMEMORATION_", "RESOLUTION_", "EMERGENCY_", "ROUTE_",
    "RANDOM_EVENT_", "GOVERNOR_PROMOTION_", "GOVERNOR_", "UNITOPERATION_", "DIPLOACTION_",
    "HAPPINESS_", "AGENDA_", "TAG_",
)


def humanize(key: str) -> str:
    k = key
    if k.startswith("GOVERNMENTBONUS_"):
        return k[len("GOVERNMENTBONUS_"):].replace("_", " ").lower()
    for p in _PREFIXES:
        if k.startswith(p):
            k = k[len(p):]
            break
    return k.replace("_", " ").title()


def md_escape(s) -> str:
    if s is None:
        return ""
    return str(s).replace("|", "\\|").replace("\n", " ")
