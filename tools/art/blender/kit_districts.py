"""Districts kit: one map token per specialty district, Classical temperate style like the city kit.
Like the Fields kit each fits a footprint about 9 m across and the map draws it at a tenth of that.
A small pennant on each is the team slot, so a district shows whose it is.
"""
import math

import kitlib
from kitlib import Piece

WALL = ["#e3d9c3", "#d9cdb2", "#ece4d2"]
ROOF = ["#a9533a", "#b45f42", "#9c4b34"]
MARBLE = ["#ebe7df", "#dedad1"]
STONE = ["#a39c8f", "#938c80", "#b1aa9d"]
BRICK = ["#9a5a42", "#8a4f3a"]
WOOD = ["#6b4c33", "#5a3f2b"]
DARK = "#3a2f28"
GOLD = "#c9a43c"
WATER = "#3f7fa6"
LEAF = ["#4f7a35", "#5c8a3c", "#46702f"]
LAWN = "#7fa046"
CANVAS = ["#e2d6bc", "#cdbf9f"]
IRON = ["#5d5f63", "#4a4c50"]
PAVING = "#c8bfae"
FLAG = "#e8e2d6"

kitlib.patterns({"plaster": WALL, "rooftile": ROOF + BRICK, "stone": MARBLE + STONE + [PAVING], "wood": WOOD,
                 "foliage": LEAF + [LAWN], "cloth": CANVAS + [FLAG], "metal": IRON + [GOLD], "water": WATER})


def _pennant(p, x, y, h=4.5):
    p.cylinder((x, y, 0), 0.07, h, WOOD[0], segments=5)
    p.team_color(True)
    p.box((x + 0.5, y, h - 0.55), (1.0, 0.05, 0.8), FLAG)
    p.team_color(False)


def _base(p, w=9.0, d=8.0, color=PAVING):
    p.box((0, 0, 0.1), (w, d, 0.2), color)


def _house(p, x, y, rot=0.0, s=1.0, k=0):
    w, d, h = 2.2 * s, 1.8 * s, 1.5 * s
    p.box((x, y, 0.2 + h / 2), (w, d, h), WALL[k % 3], rot_z=rot)
    p.gable((x, y, 0.2 + h), w, d, 0.8 * s, ROOF[k % 3], overhang=0.12, rot_z=rot)


def _hall(p, x, y, w, d, h, wall=WALL[0], roof=ROOF[0], rot=0.0):
    p.box((x, y, 0.2 + h / 2), (w, d, h), wall, rot_z=rot)
    p.hip((x, y, 0.2 + h), w, d, 0.9, roof, overhang=0.15, rot_z=rot)


def _colonnade(p, x0, x1, y, h, n, r=0.18):
    for i in range(n):
        x = x0 + (x1 - x0) * i / max(1, n - 1)
        p.cylinder((x, y, 0.2), r, h, MARBLE[0], segments=7)


def _tree(p, x, y, s=1.0):
    p.cylinder((x, y, 0.2), 0.12 * s, 0.9 * s, WOOD[0], segments=5)
    p.blob((x, y, 0.2 + 1.4 * s), 0.75 * s, LEAF[int(x * 7 + y * 3) % 3], squash=0.9)


def campus(seed=41):
    """A library hall and a domed observatory."""
    p = Piece("SM_Campus", seed)
    _base(p)
    _hall(p, -1.6, 0.8, 4.6, 3.0, 2.2, WALL[0], ROOF[0])
    _colonnade(p, -3.6, 0.4, -0.9, 2.0, 5)
    p.cylinder((2.6, -1.2, 0.2), 1.3, 2.0, MARBLE[1], segments=12)
    v = p.blob((2.6, -1.2, 2.2), 1.3, IRON[0], squash=0.85, subdiv=2, wobble=0.0)
    for q in v:
        q.co.z = max(q.co.z, 2.2)
    _tree(p, 3.2, 2.6)
    _pennant(p, -3.9, 2.9)
    return p


def holy_site(seed=42):
    """A shrine on a stepped mound among old trees."""
    p = Piece("SM_HolySite", seed)
    _base(p, color=LAWN)
    p.box((0, 0, 0.45), (5.0, 5.0, 0.5), STONE[0])
    p.box((0, 0, 0.85), (3.8, 3.8, 0.4), STONE[2])
    p.box((0, 0, 1.05 + 1.2), (2.6, 2.6, 2.4), MARBLE[1])
    p.cone((0, 0, 3.45), 2.0, 2.6, GOLD, segments=4)
    p.box((0, -1.32, 1.05 + 0.8), (0.9, 0.1, 1.6), DARK)
    for x, y in ((-3.5, -2.8), (3.4, -2.9), (-3.3, 2.9), (3.5, 2.7)):
        _tree(p, x, y, 1.1)
    _pennant(p, 1.9, -2.6, 4.0)
    return p


def commercial_hub(seed=43):
    """A warehouse, market awnings and stacked goods."""
    p = Piece("SM_CommercialHub", seed)
    _base(p)
    _hall(p, 1.6, 1.4, 4.4, 3.2, 2.6, WALL[1], ROOF[1])
    for k, x in enumerate((-3.2, -1.6, 0.0)):
        p.box((x, -2.0, 0.6), (1.3, 0.9, 0.8), WOOD[k % 2])
        p.box((x, -2.0, 1.45), (1.6, 1.2, 0.08), CANVAS[k % 2] if k != 1 else ROOF[1], taper=0.6)
    for x, y in ((-3.0, 1.6), (-2.2, 1.9), (-2.6, 2.6)):
        p.box((x, y, 0.55), (0.7, 0.7, 0.7), WOOD[1])
    _pennant(p, 3.9, -2.8)
    return p


def harbor(seed=44):
    """Quays and a pier with a crane and a warehouse (stands in the water)."""
    p = Piece("SM_Harbor", seed)
    p.box((0, 2.6, 0.35), (9.0, 3.0, 0.7), STONE[0])
    p.box((-1.0, -0.8, 0.3), (1.6, 6.0, 0.4), WOOD[0])
    for y in (-3.4, -1.8, -0.2):
        for x in (-1.7, -0.3):
            p.cylinder((x, y, -0.5), 0.12, 0.8, WOOD[1], segments=5)
    _hall(p, 2.2, 2.6, 4.0, 2.4, 1.8, WALL[2], ROOF[2])
    p.segment((-1.0, 1.6, 0.7), (-1.0, 1.6, 4.2), 0.14, 0.1, WOOD[1], segments=5, caps=False)
    p.segment((-1.0, 1.6, 4.1), (-1.0, -1.6, 3.4), 0.1, 0.08, WOOD[1], segments=5, caps=False)
    p.box((2.6, -1.8, 0.35), (3.0, 1.1, 0.7), WOOD[0], rot_z=0.15, taper=1.2)
    _pennant(p, -4.0, 3.4)
    return p


def theater_square(seed=45):
    """A stepped half-ring of seats facing a stage."""
    p = Piece("SM_TheaterSquare", seed)
    _base(p)
    for i, r in enumerate((4.2, 3.4, 2.6)):
        z = 0.2 + i * 0.45
        p.loft([(z, r, r, 0, -0.6), (z + 0.45, r, r, 0, -0.6), (z + 0.45, r - 0.8, r - 0.8, 0, -0.6)], STONE[i % 3],
               segments=12, arc=(0.15, math.pi - 0.15), two_sided=True)
    p.box((0, -1.8, 0.45), (4.0, 1.4, 0.5), MARBLE[1])
    _colonnade(p, -1.8, 1.8, -2.6, 1.8, 4)
    p.box((0, -2.6, 2.1), (4.2, 0.5, 0.3), MARBLE[0])
    _pennant(p, 3.9, -3.0)
    return p


def encampment(seed=46):
    """A palisade round tents and a drill yard."""
    p = Piece("SM_Encampment", seed)
    _base(p, color="#8c7a5a")
    s = 3.8
    for x, y, w, d in ((0, -s, 2 * s, 0.35), (0, s, 2 * s, 0.35), (-s, 0, 0.35, 2 * s), (s, 0, 0.35, 2 * s)):
        p.box((x, y, 0.9), (w, d, 1.4), WOOD[0])
    p.box((0, -s - 0.02, 0.8), (1.4, 0.4, 1.2), DARK)
    for k, (x, y) in enumerate(((-2.0, 1.6), (0.0, 2.1), (2.0, 1.6))):
        p.gable((x, y, 0.2), 1.6, 1.4, 1.2, CANVAS[k % 2], overhang=0.0, rot_z=math.pi / 2)
    for x in (-s, s):
        p.box((x, -s, 1.6), (1.0, 1.0, 2.8), WOOD[1])
    _pennant(p, 0.0, -0.8, 5.0)
    return p


def industrial_zone(seed=47):
    """A brick workshop with saw-tooth roofs and two chimneys."""
    p = Piece("SM_IndustrialZone", seed)
    _base(p, color="#8d877c")
    p.box((0, 0.5, 1.4), (7.0, 4.4, 2.4), BRICK[0])
    for i in range(4):
        x = -2.6 + i * 1.75
        p.box((x, 0.5, 2.9), (1.6, 4.4, 0.8), BRICK[1], taper=0.3)
    for x, h in ((-2.4, 6.0), (1.6, 5.2)):
        p.cylinder((x, 3.4, 0.2), 0.42, h, BRICK[1], segments=8, radius_top=0.3)
    p.cylinder((2.8, -2.8, 0.2), 0.9, 1.4, IRON[0], segments=10)
    _pennant(p, -3.9, -3.0)
    return p


def entertainment_complex(seed=48):
    """An oval arena."""
    p = Piece("SM_EntertainmentComplex", seed)
    _base(p)
    p.loft([(0.2, 4.0, 3.2), (2.8, 4.0, 3.2), (2.8, 2.9, 2.1), (1.0, 2.6, 1.8)], STONE[2], segments=20, cap_top=False)
    p.box((0, 0, 0.3), (5.0, 3.4, 0.2), "#c9b98f")
    for i in range(10):
        a = 2 * math.pi * i / 10
        p.box((4.02 * math.cos(a), 3.22 * math.sin(a), 1.2), (0.5, 0.5, 1.0), DARK, rot_z=a)
    _pennant(p, 0.0, 3.2, 5.2)
    return p


def aqueduct(seed=49):
    """Stone arches carrying a channel."""
    p = Piece("SM_Aqueduct", seed)
    n = 5
    for i in range(n):
        x = -4.0 + 8.0 * i / (n - 1)
        p.box((x, 0, 1.5), (0.6, 1.0, 3.0), STONE[i % 3])
    for i in range(n - 1):
        x = -3.0 + 2.0 * i
        p.box((x, 0, 2.75), (1.5, 1.0, 0.5), STONE[1])
    p.box((0, 0, 3.25), (8.8, 1.2, 0.5), STONE[0])
    p.box((0, 0, 3.52), (8.6, 0.6, 0.06), WATER)
    _pennant(p, 4.3, 1.2, 4.6)
    return p


def neighborhood(seed=50):
    """Streets of houses round a green."""
    p = Piece("SM_Neighborhood", seed)
    _base(p)
    p.box((0, 0, 0.22), (2.0, 2.0, 0.06), LAWN)
    for k, (x, y, r) in enumerate(((-3.0, -2.6, 0.0), (0.0, -3.0, 0.0), (3.0, -2.5, 0.1), (-3.2, 0.4, 1.57), (3.3, 0.6, 1.57),
                                   (-2.6, 3.0, 0.0), (0.4, 3.1, 0.0), (3.2, 3.0, -0.1))):
        _house(p, x, y, r, 1.0 + 0.15 * (k % 3), k)
    _tree(p, 0.0, 0.0, 0.8)
    _pennant(p, 1.4, -0.9, 3.4)
    return p


def spaceport(seed=51):
    """A launch pad, its tower and a rocket."""
    p = Piece("SM_Spaceport", seed)
    _base(p, 9.0, 9.0, "#9a968e")
    p.cylinder((0, 0, 0.2), 2.6, 0.4, "#7d7a74", segments=16)
    p.cylinder((0, 0, 0.6), 0.6, 6.0, WALL[2], segments=12)
    p.cone((0, 0, 6.6), 0.6, 1.4, ROOF[1], segments=12)
    for a in (0.0, 2.1, 4.2):
        p.box((0.75 * math.cos(a), 0.75 * math.sin(a), 1.2), (0.3, 0.3, 1.4), WALL[2], rot_z=a, taper=0.4)
    p.box((1.8, 0, 3.6), (0.5, 0.5, 7.0), IRON[1])
    _hall(p, -2.8, 3.0, 2.6, 1.8, 1.2, WALL[1], IRON[0])
    _pennant(p, 3.8, -3.6)
    return p


def government_plaza(seed=52):
    """A domed council hall on a paved square."""
    p = Piece("SM_GovernmentPlaza", seed)
    _base(p)
    p.box((0, 0.8, 1.6), (6.0, 4.0, 2.8), MARBLE[1])
    _colonnade(p, -2.6, 2.6, -1.5, 2.6, 6)
    p.box((0, 0.3, 3.15), (6.4, 5.0, 0.3), MARBLE[0])
    p.cylinder((0, 0.8, 3.3), 1.5, 0.8, MARBLE[0], segments=14)
    v = p.blob((0, 0.8, 4.1), 1.5, GOLD, squash=0.9, subdiv=2, wobble=0.0)
    for q in v:
        q.co.z = max(q.co.z, 4.1)
    for x in (-3.8, 3.8):
        _pennant(p, x, -3.2, 4.0)
    return p


def dam(seed=53):
    """A curved wall with sluices."""
    p = Piece("SM_Dam", seed)
    p.loft([(0.0, 4.6, 3.0, 0, -1.0), (3.2, 4.6, 3.0, 0, -1.0), (3.2, 4.0, 2.4, 0, -1.0), (0.0, 3.4, 1.8, 0, -1.0)], STONE[0],
           segments=12, arc=(0.35, math.pi - 0.35), two_sided=True)
    p.box((0, 1.2, 0.1), (7.0, 2.6, 0.2), WATER)
    _pennant(p, 3.6, 1.6, 4.6)
    return p


def canal(seed=54):
    """A stone-lined cut with a lock gate."""
    p = Piece("SM_Canal", seed)
    for y in (-1.3, 1.3):
        p.box((0, y, 0.35), (9.0, 0.8, 0.7), STONE[0])
    p.box((0, 0, 0.12), (9.0, 1.8, 0.1), WATER)
    for x in (-1.5, 1.5):
        p.box((x, 0, 0.55), (0.25, 1.8, 0.7), WOOD[0])
    _pennant(p, 3.8, 2.0)
    return p


def aerodrome(seed=55):
    """A runway and a hangar."""
    p = Piece("SM_Aerodrome", seed)
    p.box((0, 0, 0.1), (9.5, 2.2, 0.2), "#55554f")
    for i in range(6):
        p.box((-3.8 + i * 1.5, 0, 0.22), (0.7, 0.12, 0.04), FLAG)
    p.loft([(0.2, 1.6, 1.4, 0, 2.8), (1.4, 1.6, 1.4, 0, 2.8), (2.0, 1.0, 1.2, 0, 2.8)], IRON[0], segments=10)
    _pennant(p, -3.8, 2.6)
    return p


def water_park(seed=56):
    """Pools and a slide."""
    p = Piece("SM_WaterPark", seed)
    _base(p)
    p.cylinder((-1.4, 0.4, 0.2), 2.2, 0.1, WATER, segments=14)
    p.box((2.4, -1.4, 0.25), (2.6, 2.2, 0.1), WATER)
    p.box((2.8, 2.2, 1.4), (0.8, 0.8, 2.6), WALL[2])
    p.segment((2.8, 2.2, 2.6), (0.0, 1.2, 0.4), 0.25, 0.25, ROOF[1], segments=6, caps=False)
    _pennant(p, -3.9, -3.0)
    return p


def diplomatic_quarter(seed=57):
    """A hall flying many flags."""
    p = Piece("SM_DiplomaticQuarter", seed)
    _base(p)
    _hall(p, 0, 1.2, 6.0, 3.0, 2.6, WALL[2], ROOF[2])
    _colonnade(p, -2.6, 2.6, -0.5, 2.4, 6)
    for i in range(5):
        x = -3.2 + i * 1.6
        p.cylinder((x, -2.8, 0.2), 0.06, 3.0, WOOD[0], segments=5)
        p.box((x + 0.4, -2.8, 2.8), (0.8, 0.05, 0.6), CANVAS[i % 2] if i != 2 else FLAG)
    _pennant(p, 0.0, -3.4, 4.0)
    return p


def preserve(seed=58):
    """An untouched grove round a stone ring."""
    p = Piece("SM_Preserve", seed)
    _base(p, color=LAWN)
    for i in range(7):
        a = 2 * math.pi * i / 7
        p.box((1.5 * math.cos(a), 1.5 * math.sin(a), 0.75), (0.4, 0.4, 1.1), STONE[i % 3], rot_z=a)
    for x, y, s in ((-3.4, -2.6, 1.2), (-3.0, 2.4, 1.0), (3.2, 2.8, 1.3), (3.5, -2.2, 1.0), (0.2, 3.6, 0.9), (0.0, -3.5, 1.1)):
        _tree(p, x, y, s)
    _pennant(p, 2.4, 0.2, 3.6)
    return p


PIECES = [campus, holy_site, commercial_hub, harbor, theater_square, encampment, industrial_zone, entertainment_complex,
          aqueduct, neighborhood, spaceport, government_plaza, dam, canal, aerodrome, water_park, diplomatic_quarter, preserve]
