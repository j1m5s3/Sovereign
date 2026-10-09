"""Fields kit: the tile improvements on the map (farms, mines, pastures and the rest), shared by every civ.
Map tokens rather than true scale: each fits a footprint about 9 m across with buildings a few metres
tall, and the map draws it at a tenth of that, beside the plot's resource.
"""
import math

import kitlib
from kitlib import Piece

SOIL = ["#6e5236", "#5f4630"]
WHEAT = ["#d2b45a", "#c9a64b"]
SPROUT = ["#7fa046", "#6d9140"]
TIMBER = ["#6b4c33", "#5a3f2b"]
STONE = ["#9c968b", "#8d877c", "#aaa398"]
THATCH = "#b89b5e"
CANVAS = ["#e2d6bc", "#cdbf9f"]
LEAF = ["#4f7a35", "#5c8a3c"]
HULL = "#7a5537"
DARK = "#2e2722"
IRON = ["#5d5f63", "#4a4c50"]
WHITE = "#e6e3dc"

kitlib.patterns({"wood": TIMBER + [HULL], "stone": STONE, "thatch": [THATCH] + WHEAT, "cloth": CANVAS,
                 "foliage": LEAF + SPROUT, "metal": IRON, "plaster": WHITE})


def _hut(p, x, y, rot=0.0, w=2.0, d=1.6, h=1.3):
    p.box((x, y, h / 2), (w, d, h), TIMBER[0], rot_z=rot)
    p.gable((x, y, h), w, d, 0.9, THATCH, overhang=0.2, rot_z=rot)


def _fence(p, points, closed=True, post=0.09, height=0.7):
    """Posts at the points with two rails between them."""
    n = len(points)
    for i in range(n if closed else n - 1):
        a, b = points[i], points[(i + 1) % n]
        for z in (height * 0.45, height * 0.85):
            p.segment((a[0], a[1], z), (b[0], b[1], z), 0.04, 0.04, TIMBER[1], segments=4, caps=False)
    for x, y in points:
        p.cylinder((x, y, 0), post, height, TIMBER[0], segments=5)


def farm(seed=21):
    """Four strips of field, ripe and green by turns, with furrows across them."""
    p = Piece("SM_Farm", seed)
    for i in range(4):
        y = -3.0 + i * 2.0
        p.box((0, y, 0.05), (8.5, 1.8, 0.1), SOIL[i % 2])
        crop = WHEAT[i % 2] if i % 2 == 0 else SPROUT[i % 2]
        for j in range(7):
            x = -3.6 + j * 1.2
            p.box((x, y, 0.2), (0.7, 1.6, 0.22 if i % 2 == 0 else 0.14), crop)
    _hut(p, 3.6, 4.3, 0.0, 1.6, 1.3, 1.1)
    return p


def mine(seed=22):
    """A timbered adit in a spoil heap, a cart on its rails."""
    p = Piece("SM_Mine", seed)
    p.blob((0, 0.6, 0.6), 2.6, STONE[1], squash=0.55, wobble=0.2)
    p.blob((2.3, 1.4, 0.4), 1.5, STONE[0], squash=0.5, wobble=0.25)
    p.box((0, -1.7, 0.9), (1.4, 0.6, 1.8), DARK)
    for x in (-0.85, 0.85):
        p.box((x, -1.9, 1.0), (0.25, 0.25, 2.0), TIMBER[0])
    p.box((0, -1.9, 2.05), (2.1, 0.3, 0.3), TIMBER[1])
    for x in (-0.45, 0.45):
        p.box((x, -3.2, 0.05), (0.08, 2.6, 0.08), IRON[0])
    p.box((0, -3.6, 0.45), (1.0, 1.2, 0.6), IRON[1], taper=1.2)
    p.blob((0, -3.6, 0.8), 0.45, STONE[2], squash=0.6)
    return p


def quarry(seed=23):
    """Stepped faces cut into the rock, with blocks waiting to go."""
    p = Piece("SM_Quarry", seed)
    for i, (w, h) in enumerate(((7.0, 0.6), (5.4, 1.2), (3.8, 1.8))):
        p.box((0, 1.0 + i * 0.6, h / 2), (w, 3.6 - i * 0.6, h), STONE[i % 3])
    for k, (x, y) in enumerate(((-2.6, -2.4), (-1.2, -2.8), (0.4, -2.3), (2.4, -2.7))):
        p.box((x, y, 0.4), (1.1, 0.8, 0.8), STONE[k % 3], rot_z=0.2 * k)
    p.segment((3.2, -1.0, 0), (3.2, -1.0, 3.2), 0.1, 0.08, TIMBER[0], segments=5, caps=False)
    p.segment((3.2, -1.0, 3.1), (1.4, -2.3, 2.2), 0.07, 0.06, TIMBER[1], segments=5, caps=False)
    return p


def pasture(seed=24):
    """A fenced paddock with a shelter."""
    p = Piece("SM_Pasture", seed)
    p.box((0, 0, 0.04), (8.0, 7.0, 0.08), SPROUT[0])
    pts = [(-3.8, -3.3), (0, -3.5), (3.8, -3.3), (3.9, 0), (3.8, 3.3), (0, 3.5), (-3.8, 3.3), (-3.9, 0)]
    _fence(p, pts)
    _hut(p, -2.2, 2.0, 0.0, 2.2, 1.6, 1.2)
    p.box((1.2, -0.8, 0.25), (1.6, 0.5, 0.5), TIMBER[1])
    return p


def plantation(seed=25):
    """Rows of bushes on worked ground."""
    p = Piece("SM_Plantation", seed)
    p.box((0, 0, 0.05), (8.5, 7.5, 0.1), SOIL[0])
    for r in range(4):
        y = -2.9 + r * 1.9
        for c in range(5):
            x = -3.4 + c * 1.7 + (0.5 if r % 2 else 0.0)
            p.blob((x, y, 0.75), 0.75, LEAF[(r + c) % 2], squash=0.9)
    return p


def camp(seed=26):
    """Hunters' tents round a fire, hides on a rack."""
    p = Piece("SM_Camp", seed)
    for k, (x, y) in enumerate(((-1.8, 0.8), (1.4, 1.6), (0.2, -1.8))):
        p.cone((x, y, 0), 1.3, 2.4, CANVAS[k % 2], segments=7)
        p.segment((x, y, 2.0), (x, y, 2.8), 0.05, 0.04, TIMBER[0], segments=4, caps=False)
    p.disc((0, 0.2, 0.08), 0.5, 0.1, DARK)
    p.box((2.6, -1.2, 0.8), (1.6, 0.12, 1.2), CANVAS[1])
    p.box((2.6, -1.2, 1.5), (1.9, 0.12, 0.12), TIMBER[1])
    return p


def fishing_boats(seed=27):
    """Two small sailing boats."""
    p = Piece("SM_FishingBoats", seed)
    for k, (x, y, rot) in enumerate(((-1.6, -0.6, 0.3), (1.9, 1.2, -0.5))):
        p.box((x, y, 0.3), (2.8, 1.0, 0.6), HULL, rot_z=rot, taper=1.25)
        p.cylinder((x, y, 0.5), 0.06, 2.8, TIMBER[0], segments=5)
        dx, dy = math.cos(rot), math.sin(rot)
        p.box((x + dx * 0.55, y + dy * 0.55, 1.9), (1.0, 0.05, 2.0), CANVAS[k % 2], rot_z=rot, taper=0.2)
    return p


def lumber_mill(seed=28):
    """A shed with a log pile and a saw pit."""
    p = Piece("SM_LumberMill", seed)
    _hut(p, -1.6, 1.0, 0.0, 3.0, 2.2, 1.6)
    for row, z in enumerate((0.3, 0.8, 1.25)):
        for i in range(4 - row):
            y = -2.0 + (i + row * 0.5) * 0.55
            p.segment((1.0, y, z), (3.8, y, z), 0.27, 0.27, TIMBER[i % 2], segments=6, caps=False)
    p.box((-1.6, -2.4, 0.4), (2.4, 0.5, 0.8), TIMBER[1])
    return p


def oil_well(seed=29):
    """A derrick over the bore and a storage tank."""
    p = Piece("SM_OilWell", seed)
    p.box((0, 0, 0.1), (2.6, 2.6, 0.2), IRON[1])
    for x in (-1.0, 1.0):
        for y in (-1.0, 1.0):
            p.segment((x, y, 0.2), (x * 0.15, y * 0.15, 5.2), 0.08, 0.06, IRON[0], segments=4, caps=False)
    for z, s in ((1.6, 0.72), (3.0, 0.46)):
        p.box((0, 0, z), (2 * s * 1.0 + 0.1, 2 * s * 1.0 + 0.1, 0.1), IRON[0])
    p.cylinder((2.8, 1.6, 0), 1.1, 1.6, WHITE, segments=12)
    p.box((2.4, -1.6, 0.5), (1.2, 0.9, 1.0), IRON[1])
    return p


def fort(seed=30):
    """A square palisade with corner towers."""
    p = Piece("SM_Fort", seed)
    s = 3.2
    for x, y, w, d in ((0, -s, 2 * s, 0.4), (0, s, 2 * s, 0.4), (-s, 0, 0.4, 2 * s), (s, 0, 0.4, 2 * s)):
        p.box((x, y, 0.9), (w, d, 1.8), TIMBER[0])
    for x in (-s, s):
        for y in (-s, s):
            p.box((x, y, 1.4), (1.1, 1.1, 2.8), TIMBER[1])
            p.cone((x, y, 2.8), 0.9, 1.0, THATCH, segments=4)
    p.box((0, -s - 0.05, 0.7), (1.2, 0.3, 1.4), DARK)
    return p


def wind_farm(seed=31):
    """Two turbines."""
    p = Piece("SM_WindFarm", seed)
    for x, y, h in ((-1.8, -0.8, 6.5), (2.0, 1.2, 5.5)):
        p.cylinder((x, y, 0), 0.22, h, WHITE, segments=8, radius_top=0.12)
        p.box((x, y - 0.25, h), (0.35, 0.8, 0.35), WHITE)
        for k in range(3):
            a = math.pi / 2 + k * 2 * math.pi / 3
            p.segment((x, y - 0.7, h), (x + math.cos(a) * 2.6, y - 0.7, h + math.sin(a) * 2.6), 0.12, 0.05, WHITE,
                      segments=4, caps=False)
    return p


def solar_farm(seed=32):
    """Rows of tilted panels."""
    p = Piece("SM_SolarFarm", seed)
    for r in range(3):
        for c in range(3):
            x, y = -2.8 + c * 2.8, -2.4 + r * 2.4
            p.box((x, y, 0.4), (0.15, 0.15, 0.8), IRON[0])
            v = p.box((x, y, 0.85), (2.4, 1.4, 0.08), "#24324a")
            for q in v:
                q.co.z += (q.co.y - y) * 0.5
    return p


def works(seed=33):
    """Any other improvement: a walled yard with a workshop."""
    p = Piece("SM_Works", seed)
    p.box((0, 0, 0.04), (6.5, 5.5, 0.08), SOIL[1])
    _fence(p, [(-3.2, -2.7), (3.2, -2.7), (3.2, 2.7), (-3.2, 2.7)])
    _hut(p, -0.8, 0.6, 0.0, 2.8, 2.0, 1.5)
    p.box((1.9, -1.2, 0.35), (1.0, 1.0, 0.7), TIMBER[1])
    return p


PIECES = [farm, mine, quarry, pasture, plantation, camp, fishing_boats, lumber_mill, oil_well, fort, wind_farm,
          solar_farm, works]
