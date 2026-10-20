"""The twelve launch leaders as themselves (leaders-and-art-style.md): each with their own face, hair, headgear and
dress, after the Figures kit's ruler (same proportions and helpers). One part of each is on the team slot (a cloak,
a mantle or a sash), so on the map a leader still wears its owner's colour; the rest is their own.

Pieces are SM_Leader_<CIV> (the civ id's last word: SM_Leader_ENGLAND ... SM_Leader_INCA), in the Figures kit.
LOOK gives each a colour for its team slot in portraits and the setup screen (their own colour when no game is on).
"""
import math

import kit_figures as F
import kitlib

# Their own colours, painted (never tinted).
CRIMSON, NAVY, PURPLE, WHITE, CREAM, BLACK = "#7a1a22", "#22305e", "#4a2a6a", "#ece6d8", "#e0d4b4", "#1c1a1c"
GREEN, JADE, TURQ, RED, OCHRE, BRONZE_DARK = "#2f5a32", "#3d8a6a", "#2f9aa0", "#a8261e", "#c08a2a", "#6a4a1e"
LAPIS, GOLD, BRONZE, PEARL, LEAF = "#2a4a9a", F.GOLD, F.BRONZE, "#efeadc", "#4f7a35"
RED_HAIR, WHITE_HAIR = "#9a4a22", "#d8d2c6"
QUETZAL, FEATHER_RED = "#2d8a4a", "#b8322a"
kitlib.patterns({
    "cloth": [CRIMSON, NAVY, PURPLE, WHITE, CREAM, BLACK, GREEN, RED, OCHRE, LAPIS],
    "hair": [RED_HAIR, WHITE_HAIR, QUETZAL, FEATHER_RED],
    "metal": [BRONZE_DARK],
    "foliage": [LEAF],
    "stone": [JADE, TURQ],
})

# Each leader's team-slot colour in portraits and on the setup screen.
LOOK = {"ENGLAND": "#b01e2a", "FRANCE": "#2a4aa8", "ROME": "#6a1a7a", "GREECE": "#b0302a", "PERSIA": "#d0a020",
        "ARABIA": "#2a7a3a", "CHINA": "#c0302a", "JAPAN": "#e8e2d6", "EGYPT": "#e0c050", "MALI": "#c07a20",
        "AZTEC": "#2a8a7a", "INCA": "#c8a020"}


def _robe(p, s, color, team=False, hem=0.06, sleeves=True, sleeve_end=0.93):
    """A long robe or gown to the ankle, with long sleeves."""
    p.team_color(team)
    p.loft([(hem * s, 0.25 * s, 0.2 * s), (0.5 * s, 0.215 * s, 0.16 * s), (0.92 * s, 0.18 * s, 0.125 * s), (1.06 * s, 0.162 * s, 0.112 * s),
            (1.24 * s, 0.195 * s, 0.13 * s), (1.38 * s, 0.218 * s, 0.12 * s), (1.45 * s, 0.11 * s, 0.085 * s)],
           color, segments=20, folds=10, fold_depth=0.04, cap_top=False)
    if sleeves:
        for side in (-1, 1):
            p.segment((side * 0.215 * s, 0.0, 1.37 * s), (side * 0.258 * s, 0.005 * s, sleeve_end * s), 0.066 * s, 0.05 * s, color, caps=False)
    p.team_color(False)


def _hem(p, s, color, z=0.06):
    p.loft([(z * s, 0.252 * s, 0.202 * s), (z * s + 0.05 * s, 0.245 * s, 0.196 * s)], color, segments=20, folds=10, fold_depth=0.04)


def _band(p, z, s, color, rx=0.097, ry=0.107, h=0.05, y=0.008):
    """A circlet round the head at height z."""
    p.loft([(z, rx * s, ry * s, 0, y * s), (z + h * s, (rx + 0.003) * s, (ry + 0.003) * s, 0, y * s)], color, segments=18,
           cap_bottom=False, cap_top=False, two_sided=True)


def _base(name, seed, s=1.04):
    p = F._figure("SM_Leader_" + name, seed)
    return p, s


def england(seed=401):
    """Elizabeth I: pale, red hair dressed high, a white ruff, a jewelled crimson gown, her cloak in the team colour."""
    p, s = _base("ENGLAND", seed, 1.0)
    F._body(p, F.SKIN[3], s=s)
    F._sandals(p, s=s, color=F.DARK_LEATHER)
    _robe(p, s, CRIMSON, hem=0.04)
    p.loft([(0.04 * s, 0.28 * s, 0.25 * s), (0.4 * s, 0.25 * s, 0.21 * s), (0.9 * s, 0.17 * s, 0.12 * s)], CRIMSON, segments=22,
           folds=14, fold_depth=0.05, cap_top=False)  # the farthingale skirt
    for z in (0.3, 0.55, 0.8):  # pearls down the bodice and skirt
        p.blob((0, -0.2 * s + z * 0.08, z * s + 0.1), 0.02 * s, PEARL, subdiv=1, wobble=0.0)
    p.loft([(1.43 * s, 0.13 * s, 0.11 * s), (1.47 * s, 0.17 * s, 0.15 * s), (1.5 * s, 0.13 * s, 0.11 * s)], WHITE, segments=24,
           folds=16, fold_depth=0.12)  # the ruff
    F._cloak(p, F.LINEN, s=s, hem=0.08, clasp=GOLD, team=True)
    z = F._head(p, F.SKIN[3], RED_HAIR, s=s)
    p.blob((0, 0.03 * s, z + 0.24 * s), 0.08 * s, RED_HAIR, squash=0.8, subdiv=2)  # hair dressed high
    _band(p, z + 0.25 * s, s, GOLD, rx=0.07, ry=0.075, h=0.035)
    for i in range(5):
        a = -math.pi / 2 + 2 * math.pi * i / 5
        p.blob((0.072 * s * math.cos(a), 0.008 * s + 0.077 * s * math.sin(a), z + 0.3 * s), 0.014 * s, PEARL, subdiv=1, wobble=0.0)
    return p


def france(seed=402):
    """Charlemagne: a white beard, a closed imperial crown with a cross, a blue robe, the mantle in the team colour."""
    p, s = _base("FRANCE", seed)
    F._body(p, F.SKIN[3], s=s)
    F._sandals(p, s=s, color=F.DARK_LEATHER, boots=True)
    _robe(p, s, NAVY)
    _hem(p, s, GOLD)
    F._belt(p, GOLD, 1.03, s=s, buckle=GOLD)
    F._cloak(p, F.LINEN, s=s, hem=0.1, clasp=GOLD, team=True)
    z = F._head(p, F.SKIN[3], WHITE_HAIR, s=s, beard=WHITE_HAIR)
    cz = z + 0.21 * s
    _band(p, cz, s, GOLD, h=0.06)
    # The closed arches over the crown, front to back and side to side, and a cross on top.
    p.loft([(cz + 0.06 * s, 0.1 * s, 0.012 * s), (cz + 0.11 * s, 0.07 * s, 0.012 * s), (cz + 0.14 * s, 0.02 * s, 0.012 * s)], GOLD,
           segments=10, arc=(0, math.pi), two_sided=True)
    p.loft([(cz + 0.06 * s, 0.012 * s, 0.11 * s), (cz + 0.11 * s, 0.012 * s, 0.08 * s), (cz + 0.14 * s, 0.012 * s, 0.02 * s)], GOLD,
           segments=10, arc=(math.pi / 2, math.pi * 1.5), two_sided=True)
    p.box((0, 0.008 * s, cz + 0.18 * s), (0.012 * s, 0.012 * s, 0.07 * s), GOLD)
    p.box((0, 0.008 * s, cz + 0.19 * s), (0.045 * s, 0.012 * s, 0.012 * s), GOLD)
    p.cylinder((0.29 * s, -0.08 * s, 0.05 * s), 0.016 * s, 1.45 * s, GOLD, segments=6)  # a sceptre
    return p


def rome(seed=403):
    """Augustus: young and clean-shaven, a laurel wreath, a white toga, its purple border in the team colour."""
    p, s = _base("ROME", seed)
    F._body(p, F.SKIN[0], s=s)
    F._sandals(p, s=s)
    _robe(p, s, WHITE, sleeves=False)
    p.team_color(True)
    p.segment((0.2 * s, -0.12 * s, 1.42 * s), (-0.17 * s, -0.14 * s, 0.7 * s), 0.05 * s, 0.05 * s, F.LINEN, segments=8)  # the toga's purple border
    p.team_color(False)
    p.loft([(0.6 * s, 0.27 * s, 0.21 * s, 0.04 * s, 0), (1.1 * s, 0.24 * s, 0.17 * s, 0.03 * s, 0), (1.42 * s, 0.13 * s, 0.1 * s, 0.05 * s, 0)],
           WHITE, segments=16, arc=(math.pi * 0.8, math.pi * 2.2), folds=8, fold_depth=0.07, two_sided=True)  # the toga's drape
    z = F._head(p, F.SKIN[0], F.HAIR[1], s=s)
    for i in range(14):  # the laurel: leaves round the head, open at the brow
        a = math.pi * 0.15 + math.pi * 1.7 * i / 13 - math.pi / 2
        p.blob((0.098 * s * math.cos(a + math.pi), 0.008 * s - 0.108 * s * math.sin(a + math.pi), z + 0.19 * s), 0.022 * s, LEAF,
               squash=0.4, subdiv=1, wobble=0.1)
    return p


def greece(seed=404):
    """Leonidas: a dark beard, a crested Corinthian helmet pushed back, bronze cuirass, the cloak in the team colour."""
    p, s = _base("GREECE", seed)
    F._body(p, F.SKIN[1], s=s)
    F._sandals(p, s=s)
    F._tunic(p, RED, s=s, hem=0.7)
    F._cuirass(p, s=s)
    F._pteruges(p, s=s)
    F._greaves(p, s=s)
    F._cloak(p, F.LINEN, s=s, hem=0.3, clasp=BRONZE, team=True)
    z = F._head(p, F.SKIN[1], F.HAIR[0], s=s, beard=F.HAIR[0])
    F._helmet(p, z + 0.04 * s, s=s, crest=F.CREST, transverse=True)
    F._spear(p, 0.3, s=s)
    return p


def persia(seed=405):
    """Cyrus: a long curled black beard, a tall gold-banded tiara, a robe of deep blue, the sash in the team colour."""
    p, s = _base("PERSIA", seed)
    F._body(p, F.SKIN[1], s=s)
    F._sandals(p, s=s, color=F.DARK_LEATHER, boots=True)
    _robe(p, s, LAPIS)
    _hem(p, s, GOLD)
    p.team_color(True)
    p.segment((0.18 * s, -0.122 * s, 1.4 * s), (-0.15 * s, -0.135 * s, 1.0 * s), 0.03 * s, 0.03 * s, F.LINEN, segments=6)
    p.team_color(False)
    F._belt(p, GOLD, 1.03, s=s, buckle=GOLD)
    z = F._head(p, F.SKIN[1], F.HAIR[2], s=s, beard=F.HAIR[2])
    p.loft([(z - 0.12 * s, 0.03 * s, 0.03 * s, 0, -0.06 * s), (z - 0.06 * s, 0.05 * s, 0.04 * s, 0, -0.05 * s)], F.HAIR[2], segments=10)  # the long beard
    p.loft([(z + 0.18 * s, 0.096 * s, 0.106 * s, 0, 0.008 * s), (z + 0.38 * s, 0.1 * s, 0.11 * s, 0, 0.008 * s),
            (z + 0.4 * s, 0.09 * s, 0.1 * s, 0, 0.008 * s)], CREAM, segments=18)  # the tiara
    for zz in (0.2, 0.3):
        _band(p, z + zz * s, s, GOLD, rx=0.1, ry=0.11, h=0.025)
    return p


def arabia(seed=406):
    """Saladin: a black beard, a white turban over a green-trimmed robe, the mantle in the team colour."""
    p, s = _base("ARABIA", seed)
    F._body(p, F.SKIN[1], s=s)
    F._sandals(p, s=s, color=F.DARK_LEATHER, boots=True)
    _robe(p, s, WHITE)
    _hem(p, s, GREEN)
    F._belt(p, GREEN, 1.03, s=s, buckle=GOLD)
    F._cloak(p, F.LINEN, s=s, hem=0.12, clasp=GOLD, team=True)
    z = F._head(p, F.SKIN[1], F.HAIR[2], s=s, beard=F.HAIR[2])
    for k, r in enumerate((0.115, 0.12, 0.11, 0.09)):  # the turban, wound round
        p.loft([(z + (0.16 + 0.04 * k) * s, r * s, (r + 0.01) * s, 0, 0.01 * s), (z + (0.2 + 0.04 * k) * s, (r - 0.005) * s, r * s, 0, 0.01 * s)],
               WHITE, segments=16)
    p.blob((0, -0.095 * s, z + 0.24 * s), 0.02 * s, "#2a8a4a", subdiv=1, wobble=0.0)  # an emerald at its front
    # A curved sword at the hip.
    p.segment((-0.2 * s, -0.06 * s, 1.0 * s), (-0.26 * s, 0.02 * s, 0.4 * s), 0.025 * s, 0.018 * s, F.DARK_LEATHER, segments=8)
    return p


def china(seed=407):
    """Qin Shi Huang: a moustache and goatee, the mianguan crown with its bead tassels, a black robe red-edged, the sash team."""
    p, s = _base("CHINA", seed)
    F._body(p, F.SKIN[3], s=s)
    F._sandals(p, s=s, color=BLACK, boots=True)
    _robe(p, s, BLACK, sleeve_end=0.85)
    for side in (-1, 1):  # wide sleeves
        p.loft([(0.82 * s, 0.07 * s, 0.07 * s, side * 0.26 * s, 0), (1.0 * s, 0.11 * s, 0.1 * s, side * 0.26 * s, 0)], BLACK, segments=12)
    _hem(p, s, RED)
    p.team_color(True)
    F._belt(p, F.LINEN, 1.03, s=s)
    p.team_color(False)
    z = F._head(p, F.SKIN[3], F.HAIR[0], s=s, moustache=F.HAIR[0])
    p.box((0, -0.105 * s, z - 0.005 * s), (0.025 * s, 0.015 * s, 0.05 * s), F.HAIR[0])  # the goatee
    p.cylinder((0, 0.01 * s, z + 0.2 * s), 0.06 * s, 0.1 * s, BLACK, segments=12)  # the cap
    p.box((0, 0.0, z + 0.31 * s), (0.2 * s, 0.32 * s, 0.015 * s), BLACK)  # the board
    for y in (-0.15, 0.15):  # strings of beads hanging front and back
        for x in (-0.07, -0.035, 0.0, 0.035, 0.07):
            p.segment((x * s, y * s, z + 0.3 * s), (x * s, y * s, z + 0.2 * s), 0.004 * s, 0.004 * s, RED, segments=4, caps=False)
            p.blob((x * s, y * s, z + 0.2 * s), 0.009 * s, GOLD, subdiv=1, wobble=0.0)
    return p


def japan(seed=408):
    """Tokugawa Ieyasu: a shaved crown and topknot, a dark kimono under winged kamishimo shoulders, the vest in the team colour."""
    p, s = _base("JAPAN", seed)
    F._body(p, F.SKIN[3], s=s)
    F._sandals(p, s=s, color=CREAM)
    _robe(p, s, BLACK)
    p.team_color(True)
    p.loft([(1.02 * s, 0.18 * s, 0.12 * s), (1.3 * s, 0.2 * s, 0.13 * s), (1.42 * s, 0.13 * s, 0.09 * s)], F.LINEN, segments=16,
           arc=(math.pi * 0.05, math.pi * 0.95), two_sided=True)  # the kataginu vest over the chest
    for side in (-1, 1):  # its stiff winged shoulders
        p.box((side * 0.24 * s, 0.0, 1.43 * s), (0.2 * s, 0.14 * s, 0.02 * s), F.LINEN, rot_z=0.0)
    p.team_color(False)
    F._belt(p, OCHRE, 1.0, s=s)
    z = F._head(p, F.SKIN[3], F.HAIR[0], s=s, moustache=F.HAIR[0], bald=True)
    p.loft([(z + 0.07 * s, 0.083 * s, 0.09 * s, 0, 0.012 * s), (z + 0.15 * s, 0.093 * s, 0.1 * s, 0, 0.008 * s)], F.HAIR[0], segments=14,
           arc=(-0.2, math.pi + 0.2), two_sided=True)  # hair at the sides and back
    p.segment((0, 0.02 * s, z + 0.25 * s), (0, -0.07 * s, z + 0.26 * s), 0.014 * s, 0.011 * s, F.HAIR[0], segments=6)  # the topknot
    p.segment((-0.22 * s, -0.08 * s, 1.0 * s), (-0.3 * s, 0.1 * s, 0.75 * s), 0.02 * s, 0.018 * s, BLACK, segments=8)  # the katana
    return p


def egypt(seed=409):
    """Ramesses II: the striped nemes headdress and false beard, a broad gold collar, a white kilt, the sash in the team colour."""
    p, s = _base("EGYPT", seed)
    F._body(p, F.SKIN[2], s=s)
    F._sandals(p, s=s, color=OCHRE)
    F._tunic(p, WHITE, s=s, hem=0.55)
    p.team_color(True)
    F._belt(p, F.LINEN, 0.98, s=s, buckle=GOLD)
    p.team_color(False)
    p.loft([(1.3 * s, 0.2 * s, 0.13 * s), (1.38 * s, 0.215 * s, 0.13 * s), (1.45 * s, 0.11 * s, 0.085 * s)], GOLD, segments=18)  # broad collar
    p.loft([(1.33 * s, 0.205 * s, 0.135 * s), (1.36 * s, 0.21 * s, 0.135 * s)], LAPIS, segments=18)
    z = F._head(p, F.SKIN[2], F.HAIR[2], s=s)
    p.loft([(z + 0.02 * s, 0.13 * s, 0.11 * s, 0, 0.03 * s), (z + 0.15 * s, 0.11 * s, 0.11 * s, 0, 0.01 * s), (z + 0.25 * s, 0.095 * s, 0.1 * s, 0, 0.01 * s),
            (z + 0.28 * s, 0.06 * s, 0.07 * s, 0, 0.01 * s)], OCHRE, segments=18, arc=(-0.25, math.pi + 0.25), two_sided=True)  # the nemes
    for k in range(5):  # its blue stripes
        p.loft([(z + (0.04 + 0.045 * k) * s, 0.128 * s, 0.112 * s, 0, 0.03 * s), (z + (0.055 + 0.045 * k) * s, 0.126 * s, 0.112 * s, 0, 0.03 * s)],
               LAPIS, segments=18, arc=(-0.25, math.pi + 0.25), two_sided=True)
    for side in (-1, 1):  # lappets over the shoulders
        p.box((side * 0.1 * s, -0.04 * s, z - 0.06 * s), (0.05 * s, 0.03 * s, 0.18 * s), OCHRE)
    p.cone((0, -0.1 * s, z + 0.24 * s), 0.014 * s, 0.05 * s, GOLD, segments=6)  # the uraeus
    p.box((0, -0.075 * s, z - 0.06 * s), (0.025 * s, 0.025 * s, 0.09 * s), F.HAIR[2], taper=0.7)  # the false beard
    p.cylinder((0.29 * s, -0.08 * s, 0.75 * s), 0.014 * s, 0.4 * s, GOLD, segments=6)  # the crook
    return p


def mali(seed=410):
    """Mansa Musa: a short beard, a gold crown over a white turban, a white robe gold-trimmed, the cloak in the team colour."""
    p, s = _base("MALI", seed)
    F._body(p, F.SKIN[2], s=s)
    F._sandals(p, s=s, color=OCHRE, boots=True)
    _robe(p, s, WHITE)
    _hem(p, s, GOLD)
    F._belt(p, GOLD, 1.03, s=s, buckle=GOLD)
    F._cloak(p, F.LINEN, s=s, hem=0.14, clasp=GOLD, team=True)
    z = F._head(p, F.SKIN[2], F.HAIR[2], s=s, beard=F.HAIR[2])
    for k, r in enumerate((0.115, 0.12, 0.11)):
        p.loft([(z + (0.16 + 0.04 * k) * s, r * s, (r + 0.01) * s, 0, 0.01 * s), (z + (0.2 + 0.04 * k) * s, (r - 0.005) * s, r * s, 0, 0.01 * s)],
               WHITE, segments=16)
    _band(p, z + 0.26 * s, s, GOLD, rx=0.1, ry=0.11, h=0.06)
    for i in range(6):
        a = -math.pi / 2 + 2 * math.pi * i / 6
        p.cone((0.1 * s * math.cos(a), 0.008 * s + 0.11 * s * math.sin(a), z + 0.31 * s), 0.02 * s, 0.05 * s, GOLD, segments=6)
    p.blob((0.29 * s, -0.08 * s, 0.85 * s), 0.05 * s, GOLD, subdiv=2, wobble=0.0)  # a gold orb in hand
    return p


def aztec(seed=411):
    """Moctezuma I: a crown of quetzal plumes on a turquoise diadem, jade ear spools, the tilma cloak in the team colour."""
    p, s = _base("AZTEC", seed)
    F._body(p, F.SKIN[1], s=s)
    F._sandals(p, s=s, color=OCHRE)
    F._tunic(p, WHITE, s=s, hem=0.6)
    p.loft([(1.3 * s, 0.2 * s, 0.13 * s), (1.4 * s, 0.215 * s, 0.13 * s), (1.45 * s, 0.11 * s, 0.085 * s)], JADE, segments=18)  # jade collar
    F._cloak(p, F.LINEN, s=s, hem=0.25, clasp=JADE, team=True)
    z = F._head(p, F.SKIN[1], F.HAIR[0], s=s)
    _band(p, z + 0.18 * s, s, TURQ, h=0.06)
    for i in range(13):  # plumes fanning up and back
        a = math.pi * (0.1 + 0.8 * i / 12)
        tip = (0.3 * s * math.cos(a), 0.06 * s + 0.12 * s, z + 0.24 * s + 0.32 * s * math.sin(a))
        p.segment((0.06 * s * math.cos(a), 0.06 * s, z + 0.24 * s), tip, 0.02 * s, 0.006 * s, QUETZAL if i % 3 else FEATHER_RED, segments=5)
    for side in (-1, 1):
        p.cylinder((side * 0.095 * s, 0.005 * s, z + 0.09 * s), 0.02 * s, 0.015 * s, JADE, segments=8)  # ear spools
    p.cylinder((0.29 * s, -0.08 * s, 0.05 * s), 0.016 * s, 1.5 * s, F.WOOD, segments=6)  # a staff
    return p


def inca(seed=412):
    """Pachacuti: the mascapaicha (a red fringe on a braided band, a plume above), gold ear spools, a checkered unku tunic, the
    mantle in the team colour."""
    p, s = _base("INCA", seed)
    F._body(p, F.SKIN[1], s=s)
    F._sandals(p, s=s, color=OCHRE)
    F._tunic(p, CREAM, s=s, hem=0.55)
    for row in range(4):  # the unku's checkered band
        for col in range(6):
            a = math.pi * (0.15 + 0.7 * col / 5)
            if (row + col) % 2 == 0:
                p.box((0.19 * s * math.cos(a + math.pi), -0.14 * s, (1.05 + row * 0.06) * s), (0.05 * s, 0.01 * s, 0.05 * s), RED)
    F._cloak(p, F.LINEN, s=s, hem=0.3, clasp=GOLD, team=True)
    z = F._head(p, F.SKIN[1], F.HAIR[0], s=s)
    _band(p, z + 0.16 * s, s, OCHRE, h=0.05)  # the llautu
    for i in range(9):  # the red fringe over the brow
        x = (-0.06 + 0.015 * i) * s
        p.box((x, -0.105 * s, z + 0.12 * s), (0.008 * s, 0.01 * s, 0.07 * s), FEATHER_RED)
    p.segment((0, -0.02 * s, z + 0.2 * s), (0, 0.0, z + 0.36 * s), 0.008 * s, 0.006 * s, BLACK, segments=4)  # the plume's stem
    p.blob((0, 0.0, z + 0.38 * s), 0.03 * s, WHITE, squash=1.6, subdiv=1)
    for side in (-1, 1):
        p.cylinder((side * 0.095 * s, 0.005 * s, z + 0.09 * s), 0.025 * s, 0.012 * s, GOLD, segments=8)
    p.cylinder((0.29 * s, -0.08 * s, 0.05 * s), 0.016 * s, 1.45 * s, GOLD, segments=6)  # a gold staff
    return p


LEADERS = {"ENGLAND": england, "FRANCE": france, "ROME": rome, "GREECE": greece, "PERSIA": persia, "ARABIA": arabia,
           "CHINA": china, "JAPAN": japan, "EGYPT": egypt, "MALI": mali, "AZTEC": aztec, "INCA": inca}
PIECES = list(LEADERS.values())
