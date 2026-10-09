"""Nature kit: trees, bushes and rocks shared by every civ and era (world doc, The model sets), and the map's
terrain features (reeds, palms, coral, ice, volcanoes, fissures, burnt trees) and natural wonders' landforms."""
import math

import kitlib
from kitlib import Piece

BARK = "#5b4330"
LEAF = ["#4f7a35", "#5c8a3c", "#46702f"]
NEEDLE = ["#2f5b3a", "#356543", "#2a5233"]
STONE = ["#8a857b", "#7d786f", "#968f84"]

kitlib.patterns({"wood": BARK, "foliage": LEAF + NEEDLE, "stone": STONE})


def tree_broadleaf(seed=1):
    p = Piece("SM_Tree_Broadleaf", seed)
    p.cylinder((0, 0, 0), 0.28, 2.8, BARK, segments=7, radius_top=0.18)
    p.blob((0.0, 0.0, 3.9), 1.7, LEAF[0], squash=0.85)
    p.blob((0.9, 0.4, 3.4), 1.25, LEAF[1], squash=0.85)
    p.blob((-0.8, -0.5, 3.5), 1.2, LEAF[2], squash=0.85)
    p.blob((0.1, -0.8, 4.6), 1.0, LEAF[1], squash=0.9)
    return p


def tree_conifer(seed=2):
    p = Piece("SM_Tree_Conifer", seed)
    p.cylinder((0, 0, 0), 0.22, 1.4, BARK, segments=6)
    p.cone((0, 0, 1.0), 1.7, 2.6, NEEDLE[0], segments=8)
    p.cone((0, 0, 2.6), 1.3, 2.2, NEEDLE[1], segments=8)
    p.cone((0, 0, 3.9), 0.9, 2.0, NEEDLE[2], segments=8)
    return p


def bush(seed=3):
    p = Piece("SM_Bush", seed)
    p.blob((0, 0, 0.45), 0.7, LEAF[1], squash=0.7)
    p.blob((0.5, 0.2, 0.35), 0.5, LEAF[2], squash=0.7)
    p.blob((-0.4, -0.3, 0.35), 0.45, LEAF[0], squash=0.7)
    return p


def rocks(seed=4):
    p = Piece("SM_Rocks", seed)
    p.blob((0, 0, 0.35), 0.75, STONE[0], squash=0.6, wobble=0.25)
    p.blob((0.9, 0.3, 0.25), 0.5, STONE[1], squash=0.65, wobble=0.25)
    p.blob((-0.6, 0.6, 0.2), 0.4, STONE[2], squash=0.7, wobble=0.25)
    return p


# ---------------------------------------------------------------- features (map tokens, about 4-9 m across)

REED = ["#8a8f4e", "#9a9a58", "#7b8445"]
MUD = "#5f5a44"
WATER = "#3f7fa6"
SAND = ["#d8c48f", "#cbb47c"]
PALM = ["#5c8a3c", "#4f7a35"]
CORAL = ["#d77a6a", "#e0a35a", "#b8648f"]
ICE = ["#e8f0f4", "#d6e4ec"]
ASH = ["#4a3f3a", "#3a322e"]
LAVA = "#e0602a"
SNOW = "#f2f4f6"
RED_ROCK = ["#b8653f", "#a5583a"]
GREEN_HILL = ["#6f8f43", "#7d9a4a"]

kitlib.patterns({"foliage": REED + PALM + GREEN_HILL, "water": WATER, "stone": ASH + RED_ROCK + [MUD] + SAND, "plaster": ICE + [SNOW]})


def reeds(seed=5):
    """Marsh: tufts of reeds round standing water."""
    p = Piece("SM_Reeds", seed)
    p.cylinder((0, 0, 0), 1.8, 0.05, WATER, segments=10)
    for k in range(9):
        a, r = 2.3 * k, 1.2 + 0.25 * (k % 3)
        x, y = r * math.cos(a), r * math.sin(a)
        for j in range(3):
            p.segment((x + 0.15 * j, y, 0), (x + 0.15 * j + 0.1 * (j - 1), y + 0.05, 1.1 + 0.2 * j), 0.05, 0.02, REED[(k + j) % 3],
                      segments=4, caps=False)
    return p


def palms(seed=6):
    """Oasis: palms round a pool on sand."""
    p = Piece("SM_Palms", seed)
    p.cylinder((0, 0, 0), 2.4, 0.06, SAND[0], segments=12)
    p.cylinder((0, 0, 0.02), 1.4, 0.06, WATER, segments=12)
    for k, (x, y, h) in enumerate(((1.8, 0.6, 3.2), (-1.4, 1.5, 2.7), (0.4, -2.0, 3.0))):
        top = (x + 0.4, y + 0.2, h)
        p.segment((x, y, 0), top, 0.14, 0.1, BARK, segments=5, caps=False)
        for j in range(5):
            a = 2 * math.pi * j / 5 + k
            p.segment(top, (top[0] + 1.3 * math.cos(a), top[1] + 1.3 * math.sin(a), h - 0.6), 0.16, 0.03, PALM[j % 2], segments=4, caps=False)
    return p


def coral(seed=7):
    """Reef: branching coral heads just under the water."""
    p = Piece("SM_Coral", seed)
    for k in range(7):
        a, r = 2.5 * k, 0.6 + 0.35 * k
        x, y = r * math.cos(a), r * math.sin(a)
        p.blob((x, y, 0.15), 0.45 + 0.1 * (k % 3), CORAL[k % 3], squash=0.6, wobble=0.3)
        p.segment((x, y, 0.2), (x + 0.2, y - 0.1, 0.8), 0.1, 0.05, CORAL[(k + 1) % 3], segments=4, caps=False)
    return p


def ice_floe(seed=8):
    """Ice: broken floes and a pressure ridge."""
    p = Piece("SM_IceFloe", seed)
    for k, (x, y, s) in enumerate(((0, 0, 2.2), (2.4, 1.2, 1.2), (-2.0, 1.6, 1.0), (1.0, -2.2, 1.1))):
        p.cylinder((x, y, 0), s, 0.35, ICE[k % 2], segments=6 + k)
    p.box((0.3, 0.2, 0.6), (2.4, 0.6, 0.6), ICE[1], rot_z=0.5, taper=0.4)
    return p


def volcano(seed=9):
    """A smoking cone with lava in the crater and down one flank."""
    p = Piece("SM_Volcano", seed)
    p.loft([(0.0, 4.2, 4.2), (2.4, 2.4, 2.4), (4.4, 1.0, 1.0), (4.1, 0.7, 0.7)], ASH[0], segments=12, cap_top=False)
    p.cylinder((0, 0, 3.6), 0.75, 0.6, LAVA, segments=10)
    for q in p.box((1.6, 0, 2.0), (2.8, 0.5, 0.15), LAVA, rot_z=0.2):
        q.co.z += -(q.co.x - 1.6) * 0.9  # a lava tongue down the flank
    p.blob((0.2, 0, 5.6), 0.9, "#9a948d", squash=1.2)
    p.blob((0.6, 0.3, 6.8), 0.7, "#aaa49d", squash=1.2)
    return p


def fumarole(seed=10):
    """Geothermal fissure: a steaming crack in pale crust."""
    p = Piece("SM_Fumarole", seed)
    p.cylinder((0, 0, 0), 2.0, 0.08, "#c7b48a", segments=10)
    p.box((0, 0, 0.1), (3.0, 0.35, 0.06), ASH[1], rot_z=0.4)
    for k in range(3):
        p.blob((-0.8 + 0.8 * k, -0.3 + 0.3 * k, 0.8 + 0.5 * k), 0.45 + 0.1 * k, "#e8e6e2", squash=1.3)
    return p


def burnt_tree(seed=11):
    """A charred trunk with bare branches."""
    p = Piece("SM_BurntTree", seed)
    p.cylinder((0, 0, 0), 0.26, 3.0, ASH[1], segments=6, radius_top=0.12)
    for k in range(4):
        a = 1.6 * k
        p.segment((0, 0, 1.8 + 0.3 * k), (0.9 * math.cos(a), 0.9 * math.sin(a), 2.6 + 0.3 * k), 0.08, 0.03, ASH[0], segments=4, caps=False)
    return p


# Natural wonders: a few landforms the map gives each wonder (the rest of its look is the plot's colour).

def peak(seed=12):
    """A great snow-capped peak with a lesser one."""
    p = Piece("SM_Peak", seed)
    p.cone((0, 0, 0), 4.2, 8.5, STONE[0], segments=7)
    p.cone((0, 0, 5.6), 1.55, 2.95, SNOW, segments=7)
    p.cone((3.0, 1.8, 0), 2.2, 4.0, STONE[1], segments=6)
    return p


def mesa(seed=13):
    """A flat-topped red monolith."""
    p = Piece("SM_Mesa", seed)
    p.loft([(0.0, 4.0, 2.8), (0.4, 3.6, 2.5), (3.0, 3.3, 2.2), (3.4, 2.9, 1.9), (3.5, 2.3, 1.4)], RED_ROCK[0], segments=14, folds=5,
           fold_depth=0.06)
    return p


def spires(seed=14):
    """Clustered rock spires (karst, columns, needles)."""
    p = Piece("SM_Spires", seed)
    for k in range(9):
        a, r = 2.4 * k, 0.6 + 0.4 * (k % 5)
        x, y = r * math.cos(a), r * math.sin(a)
        p.cylinder((x, y, 0), 0.55, 2.4 + 1.2 * ((k * 7) % 4), STONE[k % 3], segments=6, radius_top=0.2)
    return p


def pool(seed=15):
    """A bright lake in a rim of rock (crater lakes, cenotes, salt lakes, terraces)."""
    p = Piece("SM_Pool", seed)
    p.loft([(0.0, 4.0, 4.0), (1.2, 3.4, 3.4), (1.0, 2.6, 2.6)], STONE[1], segments=14, cap_top=False)
    p.cylinder((0, 0, 0), 2.7, 0.7, WATER, segments=14)
    return p


def cliffs(seed=16):
    """A sheer pale cliff face with a green top."""
    p = Piece("SM_Cliffs", seed)
    p.box((0, 0, 2.5), (8.0, 2.5, 5.0), "#e6e1d6")
    p.box((0, 0.4, 5.1), (8.2, 3.0, 0.3), GREEN_HILL[0])
    for x in (-2.5, 1.0, 3.0):
        p.box((x, -1.3, 1.5), (1.0, 0.4, 3.0), "#d6d0c3")
    return p


def mounds(seed=17):
    """Rounded hills in rows."""
    p = Piece("SM_Mounds", seed)
    for k in range(7):
        a, r = 2.2 * k, 0.4 + 0.5 * k
        p.blob((r * math.cos(a), r * math.sin(a), 0.2), 1.0, GREEN_HILL[k % 2], squash=0.9, subdiv=2, wobble=0.04)
    return p


PIECES = [tree_broadleaf, tree_conifer, bush, rocks, reeds, palms, coral, ice_floe, volcano, fumarole, burnt_tree, peak, mesa, spires,
          pool, cliffs, mounds]
