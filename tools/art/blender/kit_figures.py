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


PIECES = [citizen, herald, captain, soldier, leader, ship, boat]
PEOPLE = [citizen, herald, captain, soldier, leader]
