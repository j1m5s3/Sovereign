"""Wonders kit: map tokens for the world wonders, by kind rather than one each (the map actor maps every
wonder to the nearest kind; the rest use the Classical kit's temple). Footprints about 9 m across like
the other map kits, but taller, so a wonder stands out over the districts round it.
"""
import math

import kitlib
from kitlib import Piece

SAND = ["#d8c48f", "#cbb47c", "#e2d1a0"]
STONE = ["#a39c8f", "#938c80", "#b1aa9d"]
MARBLE = ["#ebe7df", "#dedad1"]
WALL = ["#e3d9c3", "#d9cdb2"]
ROOF = ["#a9533a", "#9c4b34"]
BRONZE = ["#8c6a3a", "#7a5a30"]
GOLD = "#c9a43c"
LEAF = ["#4f7a35", "#5c8a3c", "#46702f"]
LAWN = "#7fa046"
WATER = "#3f7fa6"
IRON = ["#5d5f63", "#4a4c50"]
BRICK = ["#9a5a42", "#8a4f3a"]
DOME = ["#6d8f8a", "#c9a43c"]
DARK = "#3a2f28"
PAVING = "#c8bfae"

kitlib.patterns({"stone": SAND + STONE + MARBLE + [PAVING], "plaster": WALL, "rooftile": ROOF + BRICK, "metal": BRONZE + IRON + [GOLD] + DOME,
                 "foliage": LEAF + [LAWN], "water": WATER})


def _base(p, w=9.0, d=9.0, color=PAVING):
    p.box((0, 0, 0.1), (w, d, 0.2), color)


def _hemi(p, x, y, z, r, color, squash=1.0):
    v = p.blob((x, y, z), r, color, squash=squash, subdiv=2, wobble=0.0)
    for q in v:
        q.co.z = max(q.co.z, z)


def pyramid(seed=61):
    """A true pyramid with a smaller one beside it."""
    p = Piece("SM_Pyramid", seed)
    _base(p, color=SAND[2])
    p.cone((-0.6, 0.6, 0.2), 4.4, 5.6, SAND[0], segments=4)
    p.cone((3.0, -2.6, 0.2), 1.6, 2.0, SAND[1], segments=4)
    return p


def step_pyramid(seed=62):
    """Stepped terraces with a shrine on top and a stair up the front."""
    p = Piece("SM_StepPyramid", seed)
    _base(p, color=STONE[2])
    z = 0.2
    for i, s in enumerate((8.0, 6.4, 4.8, 3.4)):
        p.box((0, 0, z + 0.6), (s, s, 1.2), STONE[i % 2])
        z += 1.2
    p.box((0, 0, z + 0.7), (2.2, 2.2, 1.4), WALL[0])
    p.box((0, 0, z + 1.5), (2.6, 2.6, 0.25), ROOF[0])
    for q in p.box((0, -3.0, 2.6), (1.4, 3.0, 0.3), STONE[2]):  # the stair, sloped up to the shrine
        q.co.z += (q.co.y + 3.0) * 1.6
    return p


def stone_circle(seed=63):
    """Standing stones and lintels on a green."""
    p = Piece("SM_StoneCircle", seed)
    _base(p, color=LAWN)
    n = 10
    for i in range(n):
        a = 2 * math.pi * i / n
        x, y = 3.4 * math.cos(a), 3.4 * math.sin(a)
        p.box((x, y, 0.2 + 1.3), (0.7, 0.5, 2.6), STONE[i % 3], rot_z=a + math.pi / 2)
        if i % 2 == 0:
            b = a + math.pi / n
            p.box((3.4 * math.cos(b), 3.4 * math.sin(b), 3.0), (2.4, 0.5, 0.4), STONE[1], rot_z=b + math.pi / 2)
    for i in range(3):
        a = 2 * math.pi * i / 3 + 0.3
        p.box((1.3 * math.cos(a), 1.3 * math.sin(a), 0.2 + 1.6), (0.8, 0.5, 3.2), STONE[0], rot_z=a + math.pi / 2)
    return p


def gardens(seed=64):
    """Terraced gardens spilling green, with a pool."""
    p = Piece("SM_Gardens", seed)
    _base(p, color=PAVING)
    z = 0.2
    for i, s in enumerate((7.6, 5.6, 3.6)):
        p.box((0, 0.6, z + 0.8), (s, s - 0.6, 1.6), WALL[i % 2])
        for k in range(6):
            a = 2 * math.pi * k / 6 + i
            p.blob((s / 2 * 0.9 * math.cos(a), 0.6 + (s - 0.6) / 2 * 0.9 * math.sin(a), z + 1.7), 0.6, LEAF[(k + i) % 3], squash=0.8)
        z += 1.6
    p.blob((0, 0.6, z + 0.6), 1.0, LEAF[1], squash=0.9)
    p.box((0, -3.8, 0.25), (5.0, 1.0, 0.1), WATER)
    return p


def statue(seed=65):
    """A colossal figure with a raised arm on a plinth."""
    p = Piece("SM_Statue", seed)
    _base(p, 6.0, 6.0, STONE[2])
    p.box((0, 0, 1.1), (2.6, 2.6, 1.8), STONE[0])
    p.box((0, 0, 2.15), (3.0, 3.0, 0.3), STONE[1])
    p.segment((-0.35, 0, 2.3), (-0.25, 0, 4.4), 0.38, 0.4, BRONZE[0], segments=8)
    p.segment((0.35, 0, 2.3), (0.25, 0, 4.4), 0.38, 0.4, BRONZE[0], segments=8)
    p.loft([(4.2, 0.75, 0.5), (5.6, 0.85, 0.55), (6.6, 0.65, 0.45)], BRONZE[1], segments=10)
    p.blob((0, 0, 7.1), 0.5, BRONZE[0], subdiv=2, wobble=0.0)
    p.segment((0.75, 0, 6.4), (1.3, 0, 8.2), 0.2, 0.17, BRONZE[0], segments=6)
    p.segment((-0.75, 0, 6.4), (-0.95, 0, 5.0), 0.2, 0.17, BRONZE[0], segments=6)
    p.cone((1.35, 0, 8.3), 0.3, 0.7, GOLD, segments=6)
    return p


def tower(seed=66):
    """A tall tower with a lit lantern (lighthouses, clock towers)."""
    p = Piece("SM_Tower", seed)
    _base(p, 6.0, 6.0, STONE[2])
    p.box((0, 0, 0.8), (3.6, 3.6, 1.2), STONE[1])
    p.cylinder((0, 0, 1.4), 1.2, 6.4, WALL[0], segments=10, radius_top=0.85)
    p.cylinder((0, 0, 7.8), 1.05, 0.3, STONE[0], segments=10)
    p.cylinder((0, 0, 8.1), 0.7, 1.0, GOLD, segments=8)
    p.cone((0, 0, 9.1), 0.9, 1.0, ROOF[0], segments=8)
    return p


def lattice_tower(seed=67):
    """An iron lattice tower on four legs."""
    p = Piece("SM_LatticeTower", seed)
    _base(p, 7.0, 7.0, LAWN)
    for x in (-1, 1):
        for y in (-1, 1):
            p.segment((x * 2.6, y * 2.6, 0.2), (x * 0.9, y * 0.9, 4.0), 0.28, 0.2, IRON[0], segments=5, caps=False)
            p.segment((x * 0.9, y * 0.9, 4.0), (x * 0.2, y * 0.2, 9.5), 0.18, 0.08, IRON[0], segments=5, caps=False)
    p.box((0, 0, 2.2), (4.0, 4.0, 0.3), IRON[1])
    p.box((0, 0, 4.1), (2.2, 2.2, 0.25), IRON[1])
    p.cylinder((0, 0, 9.4), 0.08, 1.4, IRON[0], segments=5)
    return p


def domed_hall(seed=68):
    """A great dome on a square hall with corner towers (cathedrals, mosques, mausolea, universities)."""
    p = Piece("SM_DomedHall", seed)
    _base(p)
    p.box((0, 0, 0.2 + 1.6), (6.0, 6.0, 3.2), WALL[0])
    p.cylinder((0, 0, 3.4), 2.0, 1.0, WALL[1], segments=14)
    _hemi(p, 0, 0, 4.4, 2.0, DOME[0], squash=1.1)
    p.cylinder((0, 0, 6.5), 0.08, 0.8, GOLD, segments=5)
    for x in (-1, 1):
        for y in (-1, 1):
            p.cylinder((x * 3.6, y * 3.6, 0.2), 0.35, 5.8, WALL[0], segments=8)
            p.cone((x * 3.6, y * 3.6, 6.0), 0.4, 0.9, DOME[0], segments=8)
    p.box((0, -3.05, 1.3), (1.4, 0.15, 2.2), DARK)
    return p


def arena(seed=69):
    """A great oval arena of arches (amphitheatres, stadiums, opera houses)."""
    p = Piece("SM_Arena", seed)
    _base(p)
    p.loft([(0.2, 4.2, 3.4), (3.6, 4.2, 3.4), (3.6, 3.1, 2.3), (1.2, 2.7, 1.9)], STONE[2], segments=24, cap_top=False)
    p.box((0, 0, 0.35), (5.2, 3.6, 0.2), "#c9b98f")
    for row, z in enumerate((1.2, 2.6)):
        for i in range(16):
            a = 2 * math.pi * (i + 0.5 * row) / 16
            p.box((4.22 * math.cos(a), 3.42 * math.sin(a), z), (0.55, 0.4, 0.9), DARK, rot_z=a)
    return p


def citadel(seed=70):
    """A walled hill-town of terraces and towers (castles, palaces, rock-cut cities)."""
    p = Piece("SM_Citadel", seed)
    p.blob((0, 0, 0.6), 4.4, STONE[1], squash=0.35, wobble=0.15)
    s = 3.2
    for x, y, w, d in ((0, -s, 2 * s, 0.5), (0, s, 2 * s, 0.5), (-s, 0, 0.5, 2 * s), (s, 0, 0.5, 2 * s)):
        p.box((x, y, 1.9), (w, d, 1.6), STONE[0])
    for x in (-s, s):
        for y in (-s, s):
            p.cylinder((x, y, 1.0), 0.6, 2.6, STONE[2], segments=8)
            p.cone((x, y, 3.6), 0.7, 1.0, ROOF[1], segments=8)
    p.box((0, 0.4, 3.2), (3.4, 2.8, 2.4), WALL[0])
    p.hip((0, 0.4, 4.4), 3.4, 2.8, 1.0, ROOF[0])
    p.box((1.4, 1.6, 4.6), (1.0, 1.0, 4.0), WALL[1])
    p.cone((1.4, 1.6, 6.6), 0.8, 1.4, ROOF[0], segments=4)
    return p


def arsenal(seed=71):
    """Great sheds, cranes and a slipway (arsenals, canals, industrial wonders)."""
    p = Piece("SM_Arsenal", seed)
    _base(p, color="#8d877c")
    for k, x in enumerate((-2.2, 1.0)):
        p.box((x, 1.2, 0.2 + 1.5), (2.8, 5.0, 3.0), BRICK[k])
        p.gable((x, 1.2, 3.2), 5.0, 2.8, 1.4, ROOF[1], overhang=0.1, rot_z=math.pi / 2)
    p.cylinder((3.4, 2.6, 0.2), 0.45, 6.4, BRICK[1], segments=8, radius_top=0.32)
    p.box((0, -3.2, 0.25), (8.0, 2.0, 0.1), WATER)
    p.segment((3.4, -1.6, 0.2), (3.4, -1.6, 5.0), 0.15, 0.12, IRON[0], segments=5, caps=False)
    p.segment((3.4, -1.6, 4.9), (0.6, -3.0, 4.3), 0.1, 0.08, IRON[0], segments=5, caps=False)
    return p


PIECES = [pyramid, step_pyramid, stone_circle, gardens, statue, tower, lattice_tower, domed_hall, arena, citadel, arsenal]
