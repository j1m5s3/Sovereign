"""Props kit: the clutter of a lived-in street (the street scene's stage 5 dressing): a well, barrels, crates,
sacks, a hand cart, a bench, a lamp post, a water trough, a haystack, a woodpile and flower pots. Plain wood,
stone, iron and cloth from the other kits' palettes, so they wear the same patterns.
"""
import math

import kitlib
from kitlib import Piece

WOOD = ["#6b4c33", "#5a3f2b"]
STONE = ["#a39c8f", "#938c80"]
ROOF = "#a9533a"
THATCH = "#b89b5e"
IRON = "#5d5f63"
DARK = "#3a2f28"
CLOTH = ["#c9b27a", "#e2d6bc"]
LEAF = ["#4f7a35", "#5c8a3c"]
WATER = "#3f7fa6"
CLAY = "#9c7b5c"
FLOWERS = ["#c03a2a", "#d8b040", "#e8e2d6", "#8a4fa0"]
BARK = "#5b4532"

kitlib.patterns({"wood": WOOD + [BARK], "masonry": STONE, "rooftile": [ROOF, CLAY], "thatch": THATCH, "metal": IRON,
                 "cloth": CLOTH, "foliage": LEAF, "water": WATER})


def _barrel(p, x, y, z=0.0, h=0.85):
    """Bellied staves with three iron hoops."""
    r = 0.27
    p.loft([(z, r * 0.85, r * 0.85, x, y), (z + h * 0.5, r, r, x, y), (z + h, r * 0.85, r * 0.85, x, y)], WOOD[0], segments=12)
    for t in (0.12, 0.5, 0.88):
        rr = r * (1.0 - 0.15 * abs(t - 0.5) * 2) + 0.012
        p.cylinder((x, y, z + h * t - 0.025), rr, 0.05, IRON, segments=12)
    p.cylinder((x, y, z + h - 0.01), r * 0.8, 0.02, WOOD[1], segments=12)  # the lid


def barrels(seed=81):
    p = Piece("SM_Barrels", seed)
    _barrel(p, 0, 0)
    _barrel(p, 0.58, 0.1)
    _barrel(p, 0.25, 0.5, h=0.75)
    return p


def _crate(p, c, s, rot=0.0):
    p.box(c, (s, s, s), WOOD[0], rot_z=rot)
    # Battens round each face, a shade darker.
    for dz in (-s / 2 + 0.05, s / 2 - 0.05):
        p.box((c[0], c[1], c[2] + dz), (s + 0.02, s + 0.02, 0.08), WOOD[1], rot_z=rot)


def crates(seed=82):
    p = Piece("SM_Crates", seed)
    _crate(p, (0, 0, 0.3), 0.6)
    _crate(p, (0.66, 0.05, 0.25), 0.5, rot=0.2)
    _crate(p, (0.1, 0.02, 0.84), 0.48, rot=-0.15)
    return p


def sacks(seed=83):
    p = Piece("SM_Sacks", seed)
    for i, (x, y) in enumerate([(0, 0), (0.42, 0.06), (0.2, 0.38), (0.62, 0.42)]):
        p.blob((x, y, 0.2), 0.26, CLOTH[i % 2], squash=0.75, subdiv=2, wobble=0.08)
        p.cylinder((x, y, 0.36), 0.07, 0.1, CLOTH[i % 2], segments=6)  # the tied neck
    p.blob((0.22, 0.18, 0.55), 0.24, CLOTH[0], squash=0.7, subdiv=2, wobble=0.08)
    return p


def cart(seed=84):
    """A two-wheeled hand cart with shafts, a sack and a barrel in it."""
    p = Piece("SM_Cart", seed)
    L, W, z = 1.8, 1.1, 0.62
    p.box((0, 0, z), (L, W, 0.08), WOOD[0])
    for y in (-W / 2, W / 2):
        p.box((0, y, z + 0.2), (L, 0.05, 0.35), WOOD[1])  # side boards
    p.box((-L / 2, 0, z + 0.2), (0.05, W, 0.35), WOOD[1])
    for y in (-W / 2 - 0.1, W / 2 + 0.1):
        p.disc((0, y, 0.5), 0.5, 0.07, WOOD[1], segments=14)  # the wheels, rolling along X
        p.disc((0, y + (0.04 if y > 0 else -0.04), 0.5), 0.1, 0.06, IRON, segments=8)
    p.box((0, 0, 0.5), (0.08, W + 0.3, 0.08), IRON)  # the axle
    for y in (-0.38, 0.38):
        p.segment((L / 2 - 0.1, y, z), (L / 2 + 1.3, y, 0.35), 0.035, 0.03, WOOD[1], segments=6)  # the shafts, resting down
    p.blob((-0.3, 0.1, z + 0.28), 0.28, CLOTH[0], squash=0.7, subdiv=2, wobble=0.08)
    _barrel(p, 0.35, -0.15, z=z + 0.04, h=0.6)
    return p


def well(seed=85):
    """A round stone well with a little tiled roof on two posts, a windlass and a bucket."""
    p = Piece("SM_Well", seed)
    p.cylinder((0, 0, 0), 0.95, 0.85, STONE[0], segments=16)
    p.cylinder((0, 0, 0.85), 1.0, 0.1, STONE[1], segments=16)  # the coping
    p.cylinder((0, 0, 0.9), 0.72, 0.06, DARK, segments=16)  # the dark shaft
    for x in (-0.85, 0.85):
        p.box((x, 0, 1.45), (0.14, 0.14, 1.2), WOOD[0])
    p.segment((-0.9, 0, 1.75), (0.9, 0, 1.75), 0.07, 0.07, WOOD[1], segments=8)  # the windlass
    p.segment((0.9, 0, 1.75), (1.15, 0, 1.55), 0.025, 0.025, IRON, segments=5)  # its crank
    p.segment((0.1, 0, 1.72), (0.1, 0, 1.3), 0.01, 0.01, CLOTH[0], segments=4)  # the rope
    p.cylinder((0.1, 0, 1.08), 0.13, 0.22, WOOD[1], radius_top=0.15, segments=10)  # the bucket
    p.gable((0, 0, 2.05), 2.0, 1.2, 0.6, ROOF, overhang=0.2)
    return p


def bench(seed=86):
    p = Piece("SM_Bench", seed)
    p.box((0, 0, 0.45), (1.8, 0.4, 0.07), WOOD[0])
    for x in (-0.7, 0.7):
        p.box((x, 0, 0.21), (0.1, 0.34, 0.42), WOOD[1])
    p.box((0, 0.18, 0.8), (1.8, 0.05, 0.25), WOOD[0])  # the back rail
    for x in (-0.7, 0.7):
        p.box((x, 0.19, 0.62), (0.06, 0.05, 0.36), WOOD[1])
    return p


def lamp_post(seed=87):
    """A wooden post with an iron bracket and a lantern."""
    p = Piece("SM_LampPost", seed)
    p.box((0, 0, 0.12), (0.4, 0.4, 0.24), STONE[0])  # the footing
    p.box((0, 0, 1.55), (0.14, 0.14, 2.9), WOOD[0], taper=0.8)
    p.segment((0, 0, 2.75), (0.55, 0, 2.75), 0.025, 0.025, IRON, segments=5)
    p.segment((0, 0, 2.45), (0.4, 0, 2.75), 0.02, 0.02, IRON, segments=5)  # the brace
    p.box((0.55, 0, 2.45), (0.22, 0.22, 0.3), "#f1d58a")  # the lit glass
    p.cone((0.55, 0, 2.6), 0.18, 0.18, IRON, segments=4)
    p.box((0.55, 0, 2.29), (0.24, 0.24, 0.04), IRON)
    return p


def trough(seed=88):
    p = Piece("SM_Trough", seed)
    p.box((0, 0, 0.2), (1.8, 0.7, 0.4), STONE[0])
    for y in (-0.3, 0.3):
        p.box((0, y, 0.5), (1.8, 0.1, 0.2), STONE[1])  # the rims
    for x in (-0.85, 0.85):
        p.box((x, 0, 0.5), (0.1, 0.5, 0.2), STONE[1])
    p.box((0, 0, 0.5), (1.6, 0.5, 0.04), WATER)
    return p


def haystack(seed=89):
    p = Piece("SM_Haystack", seed)
    p.blob((0, 0, 0.55), 1.0, THATCH, squash=0.75, subdiv=2, wobble=0.1)
    p.blob((0.2, 0.1, 1.15), 0.55, THATCH, squash=0.9, subdiv=2, wobble=0.1)
    p.segment((0.9, -0.4, 0.0), (0.4, -0.6, 1.6), 0.025, 0.02, WOOD[1], segments=5)  # a pitchfork leant on it
    return p


def woodpile(seed=90):
    """Split logs stacked against a wall, under a lean-to of boards."""
    p = Piece("SM_Woodpile", seed)
    for row in range(3):
        for i in range(6 - row):
            y = -0.55 + i * 0.22 + row * 0.11
            z = 0.1 + row * 0.19
            p.segment((-0.45, y, z), (0.45, y, z), 0.1, 0.1, BARK, segments=7, caps=False)
    for x in (-0.45, 0.45):
        p.box((x, 0.75, 0.6), (0.08, 0.08, 1.2), WOOD[1])
    p.box((0, 0.1, 1.25), (1.2, 1.5, 0.05), WOOD[0])
    return p


def flower_pots(seed=91):
    p = Piece("SM_FlowerPots", seed)
    for i, (x, y, r) in enumerate([(0, 0, 0.22), (0.45, 0.05, 0.17), (0.2, 0.38, 0.19)]):
        p.cylinder((x, y, 0), r * 0.75, r * 1.6, CLAY, radius_top=r, segments=10)
        p.blob((x, y, r * 1.7), r * 1.05, LEAF[i % 2], squash=0.8, subdiv=1)
        for k in range(4):
            a = k * math.pi / 2 + i
            p.blob((x + math.cos(a) * r * 0.6, y + math.sin(a) * r * 0.6, r * 2.3), r * 0.25, FLOWERS[(i + k) % len(FLOWERS)])
    return p


PIECES = [barrels, crates, sacks, cart, well, bench, lamp_post, trough, haystack, woodpile, flower_pots]
