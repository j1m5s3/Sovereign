"""Resources kit: small map tokens for the resources, by kind. Each kind has an accent (the team slot) that the
map tints per resource: the beast's coat (cattle, sheep, horses), the shrub's fruit (grapes, citrus, cotton),
the ore's crystals (iron, jade, diamonds), so ten models cover all fifty-odd resources. About 4 m across.
"""
import math

import kitlib
from kitlib import Piece

LIGHT = "#e8e4dc"  # accents are painted light so the tint shows true
STRAW = ["#d2b45a", "#c9a64b"]
WOOD = ["#6b4c33", "#5a3f2b"]
STONE = ["#8d877c", "#9c968b"]
LEAF = ["#4f7a35", "#5c8a3c", "#46702f"]
DARK = "#2e2722"
WATER = "#3f7fa6"

kitlib.patterns({"thatch": STRAW, "wood": WOOD, "stone": STONE, "foliage": LEAF, "water": WATER})


def _accent(p, on=True):
    p.team_color(on)


def sheaf(seed=81):
    """Three sheaves of grain (wheat, rice, maize: the accent is the ears)."""
    p = Piece("SM_Sheaf", seed)
    for k, (x, y) in enumerate(((-0.9, 0.2), (0.8, 0.5), (0.0, -0.8))):
        p.cylinder((x, y, 0), 0.35, 1.4, STRAW[k % 2], segments=7, radius_top=0.5)
        p.box((x, y, 0.55), (0.75, 0.75, 0.15), WOOD[0])
        _accent(p)
        p.blob((x, y, 1.6), 0.55, LIGHT, squash=1.1)
        _accent(p, False)
    return p


def _beast(p, x=0.0, y=0.0, s=1.0, rot=0.0):
    c, sn = math.cos(rot), math.sin(rot)

    def at(u, v, z):
        return (x + (u * c - v * sn) * s, y + (u * sn + v * c) * s, z * s)

    _accent(p)
    p.segment(at(-0.7, 0, 1.1), at(0.7, 0, 1.15), 0.48 * s, 0.45 * s, LIGHT, segments=8)
    p.segment(at(0.8, 0, 1.2), at(1.3, 0, 0.75), 0.24 * s, 0.2 * s, LIGHT, segments=6)  # head down, grazing
    for u in (-0.6, 0.6):
        for v in (-0.25, 0.25):
            p.segment(at(u, v, 0.9), at(u, v, 0.0), 0.12 * s, 0.1 * s, LIGHT, segments=5, caps=False)
    _accent(p, False)


def beasts(seed=82):
    p = Piece("SM_Beast", seed)
    p.cylinder((0.1, 0.2, 0), 1.9, 0.06, LEAF[1], segments=12)  # a patch of grazing (and the base slot)
    _beast(p, -0.6, -0.4, 1.0, 0.3)
    _beast(p, 0.9, 0.8, 0.8, -2.4)
    return p


def fish(seed=83):
    """Fish leaping from a ring of water; the accent is the fish."""
    p = Piece("SM_Fish", seed)
    p.cylinder((0, 0, 0), 1.6, 0.06, WATER, segments=14)
    for k, (x, y, rot) in enumerate(((-0.5, 0.0, 0.4), (0.7, 0.4, 2.6))):
        _accent(p)
        a, b = (x - 0.6 * math.cos(rot), y - 0.6 * math.sin(rot), 0.5), (x + 0.6 * math.cos(rot), y + 0.6 * math.sin(rot), 1.0)
        p.segment(a, b, 0.18, 0.3, LIGHT, segments=7)
        _accent(p, False)
        p.cone((a[0], a[1], a[2] - 0.25), 0.3, 0.35, LIGHT, segments=4)
    return p


def shell(seed=84):
    """A ridged shell on the sand (crabs, pearls, turtles); the accent is the shell."""
    p = Piece("SM_Shell", seed)
    p.cylinder((0, 0, 0), 1.5, 0.06, "#d8c48f", segments=12)
    _accent(p)
    v = p.blob((0, 0, 0.1), 1.0, LIGHT, squash=0.55, subdiv=2, wobble=0.0)
    for q in v:
        q.co.z = max(q.co.z, 0.1)
        q.co.z += 0.06 * math.sin(8 * math.atan2(q.co.y, q.co.x))
    _accent(p, False)
    p.blob((1.0, 0.7, 0.15), 0.3, LIGHT, squash=0.6)
    return p


def ore(seed=85):
    """Rock with crystals or ore breaking out (iron, coal, jade, diamonds, salt...); the accent is the ore."""
    p = Piece("SM_Ore", seed)
    p.blob((0, 0, 0.4), 1.2, STONE[0], squash=0.6, wobble=0.25)
    p.blob((1.0, 0.5, 0.3), 0.7, STONE[1], squash=0.6, wobble=0.25)
    _accent(p)
    for k, (x, y, h, tilt) in enumerate(((-0.3, -0.5, 1.6, 0.3), (0.3, -0.2, 1.2, -0.4), (0.8, -0.6, 0.9, 0.6), (-0.9, 0.0, 1.0, -0.6))):
        top = (x + math.sin(tilt) * h * 0.5, y - 0.2, h)
        p.segment((x, y, 0.3), top, 0.25, 0.04, LIGHT, segments=5, caps=False)
    _accent(p, False)
    return p


def blocks(seed=86):
    """Cut stone blocks (stone, marble, gypsum); the accent is the stone."""
    p = Piece("SM_Blocks", seed)
    p.cylinder((0, 0.1, 0), 1.6, 0.05, STONE[0], segments=10)  # chippings (and the base slot)
    _accent(p)
    for k, (x, y, z, r) in enumerate(((-0.6, -0.3, 0.4, 0.1), (0.7, -0.2, 0.4, -0.2), (0.0, 0.7, 0.4, 0.4), (0.0, -0.1, 1.15, 0.0))):
        p.box((x, y, z), (1.2, 0.9, 0.8), LIGHT, rot_z=r)
    _accent(p, False)
    return p


def shrub(seed=87):
    """A bush heavy with fruit or flowers (grapes, citrus, coffee, cotton, spices...); the accent is the crop."""
    p = Piece("SM_Shrub", seed)
    p.cylinder((0, 0, 0), 0.15, 0.5, WOOD[0], segments=5)
    p.blob((0, 0, 1.0), 1.0, LEAF[0], squash=0.85)
    p.blob((0.9, 0.5, 0.7), 0.65, LEAF[1], squash=0.85)
    p.blob((-0.8, 0.4, 0.65), 0.6, LEAF[2], squash=0.85)
    _accent(p)
    for k in range(9):
        a = 2.4 * k
        r = 0.75 + 0.2 * (k % 3)
        p.blob((r * math.cos(a), r * math.sin(a) - 0.2, 0.75 + 0.35 * (k % 2)), 0.2, LIGHT, squash=1.0, subdiv=1, wobble=0.0)
    _accent(p, False)
    return p


def oil_pool(seed=88):
    """A dark seep with barrels; the accent is the barrels' bands."""
    p = Piece("SM_OilPool", seed)
    p.cylinder((0, 0, 0), 1.3, 0.08, DARK, segments=12)
    for k, (x, y) in enumerate(((1.2, 0.8), (1.6, -0.2))):
        p.cylinder((x, y, 0), 0.4, 1.0, WOOD[k % 2], segments=8)
        _accent(p)
        p.cylinder((x, y, 0.65), 0.42, 0.12, LIGHT, segments=8)
        _accent(p, False)
    return p


def hive(seed=89):
    """Straw skeps on a bench (honey); the accent is the bands."""
    p = Piece("SM_Hive", seed)
    p.box((0, 0, 0.35), (2.6, 0.9, 0.12), WOOD[0])
    for x in (-0.9, 0.9):
        p.box((x, 0, 0.15), (0.2, 0.7, 0.3), WOOD[1])
    for x in (-0.7, 0.7):
        v = p.blob((x, 0, 0.41), 0.55, STRAW[0], squash=1.3, subdiv=2, wobble=0.0)
        for q in v:
            q.co.z = max(q.co.z, 0.41)
        _accent(p)
        p.cylinder((x, 0, 0.6), 0.56, 0.1, LIGHT, segments=10)
        _accent(p, False)
    return p


def goods(seed=90):
    """Crates and bales of made goods (cosmetics, jeans, perfume, toys); the accent is the wrapping."""
    p = Piece("SM_Goods", seed)
    p.box((-0.5, 0, 0.45), (0.9, 0.9, 0.9), WOOD[0])
    p.box((0.6, 0.3, 0.35), (0.7, 0.7, 0.7), WOOD[1], rot_z=0.4)
    _accent(p)
    p.box((0.1, -0.9, 0.3), (1.2, 0.6, 0.6), LIGHT, rot_z=-0.2)
    p.box((-0.5, 0, 1.1), (0.6, 0.6, 0.4), LIGHT)
    _accent(p, False)
    return p


PIECES = [sheaf, beasts, fish, shell, ore, blocks, shrub, oil_pool, hive, goods]
