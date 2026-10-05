"""Figures kit: static people at real proportions (about 1.75 m), Ancient/Classical dress.
Clothing on the team slot takes the owner's colour in game; skin, hair, metal and wood keep
their own. Rigging and animation come later (art plan: static figures stand in).
"""
import math

from kitlib import Piece

SKIN = ["#c99a76", "#b07e5c", "#8d5f43", "#e0b896"]
HAIR = ["#3a2a1e", "#5a3d26", "#1f1a17", "#7a5a3a"]
LINEN = "#e8e2d4"     # team cloth is painted light so the tint reads
BRONZE = "#a87e3c"
IRON = "#8a8d90"
LEATHER = "#6b4a2f"
WOOD = "#6b4c33"
GOLD = "#d6aa3a"


def _body(p, skin, hair, tunic, tunic_team=False, legs="#5a4a3c", cloak=None, robe=False, height=1.0):
    """A standing figure: legs, tunic (or robe), arms at the sides, head and hair."""
    s = height
    if robe:
        p.team_color(tunic_team)
        p.box((0, 0, 0.62 * s), (0.5 * s, 0.32 * s, 1.24 * s), tunic, taper=0.72)
        p.team_color(False)
    else:
        for x in (-0.11, 0.11):
            p.box((x * s, 0, 0.42 * s), (0.15 * s, 0.17 * s, 0.84 * s), legs, taper=0.85)
        p.team_color(tunic_team)
        p.box((0, 0, 0.98 * s), (0.44 * s, 0.26 * s, 0.62 * s), tunic, taper=0.82)  # tunic skirt and chest
        p.team_color(False)
    p.box((0, 0, 1.36 * s), (0.4 * s, 0.24 * s, 0.22 * s), tunic if not tunic_team else "#d8d0c0")  # shoulders
    for x in (-0.27, 0.27):
        p.box((x * s, 0, 1.08 * s), (0.11 * s, 0.13 * s, 0.56 * s), skin, taper=0.9)
    p.box((0, 0, 1.5 * s), (0.1 * s, 0.1 * s, 0.1 * s), skin)  # neck
    p.blob((0, 0, 1.63 * s), 0.12 * s, skin, squash=1.15, subdiv=1, wobble=0.03)
    p.blob((0, 0.02 * s, 1.68 * s), 0.115 * s, hair, squash=0.75, subdiv=1, wobble=0.05)
    if cloak:
        p.team_color(True)
        p.box((0, 0.17 * s, 0.95 * s), (0.5 * s, 0.05 * s, 1.0 * s), cloak, taper=1.25)
        p.team_color(False)


def citizen(seed=31):
    p = Piece("SM_Citizen", seed)
    _body(p, SKIN[0], HAIR[1], "#a58a64")
    p.box((0, -0.16, 0.95), (0.3, 0.06, 0.25), "#7d6448")  # a satchel
    return p


def herald(seed=32):
    p = Piece("SM_Herald", seed)
    _body(p, SKIN[3], HAIR[3], LINEN, tunic_team=True)
    p.cylinder((0.3, -0.12, 1.15), 0.035, 0.32, "#efe6cf", segments=6)  # the scroll
    p.box((0, 0, 1.82), (0.26, 0.26, 0.08), "#8f2f24")  # a cap
    return p


def captain(seed=33):
    p = Piece("SM_Captain", seed)
    _body(p, SKIN[1], HAIR[0], "#8b2a22", cloak=LINEN)
    p.box((0, -0.01, 1.1), (0.46, 0.29, 0.5), BRONZE)  # cuirass
    p.cone((0, 0, 1.66), 0.16, 0.26, BRONZE, segments=8)  # helmet
    p.box((0, 0, 1.95), (0.04, 0.3, 0.12), "#b33a2a")  # crest
    p.cylinder((0.33, 0.0, 0.0), 0.025, 2.3, WOOD, segments=5)  # spear shaft
    p.cone((0.33, 0.0, 2.3), 0.05, 0.25, IRON, segments=5)
    return p


def soldier(seed=34):
    p = Piece("SM_Soldier", seed)
    _body(p, SKIN[2], HAIR[2], LINEN, tunic_team=True, legs=LEATHER)
    p.cone((0, 0, 1.66), 0.155, 0.24, BRONZE, segments=8)  # helmet
    p.team_color(True)
    p.disc((-0.36, -0.12, 1.0), 0.32, 0.06, LINEN, segments=14)  # round shield on the left arm, team-painted
    p.team_color(False)
    p.disc((-0.36, -0.16, 1.0), 0.07, 0.04, BRONZE, segments=8)  # boss
    p.cylinder((0.33, 0.0, 0.0), 0.025, 2.2, WOOD, segments=5)
    p.cone((0.33, 0.0, 2.2), 0.05, 0.25, IRON, segments=5)
    return p


def leader(seed=35):
    p = Piece("SM_Leader", seed)
    _body(p, SKIN[0], HAIR[3], LINEN, tunic_team=True, robe=True, cloak=LINEN, height=1.04)
    p.box((0, 0, 1.06), (0.44, 0.28, 0.08), GOLD)  # belt
    p.cylinder((0, 0.01, 1.78), 0.12, 0.09, GOLD, segments=10)  # crown band
    for i in range(5):
        a = 2 * math.pi * i / 5
        p.cone((0.11 * math.cos(a), 0.01 + 0.11 * math.sin(a), 1.86), 0.03, 0.09, GOLD, segments=4)
    p.cylinder((0.31, -0.05, 0.25), 0.02, 0.85, IRON, segments=5)  # sword at the side
    p.box((0.31, -0.05, 1.12), (0.16, 0.04, 0.03), GOLD)
    return p


PIECES = [citizen, herald, captain, soldier, leader]
