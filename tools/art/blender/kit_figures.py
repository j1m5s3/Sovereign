"""Figures kit: static people at real proportions (about 1.78 m), Ancient/Classical dress, in the
stylized-realism look (specs/sovereign/leaders-and-art-style.md): rounded anatomy lofted from
rings, a face (brow, nose, eyes, ears, beard), and costumes in layers (tunic, cuirass, leather
strips, greaves, cloak with folds, crested helmet). Smooth-shaded, with the paint darkening
toward the foot of every part. Clothing on the team slot takes the owner's colour in game, so it
is painted light; skin, hair, metal and leather keep their own. The front is -Y.
Rigging and animation come later (art plan: static figures stand in).
"""
import math

import kitlib
from kitlib import Piece

SKIN = ["#c27a52", "#a2643f", "#7a4a2e", "#d69a72"]
HAIR = ["#2a1d14", "#4a2e1a", "#171310", "#5e3f22"]
LINEN = "#e8e2d4"     # team cloth is painted light so the tint reads
WOOL = "#b9a57a"
BRONZE = "#8f5e22"
GOLD = "#c9921e"
IRON = "#8a8d90"
LEATHER = "#5a361e"
DARK_LEATHER = "#3a2414"
WOOD = "#6b4c33"
CREST = "#9a1f1a"
EYE = "#2a1e16"
EYE_WHITE = "#e6dccb"

kitlib.patterns({
    "skin": SKIN,
    "hair": HAIR + [CREST],
    "cloth": [LINEN, WOOL, "#6f7a3a", "#7a3b22", "#8b2a22", "#8f2f24", "#4a2a6a", "#6a1420"],
    "leather": [LEATHER, DARK_LEATHER, "#7d6448"],
    "metal": [BRONZE, GOLD, IRON],
    "wood": [WOOD],
})


def _figure(name, seed):
    p = Piece(name, seed)
    p.smooth = True
    p.part_shade = 0.6
    p.shade_low, p.shade_range = 0.58, 0.52  # deeper painted shadow at the foot of each part
    return p


def _head(p, skin, hair, s=1.0, beard=None, moustache=None, bald=False):
    """Head and face, centred at x = 0 with the face toward -Y."""
    z = 1.49 * s
    p.loft([(z, 0.04 * s, 0.045 * s, 0, -0.012 * s),           # chin
            (z + 0.04 * s, 0.07 * s, 0.075 * s, 0, -0.006 * s),  # jaw
            (z + 0.10 * s, 0.083 * s, 0.095 * s),                # cheekbones
            (z + 0.16 * s, 0.088 * s, 0.1 * s),                  # temples
            (z + 0.21 * s, 0.078 * s, 0.09 * s, 0, 0.008 * s),
            (z + 0.245 * s, 0.045 * s, 0.055 * s, 0, 0.012 * s)], skin, segments=16)
    p.segment((0, 0.005 * s, 1.40 * s), (0, 0.0, z + 0.03 * s), 0.05 * s, 0.045 * s, skin)  # neck
    p.box((0, -0.098 * s, z + 0.105 * s), (0.026 * s, 0.03 * s, 0.055 * s), skin, taper=0.6)  # nose
    for side in (-1, 1):  # brows
        p.box((side * 0.036 * s, -0.09 * s, z + 0.152 * s), (0.045 * s, 0.014 * s, 0.012 * s), hair if not bald else skin, rot_z=side * 0.12)
    for x in (-0.035, 0.035):
        p.blob((x * s, -0.082 * s, z + 0.13 * s), 0.017 * s, EYE_WHITE, squash=0.6, subdiv=2, wobble=0.0)  # eyes
        p.blob((x * s, -0.092 * s, z + 0.13 * s), 0.009 * s, EYE, squash=0.9, subdiv=1, wobble=0.0)
        p.blob((x * 2.55 * s, 0.005 * s, z + 0.11 * s), 0.022 * s, skin, squash=1.3, subdiv=1, wobble=0.0)  # ears
    p.box((0, -0.088 * s, z + 0.06 * s), (0.04 * s, 0.012 * s, 0.008 * s), "#9a5a4a")  # mouth
    if not bald:
        # Hair: over the back of the head down to the nape, and a dome on the crown from the hairline up.
        p.loft([(z + 0.07 * s, 0.083 * s, 0.09 * s, 0, 0.012 * s), (z + 0.14 * s, 0.095 * s, 0.104 * s, 0, 0.006 * s),
                (z + 0.19 * s, 0.092 * s, 0.1 * s, 0, 0.008 * s)], hair, segments=14, arc=(-0.2, math.pi + 0.2), folds=7, fold_depth=0.04, two_sided=True)
        p.loft([(z + 0.172 * s, 0.093 * s, 0.103 * s, 0, 0.004 * s), (z + 0.215 * s, 0.087 * s, 0.098 * s, 0, 0.009 * s),
                (z + 0.255 * s, 0.06 * s, 0.07 * s, 0, 0.013 * s), (z + 0.272 * s, 0.02 * s, 0.025 * s, 0, 0.014 * s)], hair, segments=16, folds=8, fold_depth=0.05)
    if beard:
        p.loft([(z - 0.03 * s, 0.03 * s, 0.03 * s, 0, -0.04 * s),
                (z + 0.02 * s, 0.075 * s, 0.07 * s, 0, -0.018 * s),
                (z + 0.075 * s, 0.088 * s, 0.09 * s, 0, -0.004 * s)], beard, segments=14, arc=(math.pi * 1.05, math.pi * 1.95), two_sided=True)
        p.blob((0, -0.065 * s, z + 0.0), 0.045 * s, beard, squash=1.2, subdiv=1, wobble=0.1)
    if moustache or beard:
        # Two drooping wings from under the nose.
        for side in (-1, 1):
            p.segment((side * 0.006 * s, -0.098 * s, z + 0.079 * s), (side * 0.03 * s, -0.095 * s, z + 0.072 * s), 0.011 * s, 0.009 * s, moustache or beard, segments=8)
            p.segment((side * 0.03 * s, -0.095 * s, z + 0.072 * s), (side * 0.045 * s, -0.087 * s, z + 0.052 * s), 0.009 * s, 0.005 * s, moustache or beard, segments=8)
    return z


def _body(p, skin, s=1.0, legs=None, arms=True):
    """Torso, arms and legs under the clothes. `legs` paints bare legs (None: skin)."""
    p.loft([(0.92 * s, 0.16 * s, 0.105 * s),   # hips
            (1.06 * s, 0.145 * s, 0.095 * s),  # waist
            (1.24 * s, 0.18 * s, 0.115 * s),   # chest
            (1.38 * s, 0.205 * s, 0.105 * s),  # shoulders
            (1.45 * s, 0.09 * s, 0.07 * s)], skin, segments=16)
    if arms:
        for side in (-1, 1):
            sh, el, wr = (side * 0.215 * s, 0.0, 1.37 * s), (side * 0.255 * s, 0.01 * s, 1.11 * s), (side * 0.265 * s, -0.03 * s, 0.86 * s)
            p.segment(sh, el, 0.052 * s, 0.042 * s, skin)
            p.segment(el, wr, 0.041 * s, 0.031 * s, skin)
            p.blob((side * 0.268 * s, -0.035 * s, 0.8 * s), 0.042 * s, skin, squash=1.5, subdiv=2, wobble=0.04)  # hand
    for side in (-1, 1):
        hip, knee, ankle = (side * 0.09 * s, 0.0, 0.9 * s), (side * 0.1 * s, -0.01 * s, 0.5 * s), (side * 0.1 * s, 0.01 * s, 0.09 * s)
        p.segment(hip, knee, 0.078 * s, 0.056 * s, legs or skin)
        p.segment(knee, ankle, 0.054 * s, 0.036 * s, legs or skin)


def _sandals(p, s=1.0, color=LEATHER, boots=False):
    for side in (-1, 1):
        x = side * 0.1 * s
        p.box((x, -0.05 * s, 0.035 * s), (0.09 * s, 0.24 * s, 0.07 * s), color, taper=0.85)  # foot
        if boots:
            p.segment((x, 0.01 * s, 0.06 * s), (x, 0.0, 0.32 * s), 0.05 * s, 0.047 * s, color, caps=False)
        else:
            for z in (0.12, 0.2):
                p.cylinder((x, 0.01 * s, z * s), 0.043 * s, 0.022 * s, color, segments=10)  # straps


def _tunic(p, color, s=1.0, hem=0.66, team=False):
    p.team_color(team)
    p.loft([(hem * s, 0.215 * s, 0.15 * s), (0.9 * s, 0.175 * s, 0.12 * s), (1.06 * s, 0.158 * s, 0.108 * s),
            (1.24 * s, 0.192 * s, 0.128 * s), (1.38 * s, 0.215 * s, 0.117 * s), (1.44 * s, 0.11 * s, 0.085 * s)],
           color, segments=18, folds=9, fold_depth=0.035, cap_bottom=True, cap_top=False)
    for side in (-1, 1):  # short sleeves
        p.segment((side * 0.215 * s, 0.0, 1.37 * s), (side * 0.24 * s, 0.005 * s, 1.24 * s), 0.064 * s, 0.058 * s, color, caps=False)
    p.team_color(False)


def _belt(p, color, z, s=1.0, buckle=None):
    p.loft([(z * s - 0.025 * s, 0.168 * s, 0.116 * s), (z * s + 0.025 * s, 0.163 * s, 0.112 * s)], color, segments=18)
    if buckle:
        p.box((0, -0.118 * s, z * s), (0.05 * s, 0.015 * s, 0.045 * s), buckle)


def _cloak(p, color, s=1.0, hem=0.42, clasp=GOLD, team=True):
    """Hangs from the shoulders down the back, rippled in folds; two-sided so it shows from the front."""
    p.team_color(team)
    p.loft([(hem * s, 0.34 * s, 0.24 * s, 0, 0.05 * s), (0.8 * s, 0.29 * s, 0.2 * s, 0, 0.04 * s), (1.2 * s, 0.25 * s, 0.16 * s, 0, 0.02 * s),
            (1.42 * s, 0.235 * s, 0.135 * s), (1.47 * s, 0.13 * s, 0.1 * s)],
           color, segments=16, arc=(-0.15, math.pi + 0.15), folds=11, fold_depth=0.05, two_sided=True)
    # Over the shoulders and closed at the chest by a clasp.
    p.loft([(1.33 * s, 0.235 * s, 0.14 * s), (1.41 * s, 0.222 * s, 0.128 * s), (1.465 * s, 0.11 * s, 0.09 * s)], color, segments=20, cap_bottom=False,
           cap_top=False, two_sided=True, folds=12, fold_depth=0.04, arc=(-0.35, math.pi + 0.35))  # over the shoulders and back
    p.team_color(False)
    if clasp:
        p.cylinder((0.13 * s, -0.125 * s, 1.37 * s), 0.028 * s, 0.02 * s, clasp, segments=10)


def _cuirass(p, s=1.0, color=BRONZE):
    """A muscled bronze cuirass with a moulded chest and a flared lower edge."""
    p.loft([(0.98 * s, 0.182 * s, 0.13 * s), (1.07 * s, 0.168 * s, 0.12 * s), (1.18 * s, 0.19 * s, 0.135 * s),
            (1.27 * s, 0.205 * s, 0.142 * s), (1.37 * s, 0.215 * s, 0.128 * s), (1.43 * s, 0.13 * s, 0.095 * s)], color, segments=18, cap_top=False)
    for x in (-0.07, 0.07):
        p.blob((x * s, -0.12 * s, 1.24 * s), 0.07 * s, color, squash=0.6, subdiv=2, wobble=0.0)  # pectorals
    for z in (1.08, 1.14):
        for x in (-0.04, 0.04):
            p.blob((x * s, -0.118 * s, z * s), 0.035 * s, color, squash=0.7, subdiv=1, wobble=0.0)  # abdomen
    for side in (-1, 1):  # shoulder guards
        p.loft([(1.3 * s, 0.075 * s, 0.09 * s, side * 0.2 * s, 0), (1.4 * s, 0.07 * s, 0.085 * s, side * 0.19 * s, 0),
                (1.45 * s, 0.03 * s, 0.05 * s, side * 0.17 * s, 0)], color, segments=12, cap_bottom=False)


def _pteruges(p, s=1.0, color=LEATHER, top=0.98, length=0.26):
    """Leather strips hanging from the cuirass around the hips."""
    n = 14
    for i in range(n):
        a = 2 * math.pi * i / n
        x, y = math.cos(a), math.sin(a)
        p.box((0.185 * s * x, 0.13 * s * y, (top - length / 2) * s), (0.06 * s, 0.018 * s, length * s), color, rot_z=a + math.pi / 2, taper=1.1)
        p.box((0.19 * s * x, 0.134 * s * y, (top - length + 0.012) * s), (0.062 * s, 0.022 * s, 0.024 * s), BRONZE, rot_z=a + math.pi / 2)  # studded tips


def _greaves(p, s=1.0, color=BRONZE):
    for side in (-1, 1):
        p.segment((side * 0.1 * s, -0.012 * s, 0.47 * s), (side * 0.1 * s, -0.004 * s, 0.14 * s), 0.06 * s, 0.043 * s, color, caps=False)


def _helmet(p, z, s=1.0, crest=CREST, transverse=False, color=BRONZE):
    """A Corinthian-style bronze helmet with cheek guards and a horsehair crest."""
    h = z + 0.07 * s
    p.loft([(h, 0.098 * s, 0.108 * s), (h + 0.08 * s, 0.1 * s, 0.112 * s), (h + 0.14 * s, 0.088 * s, 0.1 * s),
            (h + 0.19 * s, 0.05 * s, 0.06 * s), (h + 0.205 * s, 0.01 * s, 0.01 * s)], color, segments=16, cap_bottom=False)
    for side in (-1, 1):
        p.box((side * 0.075 * s, -0.07 * s, h - 0.01 * s), (0.03 * s, 0.07 * s, 0.11 * s), color, rot_z=side * 0.45, taper=0.8)  # cheek guards
    p.box((0, -0.105 * s, h + 0.07 * s), (0.17 * s, 0.02 * s, 0.03 * s), color)  # brow band
    # The crest: a comb of segments on a crescent over the crown, front to back (or side to side).
    base = h + 0.2 * s
    # Horsehair: a crescent standing on the crown, front to back (or side to side), with a ragged top.
    outer, inner = [], []
    for i in range(17):
        a = math.pi * i / 16
        rough = 0.012 * s * (1 if i % 2 else -1) if 0 < i < 16 else 0.0
        r_out = 0.16 * s + 0.05 * s * math.sin(a) + rough
        outer.append((r_out * math.cos(a), base - 0.02 * s + r_out * 0.75 * math.sin(a)))
        inner.append((0.1 * s * math.cos(a), base - 0.02 * s + 0.1 * s * 0.75 * math.sin(a)))
    for i in range(16):  # convex pieces, so nothing triangulates across the hollow
        p.slab([outer[i], outer[i + 1], inner[i + 1], inner[i]], 0.035 * s, crest, plane="XZ" if transverse else "YZ")
    p.box((0, 0, base + 0.02 * s), (0.03 * s, 0.03 * s, 0.06 * s) if not transverse else (0.03 * s, 0.03 * s, 0.06 * s), color)  # crest holder


def _round_shield(p, center, s=1.0, team=True, face=LINEN, rim=BRONZE):
    cx, cy, cz = center
    p.team_color(team)
    p.disc((cx, cy, cz), 0.36 * s, 0.05 * s, face, segments=20)
    p.team_color(False)
    n = 28
    for i in range(n):  # the rim, as a ring of short rounded segments
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        p.segment((cx + 0.36 * s * math.cos(a0), cy - 0.028 * s, cz + 0.36 * s * math.sin(a0)),
                  (cx + 0.36 * s * math.cos(a1), cy - 0.028 * s, cz + 0.36 * s * math.sin(a1)), 0.02 * s, 0.02 * s, rim, segments=8)
    for i in range(8):  # bronze studs around the face
        a = 2 * math.pi * i / 8
        p.blob((cx + 0.24 * s * math.cos(a), cy - 0.035 * s, cz + 0.24 * s * math.sin(a)), 0.018 * s, rim, squash=0.7, subdiv=1, wobble=0.0)
    p.blob((cx, cy - 0.045 * s, cz), 0.07 * s, rim, squash=0.6, subdiv=2, wobble=0.0)  # boss


def _spear(p, x, s=1.0, length=2.3):
    p.cylinder((x * s, -0.03 * s, 0.0), 0.017 * s, length * s, WOOD, segments=8)
    p.loft([(length * s, 0.03 * s, 0.012 * s, x * s, -0.03 * s), (length * s + 0.12 * s, 0.035 * s, 0.01 * s, x * s, -0.03 * s),
            (length * s + 0.26 * s, 0.002 * s, 0.002 * s, x * s, -0.03 * s)], IRON, segments=8)


# ---------------------------------------------------------------------------------- pieces

def citizen(seed=31):
    p = _figure("SM_Citizen", seed)
    _body(p, SKIN[0])
    _sandals(p)
    _tunic(p, "#6f7a3a", hem=0.62)
    _belt(p, DARK_LEATHER, 1.0)
    # A shawl over one shoulder, and a satchel on a strap.
    p.loft([(1.12, 0.21, 0.135), (1.3, 0.215, 0.13), (1.44, 0.15, 0.1)], "#7a3b22", segments=14, arc=(math.pi * 0.25, math.pi * 1.35),
           folds=6, fold_depth=0.06, two_sided=True)
    p.segment((0.16, -0.11, 1.4), (-0.15, -0.12, 0.98), 0.012, 0.012, DARK_LEATHER, segments=6)
    p.box((-0.19, -0.1, 0.94), (0.16, 0.06, 0.15), "#7d6448", taper=0.9)
    _head(p, SKIN[0], HAIR[1])
    return p


def herald(seed=32):
    p = _figure("SM_Herald", seed)
    _body(p, SKIN[3], legs=WOOL)
    _sandals(p, boots=True, color=DARK_LEATHER)
    _tunic(p, LINEN, hem=0.58, team=True)
    _belt(p, LEATHER, 1.0, buckle=GOLD)
    _cloak(p, WOOL, hem=0.7, team=False)
    p.cylinder((0.29, -0.09, 0.78), 0.03, 0.3, "#efe6cf", segments=10)  # the scroll in hand
    for z in (0.78, 1.08):
        p.cylinder((0.29, -0.09, z - 0.01), 0.034, 0.02, WOOD, segments=10)
    z = _head(p, SKIN[3], HAIR[3], moustache=HAIR[3])
    p.loft([(z + 0.2, 0.095, 0.105), (z + 0.27, 0.085, 0.095), (z + 0.3, 0.04, 0.05)], "#8f2f24", segments=14, cap_bottom=False)  # a felt cap
    return p


def captain(seed=33):
    p = _figure("SM_Captain", seed)
    _body(p, SKIN[1])
    _sandals(p)
    _tunic(p, "#8b2a22", hem=0.7)
    _pteruges(p)
    _cuirass(p)
    _greaves(p)
    _cloak(p, LINEN, hem=0.4)
    z = _head(p, SKIN[1], HAIR[0], beard=HAIR[0])
    _helmet(p, z, transverse=True)
    _spear(p, 0.33)
    return p


def soldier(seed=34):
    p = _figure("SM_Soldier", seed)
    _body(p, SKIN[2])
    _sandals(p)
    _tunic(p, LINEN, hem=0.7, team=True)
    _pteruges(p)
    _cuirass(p)
    _greaves(p)
    z = _head(p, SKIN[2], HAIR[2], moustache=HAIR[2])
    _helmet(p, z)
    _round_shield(p, (-0.36, -0.16, 1.0))
    _spear(p, 0.33, length=2.2)
    return p


def leader(seed=35):
    """The ruler: a long team-coloured robe with a gold-edged sash, a cloak with a clasp, a crown, a sword."""
    s = 1.04
    p = _figure("SM_Leader", seed)
    _body(p, SKIN[0], s=s)
    _sandals(p, s=s, color=DARK_LEATHER, boots=True)
    p.team_color(True)
    p.loft([(0.06 * s, 0.25 * s, 0.2 * s), (0.5 * s, 0.215 * s, 0.16 * s), (0.92 * s, 0.18 * s, 0.125 * s), (1.06 * s, 0.162 * s, 0.112 * s),
            (1.24 * s, 0.195 * s, 0.13 * s), (1.38 * s, 0.218 * s, 0.12 * s), (1.45 * s, 0.11 * s, 0.085 * s)],
           LINEN, segments=20, folds=10, fold_depth=0.04, cap_top=False)  # the robe
    for side in (-1, 1):  # long sleeves
        p.segment((side * 0.215 * s, 0.0, 1.37 * s), (side * 0.258 * s, 0.005 * s, 0.93 * s), 0.066 * s, 0.05 * s, LINEN, caps=False)
    p.team_color(False)
    p.loft([(0.06 * s, 0.252 * s, 0.202 * s), (0.11 * s, 0.245 * s, 0.196 * s)], GOLD, segments=20, folds=10, fold_depth=0.04)  # gold hem
    _belt(p, GOLD, 1.03, s=s, buckle=BRONZE)
    # A sash from the right shoulder to the left hip.
    p.segment((0.18 * s, -0.122 * s, 1.4 * s), (-0.15 * s, -0.135 * s, 1.0 * s), 0.02 * s, 0.02 * s, "#6a1420", segments=6)
    _cloak(p, "#4a2a6a", s=s, hem=0.2, team=False)
    z = _head(p, SKIN[0], HAIR[3], s=s, beard=HAIR[3])
    # A gold crown: a band with five points.
    cz = z + 0.215 * s
    p.loft([(cz, 0.097 * s, 0.107 * s, 0, 0.008 * s), (cz + 0.05 * s, 0.1 * s, 0.11 * s, 0, 0.008 * s)], GOLD, segments=18, cap_bottom=False, cap_top=False, two_sided=True)
    for i in range(5):
        a = -math.pi / 2 + 2 * math.pi * i / 5
        p.cone((0.098 * s * math.cos(a), 0.008 * s + 0.108 * s * math.sin(a), cz + 0.045 * s), 0.022 * s, 0.06 * s, GOLD, segments=6)
    # A sword in its scabbard at the left hip.
    p.segment((-0.2 * s, -0.06 * s, 1.0 * s), (-0.24 * s, -0.02 * s, 0.38 * s), 0.025 * s, 0.02 * s, DARK_LEATHER, segments=8)
    p.box((-0.198 * s, -0.062 * s, 1.03 * s), (0.14 * s, 0.03 * s, 0.025 * s), GOLD, rot_z=0.3)  # guard
    p.segment((-0.196 * s, -0.064 * s, 1.04 * s), (-0.19 * s, -0.07 * s, 1.15 * s), 0.016 * s, 0.016 * s, LEATHER, segments=6)  # grip
    return p


def ship(seed=36):
    """A Classical war galley, side-on along X: hull, bow ram, oars, a mast and a team-painted sail."""
    p = Piece("SM_Ship", seed)
    p.box((0, 0, 0.45), (6.0, 1.3, 0.9), WOOD, taper=1.3)  # hull, wider at the gunwale
    p.box((0, 0, 0.92), (5.6, 1.5, 0.08), "#8a6a48")  # deck
    p.box((3.25, 0, 0.35), (0.7, 0.35, 0.3), BRONZE)  # ram
    p.box((-2.9, 0, 1.25), (0.35, 0.4, 0.7), WOOD)  # raised stern
    for i in range(6):
        x = -2.0 + 0.8 * i
        for side in (-1, 1):
            p.box((x, side * 1.15, 0.55), (0.08, 0.9, 0.06), WOOD, rot_z=side * 0.25)  # oars
    p.cylinder((0.3, 0, 0.95), 0.07, 3.4, WOOD, segments=6)  # mast
    p.box((0.3, 0, 3.2), (0.1, 2.6, 0.08), WOOD)  # yard
    p.team_color(True)
    p.box((0.3, 0, 2.4), (0.06, 2.4, 1.5), LINEN, taper=0.9)  # the sail
    p.team_color(False)
    return p


def boat(seed=37):
    """The small boat an embarked land unit rides in, a team-painted strake along its side."""
    p = Piece("SM_Boat", seed)
    p.box((0, 0, 0.22), (2.6, 0.9, 0.44), WOOD, taper=1.3)
    p.team_color(True)
    for side in (-1, 1):
        p.box((0, side * 0.56, 0.4), (2.5, 0.1, 0.14), LINEN)  # painted strakes
    p.team_color(False)
    for side in (-1, 1):
        p.box((0.2, side * 0.75, 0.35), (0.06, 0.7, 0.05), WOOD, rot_z=side * 0.3)
    return p


# ---------------------------------------------------------------------------------- later and other arms (map tokens)

OLIVE = "#5a5f3a"
STEEL = "#6f7478"
NAVY = "#2e3440"
HORSE = ["#6b4226", "#4a2f1c"]


def _bow(p, x, s=1.0):
    """A self bow held upright in the left hand, its string toward the body."""
    pts = [(x * s, -0.06 * s, (0.95 + 0.62 * math.sin(a)) * s, a) for a in [(-0.5 + i / 8.0) * math.pi * 0.9 for i in range(9)]]
    for (x0, y0, z0, a0), (x1, y1, z1, a1) in zip(pts, pts[1:]):
        b0, b1 = 0.08 * math.cos(a0) * s, 0.08 * math.cos(a1) * s
        p.segment((x0, y0 - b0, z0), (x1, y1 - b1, z1), 0.016 * s, 0.016 * s, WOOD, segments=6, caps=False)
    p.segment(pts[0][:3], pts[-1][:3], 0.004 * s, 0.004 * s, LINEN, segments=4, caps=False)


def archer(seed=38):
    p = _figure("SM_Archer", seed)
    _body(p, SKIN[3])
    _sandals(p)
    _tunic(p, LINEN, hem=0.68, team=True)
    _belt(p, LEATHER, 1.0)
    p.cylinder((0.12, 0.14, 0.95), 0.06, 0.5, LEATHER, segments=8)  # quiver on the back
    for i in range(4):
        p.segment((0.1 + 0.015 * i, 0.14, 1.4), (0.1 + 0.015 * i, 0.14, 1.52), 0.01, 0.01, CREST, segments=4, caps=False)
    _bow(p, -0.3)
    z = _head(p, SKIN[3], HAIR[1])
    p.loft([(z + 0.19, 0.095, 0.105), (z + 0.26, 0.07, 0.08), (z + 0.31, 0.01, 0.01)], DARK_LEATHER, segments=12, cap_bottom=False)  # leather cap
    return p


def rider(seed=39):
    """A horseman: the horse side-on along X, the rider astride with a lance, a team saddle cloth."""
    p = _figure("SM_Rider", seed)
    p.segment((-0.75, 0, 1.2), (0.75, 0, 1.25), 0.3, 0.32, HORSE[0], segments=12)  # barrel
    p.segment((0.75, 0, 1.3), (1.05, 0, 1.75), 0.17, 0.12, HORSE[0], segments=10)  # neck
    p.segment((1.05, 0, 1.78), (1.38, 0, 1.6), 0.11, 0.07, HORSE[0], segments=10)  # head
    p.segment((0.82, 0, 1.6), (1.02, 0, 1.92), 0.04, 0.03, HORSE[1], segments=6)  # mane
    for x in (-0.6, 0.6):
        for y in (-0.15, 0.15):
            p.segment((x, y, 1.0), (x + 0.03, y, 0.5), 0.08, 0.06, HORSE[0], segments=8)
            p.segment((x + 0.03, y, 0.5), (x, y, 0.05), 0.05, 0.04, HORSE[0], segments=8)
            p.cylinder((x, y, 0.0), 0.055, 0.08, DARK_LEATHER, segments=8)  # hooves
    p.segment((-0.78, 0, 1.3), (-1.0, 0, 0.8), 0.07, 0.03, HORSE[1], segments=6)  # tail
    p.team_color(True)
    p.box((-0.05, 0, 1.5), (0.7, 0.72, 0.05), LINEN)  # saddle cloth
    p.team_color(False)
    # The rider, seated: torso, legs down the flanks, arms, head and helmet, a lance.
    off = 0.63
    p.loft([(0.92 + off, 0.16, 0.105), (1.06 + off, 0.145, 0.095), (1.24 + off, 0.18, 0.115), (1.38 + off, 0.205, 0.105),
            (1.45 + off, 0.09, 0.07)], LINEN, segments=14)
    for side in (-1, 1):
        p.segment((0.0, side * 0.12, 1.55), (0.12, side * 0.36, 1.15), 0.07, 0.055, LEATHER, segments=8)
        p.segment((0.12, side * 0.36, 1.15), (0.05, side * 0.36, 0.8), 0.05, 0.04, LEATHER, segments=8)
        p.segment((0.0, side * 0.215, 1.37 + off), (0.15, side * 0.26, 1.15 + off), 0.045, 0.035, SKIN[1], segments=8)
    hz = 1.49 + off
    p.segment((0, 0, 1.4 + off), (0, 0, hz + 0.03), 0.05, 0.045, SKIN[1])
    p.blob((0, 0, hz + 0.13), 0.1, SKIN[1], squash=1.25, subdiv=2, wobble=0.0)
    p.loft([(hz + 0.1, 0.1, 0.11), (hz + 0.2, 0.09, 0.1), (hz + 0.27, 0.01, 0.01)], BRONZE, segments=12, cap_bottom=False)
    p.segment((0.2, -0.3, 0.9), (1.1, -0.3, 3.0), 0.02, 0.015, WOOD, segments=6, caps=False)  # lance
    p.cone((1.1, -0.3, 3.0), 0.03, 0.2, IRON, segments=6)
    return p


def siege(seed=40):
    """A wooden siege engine (catapult) on wheels, a team pennant on its frame."""
    p = Piece("SM_Siege", seed)
    for y in (-0.5, 0.5):
        p.box((0, y, 0.45), (2.4, 0.14, 0.14), WOOD)
        for x in (-0.8, 0.8):
            p.disc((x, y * 1.25, 0.32), 0.32, 0.08, WOOD, segments=12)
        p.segment((-0.2, y, 0.5), (0.1, y, 1.4), 0.07, 0.06, WOOD, segments=6, caps=False)
    p.box((0.1, 0, 1.4), (0.16, 1.1, 0.16), WOOD)
    p.segment((0.9, 0, 0.6), (-0.9, 0, 1.9), 0.06, 0.05, WOOD, segments=6, caps=False)  # the arm, cocked
    p.box((-0.95, 0, 1.95), (0.35, 0.35, 0.15), LEATHER)  # the cup
    p.blob((-0.95, 0, 2.1), 0.14, "#8a857b", squash=0.9)
    p.cylinder((1.1, 0.5, 0.5), 0.025, 1.6, WOOD, segments=5)
    p.team_color(True)
    p.box((1.35, 0.5, 1.9), (0.5, 0.03, 0.32), LINEN)
    p.team_color(False)
    return p


def musketeer(seed=41):
    """Gunpowder infantry: a long team-coloured coat, a tricorne, a musket shouldered upright."""
    p = _figure("SM_Musketeer", seed)
    _body(p, SKIN[0], legs="#d8d0bc")
    _sandals(p, boots=True, color=DARK_LEATHER)
    _tunic(p, LINEN, hem=0.55, team=True)
    _belt(p, "#e8e2d6", 1.0, buckle=GOLD)
    p.segment((0.16, -0.11, 1.4), (-0.15, -0.12, 0.98), 0.018, 0.018, "#e8e2d6", segments=6)  # cross belt
    z = _head(p, SKIN[0], HAIR[3], moustache=HAIR[3])
    p.loft([(z + 0.18, 0.15, 0.15), (z + 0.21, 0.16, 0.16), (z + 0.23, 0.09, 0.1), (z + 0.31, 0.08, 0.09), (z + 0.33, 0.02, 0.02)], NAVY,
           segments=3)  # tricorne: three corners
    p.cylinder((0.3, -0.04, 0.6), 0.022, 1.25, WOOD, segments=8)  # stock and barrel
    p.cylinder((0.3, -0.04, 1.85), 0.014, 0.12, IRON, segments=6)
    p.cone((0.3, -0.04, 1.97), 0.012, 0.25, IRON, segments=4)  # bayonet
    return p


def rifleman(seed=42):
    """Modern infantry: a team-coloured field tunic, olive trousers, a steel helmet, a rifle."""
    p = _figure("SM_Rifleman", seed)
    _body(p, SKIN[1], legs=OLIVE)
    _sandals(p, boots=True, color=DARK_LEATHER)
    _tunic(p, LINEN, hem=0.78, team=True)
    _belt(p, OLIVE, 1.0, buckle=STEEL)
    p.box((0.0, 0.16, 1.15), (0.3, 0.14, 0.3), OLIVE)  # pack
    z = _head(p, SKIN[1], HAIR[0])
    p.loft([(z + 0.13, 0.13, 0.14), (z + 0.15, 0.12, 0.13), (z + 0.22, 0.1, 0.11), (z + 0.28, 0.05, 0.06), (z + 0.3, 0.01, 0.01)], OLIVE,
           segments=14)  # steel helmet
    p.segment((0.28, -0.12, 0.8), (0.34, -0.05, 1.75), 0.022, 0.016, WOOD, segments=6, caps=False)  # rifle at the slope
    p.segment((0.33, -0.06, 1.55), (0.35, -0.04, 1.95), 0.012, 0.012, STEEL, segments=6, caps=False)
    return p


def tank(seed=43):
    """An armoured fighting vehicle along X: tracks, hull, turret and gun; team panels on the turret."""
    p = Piece("SM_Tank", seed)
    for y in (-0.95, 0.95):
        p.box((0, y, 0.45), (4.6, 0.6, 0.9), "#3a3a36", taper=0.9)  # tracks
        for x in (-1.7, -0.85, 0.0, 0.85, 1.7):
            p.disc((x, y * 1.12, 0.42), 0.33, 0.06, STEEL, segments=10)
    p.box((0, 0, 1.1), (4.4, 1.9, 0.6), OLIVE, taper=0.9)  # hull
    p.box((-0.2, 0, 1.65), (1.8, 1.4, 0.55), OLIVE, taper=0.85)  # turret
    p.team_color(True)
    for y in (-0.71, 0.71):
        p.box((-0.2, y, 1.65), (1.0, 0.03, 0.35), LINEN)
    p.team_color(False)
    p.segment((0.6, 0, 1.7), (3.2, 0, 1.75), 0.09, 0.07, STEEL, segments=8, caps=False)  # gun
    return p


def plane(seed=44):
    """A propeller fighter along X, team roundels on the wings."""
    p = Piece("SM_Plane", seed)
    p.segment((-2.6, 0, 1.0), (2.2, 0, 1.0), 0.18, 0.42, OLIVE, segments=12)  # fuselage
    p.box((0.6, 0, 0.95), (1.4, 6.4, 0.1), OLIVE)  # wings
    p.box((-2.4, 0, 1.05), (0.6, 2.0, 0.06), OLIVE)  # tailplane
    p.box((-2.4, 0, 1.5), (0.7, 0.06, 0.9), OLIVE, taper=0.6)  # fin
    p.blob((0.9, 0, 1.38), 0.32, "#8fb3c9", squash=0.7)  # canopy
    p.box((2.45, 0, 1.0), (0.05, 0.12, 1.8), DARK_LEATHER)  # propeller
    p.team_color(True)
    for y in (-2.3, 2.3):
        p.cylinder((0.6, y, 1.0), 0.42, 0.02, LINEN, segments=14)  # roundels
    p.team_color(False)
    for y in (-0.9, 0.9):
        p.segment((0.8, y, 0.95), (0.8, y * 1.1, 0.25), 0.04, 0.04, STEEL, segments=6, caps=False)  # undercarriage
        p.disc((0.8, y * 1.1, 0.2), 0.17, 0.08, "#222222", segments=10)
    return p


def steamship(seed=45):
    """An ironclad steamer along X: steel hull, deckhouse, a funnel banded in the owner's colour, a gun turret."""
    p = Piece("SM_Steamship", seed)
    p.box((0, 0, 0.5), (7.0, 1.5, 1.0), "#4b4f55", taper=1.2)  # hull
    p.box((3.7, 0, 0.6), (0.6, 0.6, 0.6), "#4b4f55", taper=0.4)  # bow
    p.box((0, 0, 1.05), (6.6, 1.6, 0.1), "#8a6a48")  # deck
    p.box((-0.6, 0, 1.5), (2.0, 1.1, 0.8), "#c9c6bf")  # deckhouse
    p.cylinder((-0.3, 0, 1.9), 0.3, 1.5, NAVY, segments=10)  # funnel
    p.team_color(True)
    p.cylinder((-0.3, 0, 2.9), 0.31, 0.3, LINEN, segments=10)
    p.team_color(False)
    p.cylinder((2.0, 0, 1.1), 0.5, 0.45, STEEL, segments=10)  # turret
    p.segment((2.2, 0, 1.35), (3.4, 0, 1.4), 0.07, 0.06, STEEL, segments=6, caps=False)
    p.cylinder((-2.2, 0, 1.1), 0.05, 2.6, WOOD, segments=6)  # mast
    return p


PIECES = [citizen, herald, captain, soldier, leader, ship, boat, archer, rider, siege, musketeer, rifleman, tank, plane, steamship]
PEOPLE = [citizen, herald, captain, soldier, leader]
