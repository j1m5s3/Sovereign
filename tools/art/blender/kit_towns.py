"""Towns kit: the later eras' city centres, in the Classical kit's sizes so the map draws them the same way.
For each style, three houses, a hall (a city's centre) and a palace (a capital's):
  Medieval (Medieval and Renaissance): timber-framed houses, a stone church, a castle keep.
  Industrial (Industrial and Modern): brick terraces with chimneys, a clock-tower town hall, a domed state house.
  Modern (Atomic onward): flat-roofed blocks, an office tower, a glass capitol.
"""
import math

import kitlib
from kitlib import Piece

PLASTER = ["#e6dcc6", "#ddd0b4", "#efe6d4"]
BEAM = ["#4a3526", "#3d2c20"]
SLATE = ["#5b5f66", "#4f535a"]
TILE = ["#8f4a35", "#9c5640"]
STONE = ["#a39c8f", "#938c80", "#b1aa9d"]
BRICK = ["#9a5a42", "#8a4f3a", "#a5654b"]
TRIM = "#e8e2d6"
CONCRETE = ["#c9c6bf", "#b9b6af", "#d6d3cc"]
GLASS = ["#5c7f99", "#4f6f87"]
DARK = "#2e2722"
GOLD = "#c9a43c"
COPPER = "#6d8f8a"

kitlib.patterns({"plaster": PLASTER + [TRIM], "wood": BEAM, "rooftile": TILE + BRICK, "stone": STONE + CONCRETE, "metal": SLATE + GLASS + [GOLD, COPPER]})


def _windows(p, w, d, floors, h, color=DARK, step=None):
    """Windows on the front (-Y) and one side (+X), per floor."""
    for f in range(floors):
        z = f * h + h * 0.6
        n = max(1, int(w / 1.8))
        for i in range(n):
            x = -w / 2 + w * (i + 0.5) / n
            p.box((x, -d / 2 - 0.03, z), (0.7, 0.08, 0.9), color)
        m = max(1, int(d / 1.8))
        for i in range(m):
            y = -d / 2 + d * (i + 0.5) / m
            p.box((w / 2 + 0.03, y, z), (0.08, 0.7, 0.9), color)


# ---------------------------------------------------------------- Medieval

def _timber(p, w, d, h, wall, z0=0.0):
    p.box((0, 0, z0 + h / 2), (w, d, h), wall)
    for x in (-w / 2, 0.0, w / 2):
        p.box((x, -d / 2 - 0.04, z0 + h / 2), (0.25, 0.08, h), BEAM[0])
    p.box((0, -d / 2 - 0.04, z0 + h - 0.15), (w, 0.08, 0.25), BEAM[1])
    p.box((0, -d / 2 - 0.04, z0 + 0.15), (w, 0.08, 0.25), BEAM[1])
    for s in (-1, 1):
        p.segment((s * w / 2, -d / 2 - 0.05, z0 + 0.3), (0.0, -d / 2 - 0.05, z0 + h - 0.3), 0.1, 0.1, BEAM[0], segments=4, caps=False)


def medieval_house_a(seed=101):
    p = Piece("SM_Medieval_House_A", seed)
    p.box((0, 0, 1.5), (6.0, 5.0, 3.0), STONE[0])
    _timber(p, 6.4, 5.4, 2.6, PLASTER[0], z0=3.0)  # the upper floor jetties out
    p.gable((0, 0, 5.6), 6.4, 5.4, 3.2, TILE[0], overhang=0.3)
    p.box((0, -2.53, 1.0), (1.0, 0.08, 2.0), DARK)
    return p


def medieval_house_b(seed=102):
    p = Piece("SM_Medieval_House_B", seed)
    _timber(p, 5.0, 6.0, 3.0, PLASTER[1])
    _timber(p, 5.0, 6.0, 2.6, PLASTER[2], z0=3.0)
    p.gable((0, 0, 5.6), 6.0, 5.0, 3.6, SLATE[0], overhang=0.3, rot_z=math.pi / 2)
    p.box((1.6, 2.0, 7.0), (0.7, 0.7, 2.0), STONE[1])  # chimney
    return p


def medieval_house_c(seed=103):
    p = Piece("SM_Medieval_House_C", seed)
    _timber(p, 7.0, 4.6, 3.0, PLASTER[2])
    p.gable((0, 0, 3.0), 7.0, 4.6, 2.6, "#b89b5e", overhang=0.4)  # thatch
    p.box((-3.0, 3.0, 1.0), (1.2, 1.4, 2.0), BEAM[0])  # a lean-to
    return p


def medieval_hall(seed=104):
    """A stone church with a spire."""
    p = Piece("SM_Medieval_Hall", seed)
    p.box((0, 1.0, 3.0), (6.0, 12.0, 6.0), STONE[0])
    p.gable((0, 1.0, 6.0), 12.0, 6.0, 3.6, SLATE[1], overhang=0.2, rot_z=math.pi / 2)
    p.box((0, -6.0, 5.0), (4.0, 4.0, 10.0), STONE[2])
    p.cone((0, -6.0, 10.0), 2.9, 7.0, SLATE[0], segments=4)
    p.box((0, -8.03, 1.6), (1.6, 0.08, 3.2), DARK)
    for y in (-2.0, 1.0, 4.0):
        p.box((3.03, y, 3.4), (0.08, 0.9, 2.6), DARK)
    return p


def medieval_palace(seed=105):
    """A castle keep with corner towers and a curtain wall."""
    p = Piece("SM_Medieval_Palace", seed)
    s = 8.0
    for x, y, w, d in ((0, -s, 2 * s, 1.2), (0, s, 2 * s, 1.2), (-s, 0, 1.2, 2 * s), (s, 0, 1.2, 2 * s)):
        p.box((x, y, 2.5), (w, d, 5.0), STONE[1])
    for x in (-s, s):
        for y in (-s, s):
            p.cylinder((x, y, 0), 1.8, 7.0, STONE[0], segments=10)
            p.cone((x, y, 7.0), 2.1, 3.0, SLATE[0], segments=10)
    p.box((0, 0, 6.0), (8.0, 8.0, 12.0), STONE[2])
    for k in range(3):  # crenellations on the keep
        x = -3.0 + 3.0 * k
        p.box((x, -4.1, 12.5), (1.2, 0.6, 1.0), STONE[0])
        p.box((x, 4.1, 12.5), (1.2, 0.6, 1.0), STONE[0])
    p.box((0, -s - 0.62, 1.8), (2.6, 0.1, 3.6), DARK)
    return p


# ---------------------------------------------------------------- Industrial

def _terrace(p, w, d, floors, wall, roof):
    h = 3.0
    p.box((0, 0, floors * h / 2), (w, d, floors * h), wall)
    p.box((0, 0, floors * h + 0.2), (w + 0.2, d + 0.2, 0.4), TRIM)
    p.gable((0, 0, floors * h + 0.4), w, d, 1.8, roof, overhang=0.15)
    _windows(p, w, d, floors, h)
    for x in (-w / 2 + 0.6, w / 2 - 0.6):
        p.box((x, 0, floors * h + 2.0), (0.6, 0.9, 2.2), BRICK[1])  # chimneys


def industrial_house_a(seed=111):
    p = Piece("SM_Industrial_House_A", seed)
    _terrace(p, 7.0, 5.0, 2, BRICK[0], SLATE[0])
    p.box((0, -2.53, 1.1), (1.0, 0.08, 2.2), DARK)
    return p


def industrial_house_b(seed=112):
    p = Piece("SM_Industrial_House_B", seed)
    _terrace(p, 5.5, 6.0, 3, BRICK[2], SLATE[1])
    return p


def industrial_house_c(seed=113):
    p = Piece("SM_Industrial_House_C", seed)
    _terrace(p, 8.0, 4.8, 2, PLASTER[0], SLATE[0])
    p.box((0, -2.6, 3.0), (8.2, 0.4, 0.2), TRIM)
    return p


def industrial_hall(seed=114):
    """A town hall with a clock tower."""
    p = Piece("SM_Industrial_Hall", seed)
    p.box((0, 1.0, 3.5), (12.0, 8.0, 7.0), BRICK[0])
    p.hip((0, 1.0, 7.0), 12.0, 8.0, 2.4, SLATE[0], overhang=0.2)
    _windows(p, 12.0, 8.0, 2, 3.5)
    p.box((0, -3.2, 7.0), (3.4, 3.4, 14.0), BRICK[1])
    p.box((0, -4.93, 11.5), (2.2, 0.1, 2.2), TRIM)
    p.cylinder((0, -5.0, 11.5), 0.9, 0.12, DARK, segments=12, rot_z=0.0)
    p.cone((0, -3.2, 14.0), 2.4, 3.6, SLATE[1], segments=4)
    p.box((0, -4.93, 1.4), (1.8, 0.1, 2.8), DARK)
    return p


def industrial_palace(seed=115):
    """A domed state house with a portico."""
    p = Piece("SM_Industrial_Palace", seed)
    p.box((0, 0, 0.4), (20.0, 14.0, 0.8), STONE[0])
    p.box((0, 1.0, 0.8 + 4.0), (18.0, 10.0, 8.0), PLASTER[2])
    _windows(p, 18.0, 10.0, 2, 4.0)
    for i in range(6):
        x = -4.5 + 9.0 * i / 5
        p.cylinder((x, -4.6, 0.8), 0.45, 7.0, TRIM, segments=10)
    p.gable((0, -4.0, 7.8), 10.0, 2.6, 1.8, PLASTER[1], overhang=0.2)
    p.cylinder((0, 1.0, 8.8), 3.6, 2.6, PLASTER[2], segments=16)
    v = p.blob((0, 1.0, 11.4), 3.6, COPPER, squash=1.0, subdiv=2, wobble=0.0)
    for q in v:
        q.co.z = max(q.co.z, 11.4)
    p.cylinder((0, 1.0, 14.8), 0.5, 1.4, TRIM, segments=8)
    return p


# ---------------------------------------------------------------- Modern

def _block(p, w, d, floors, wall, glass=GLASS[0]):
    h = 3.2
    p.box((0, 0, floors * h / 2), (w, d, floors * h), wall)
    for f in range(floors):
        z = f * h + h * 0.55
        p.box((0, -d / 2 - 0.03, z), (w - 0.6, 0.08, 1.4), glass)
        p.box((w / 2 + 0.03, 0, z), (0.08, d - 0.6, 1.4), glass)
    p.box((0, 0, floors * h + 0.15), (w + 0.1, d + 0.1, 0.3), CONCRETE[1])


def modern_house_a(seed=121):
    p = Piece("SM_Modern_House_A", seed)
    _block(p, 7.0, 6.0, 3, CONCRETE[0])
    p.box((1.5, 1.0, 10.3), (2.0, 2.0, 1.2), CONCRETE[1])
    return p


def modern_house_b(seed=122):
    p = Piece("SM_Modern_House_B", seed)
    _block(p, 6.0, 6.0, 5, CONCRETE[2], GLASS[1])
    return p


def modern_house_c(seed=123):
    p = Piece("SM_Modern_House_C", seed)
    _block(p, 8.0, 5.0, 2, BRICK[2])
    p.box((0, -2.8, 3.2), (8.0, 0.6, 0.2), CONCRETE[1])
    return p


def modern_hall(seed=124):
    """An office tower on a podium."""
    p = Piece("SM_Modern_Hall", seed)
    p.box((0, 0, 2.0), (12.0, 10.0, 4.0), CONCRETE[0])
    p.box((0, 0, 4.0 + 9.0), (6.0, 6.0, 18.0), GLASS[0])
    for f in range(6):
        p.box((0, 0, 4.0 + 3.0 * (f + 1)), (6.1, 6.1, 0.25), CONCRETE[2])
    p.box((0, 0, 22.3), (4.0, 4.0, 0.6), CONCRETE[1])
    p.box((0, -5.03, 1.4), (3.0, 0.08, 2.4), GLASS[1])
    return p


def modern_palace(seed=125):
    """A glass capitol: a wide low hall round a tall slender tower, with a flag."""
    p = Piece("SM_Modern_Palace", seed)
    p.box((0, 0, 0.3), (22.0, 16.0, 0.6), CONCRETE[1])
    p.box((0, 2.0, 0.6 + 3.0), (18.0, 9.0, 6.0), CONCRETE[2])
    p.box((0, -2.53, 0.6 + 3.0), (16.0, 0.1, 4.0), GLASS[1])
    p.cylinder((0, 3.0, 6.6), 3.0, 18.0, GLASS[0], segments=12, radius_top=2.2)
    p.cylinder((0, 3.0, 24.6), 2.3, 0.6, CONCRETE[0], segments=12)
    p.cylinder((7.0, -5.5, 0.6), 0.1, 8.0, CONCRETE[0], segments=6)
    p.team_color(True)
    p.box((7.9, -5.5, 7.6), (1.8, 0.05, 1.1), TRIM)
    p.team_color(False)
    return p


PIECES = [medieval_house_a, medieval_house_b, medieval_house_c, medieval_hall, medieval_palace,
          industrial_house_a, industrial_house_b, industrial_house_c, industrial_hall, industrial_palace,
          modern_house_a, modern_house_b, modern_house_c, modern_hall, modern_palace]
