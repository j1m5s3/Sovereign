"""Classical City Center kit, temperate style, Ancient/Classical era band (world doc, Production
order 1). Whitewashed walls, terracotta roofs and marble landmarks; owner colours go on
banners through the material's Tint, so the buildings themselves stay neutral.
"""
import math

from kitlib import Piece

WALL = ["#e3d9c3", "#d9cdb2", "#ece4d2"]
ROOF = ["#a9533a", "#b45f42", "#9c4b34"]
MARBLE = ["#ebe7df", "#dedad1"]
STONE = ["#a39c8f", "#938c80"]
WOOD = "#6b4c33"
DARK = "#3a2f28"
THATCH = "#b89b5e"
CLOTH = ["#c9b27a", "#a8452f", "#e8e2d6"]


def _door_and_windows(p, w, d, h, rng_floor=0.0):
    # Front (-Y) door and a window either side; side windows.
    p.box((0, -d / 2 - 0.02, rng_floor + 1.05), (1.0, 0.08, 2.1), DARK)
    for x in (-w / 3, w / 3):
        p.box((x, -d / 2 - 0.02, rng_floor + 1.7), (0.7, 0.08, 0.8), DARK)
    for y in (-d / 4, d / 4):
        p.box((w / 2 + 0.02, y, rng_floor + 1.7), (0.08, 0.6, 0.8), DARK)


def house_a(seed=11):
    p = Piece("SM_House_A", seed)
    w, d, h = 6.0, 5.0, 3.2
    p.box((0, 0, h / 2), (w, d, h), WALL[0])
    p.gable((0, 0, h), w, d, 1.8, ROOF[0])
    _door_and_windows(p, w, d, h)
    return p


def house_b(seed=12):
    p = Piece("SM_House_B", seed)
    w, d, h = 5.0, 5.0, 5.8
    p.box((0, 0, h / 2), (w, d, h), WALL[1])
    p.box((0, 0, h + 0.25), (w + 0.2, d + 0.2, 0.5), WALL[2])  # parapet band
    p.box((0, 0, h + 0.05), (w - 0.6, d - 0.6, 0.3), ROOF[1])   # flat terrace
    _door_and_windows(p, w, d, h)
    _door_and_windows(p, w, d, h, rng_floor=2.9)
    return p


def house_c(seed=13):
    p = Piece("SM_House_C", seed)
    p.box((0, 0, 1.6), (7.0, 4.5, 3.2), WALL[2])
    p.gable((0, 0, 3.2), 7.0, 4.5, 1.6, ROOF[2])
    p.box((2.2, 3.2, 1.4), (2.6, 2.4, 2.8), WALL[0])
    p.gable((2.2, 3.2, 2.8), 2.4, 2.6, 1.2, ROOF[0], rot_z=math.pi / 2)
    _door_and_windows(p, 7.0, 4.5, 3.2)
    return p


def house_boarded(seed=14):
    p = Piece("SM_House_Boarded", seed)
    w, d, h = 6.0, 5.0, 3.2
    p.box((0, 0, h / 2), (w, d, h), "#a89c86")
    p.gable((0, 0, h), w, d, 1.8, "#7a4434")
    p.box((0, -d / 2 - 0.05, 1.05), (1.0, 0.1, 2.1), WOOD)
    for x in (-w / 3, w / 3):  # boards nailed across the shutters
        p.box((x, -d / 2 - 0.06, 1.7), (0.9, 0.08, 0.16), WOOD, rot_z=0.0)
        p.box((x, -d / 2 - 0.07, 1.55), (0.9, 0.08, 0.16), WOOD)
    return p


def _colonnade(p, x0, x1, y, h, n, r=0.32):
    for i in range(n):
        x = x0 + (x1 - x0) * i / max(1, n - 1)
        p.cylinder((x, y, 0.0), r, h, MARBLE[0], segments=8)


def palace(seed=21):
    p = Piece("SM_Palace", seed)
    # Stepped podium, a columned front hall and a pediment roof.
    p.box((0, 0, 0.3), (18.0, 14.0, 0.6), STONE[0])
    p.box((0, 0, 0.8), (17.0, 13.0, 0.4), STONE[1])
    hall_h = 6.5
    p.box((0, 1.5, 1.0 + hall_h / 2), (14.0, 9.0, hall_h), MARBLE[1])
    for i in range(8):
        x = -6.3 + 12.6 * i / 7
        p.cylinder((x, -4.6, 1.0), 0.45, hall_h, MARBLE[0], segments=10)
    p.box((0, -1.0, 1.0 + hall_h + 0.35), (15.0, 13.0, 0.7), MARBLE[0])  # entablature
    p.gable((0, -1.0, 1.0 + hall_h + 0.7), 15.0, 13.0, 2.6, ROOF[0], overhang=0.2, rot_z=math.pi / 2)
    p.box((0, -4.0, 1.0 + 1.4), (2.2, 0.2, 2.8), DARK)  # great door
    return p


def monument(seed=22):
    p = Piece("SM_Monument", seed)
    p.box((0, 0, 0.4), (3.2, 3.2, 0.8), STONE[0])
    p.box((0, 0, 1.1), (2.2, 2.2, 0.6), STONE[1])
    p.box((0, 0, 1.4 + 3.2), (1.0, 1.0, 6.4), MARBLE[0], taper=0.55)
    p.cone((0, 0, 7.8), 0.42, 0.9, MARBLE[1], segments=4)
    return p


def granary(seed=23):
    p = Piece("SM_Granary", seed)
    p.cylinder((0, 0, 0), 3.0, 3.4, WALL[1], segments=14)
    p.cone((0, 0, 3.4), 3.6, 2.8, THATCH, segments=14)
    p.box((0, -3.0, 1.0), (1.1, 0.3, 2.0), DARK)
    p.box((4.0, 0, 0.6), (2.0, 1.6, 1.2), WOOD)  # sacks and crates
    return p


def temple(seed=24):
    p = Piece("SM_Temple", seed)
    p.box((0, 0, 0.5), (10.0, 14.0, 1.0), STONE[0])
    h = 5.0
    p.box((0, 1.0, 1.0 + h / 2), (6.5, 9.0, h), MARBLE[1])
    for side in (-1, 1):
        for i in range(5):
            p.cylinder((side * 4.2, -5.5 + 11.0 * i / 4, 1.0), 0.36, h, MARBLE[0], segments=8)
    for i in range(4):
        p.cylinder((-3.0 + 2.0 * i, -6.3, 1.0), 0.36, h, MARBLE[0], segments=8)
    p.box((0, 0, 1.0 + h + 0.3), (9.6, 13.6, 0.6), MARBLE[0])
    p.gable((0, 0, 1.0 + h + 0.6), 13.6, 9.6, 2.0, ROOF[1], overhang=0.15, rot_z=math.pi / 2)
    return p


def landmark_generic(seed=25):
    p = Piece("SM_Landmark", seed)
    p.box((0, 0, 0.3), (10.0, 8.0, 0.6), STONE[0])
    p.box((0, 0.8, 0.6 + 2.6), (8.5, 6.0, 5.2), WALL[0])
    p.hip((0, 0.8, 5.8), 8.5, 6.0, 2.2, ROOF[2])
    _colonnade(p, -3.0, 3.0, -2.9, 3.6, 4, r=0.3)
    p.gable((0, -3.0, 4.2), 7.0, 1.4, 1.0, ROOF[0], overhang=0.2)
    p.box((0, -2.25, 1.6), (1.4, 0.15, 2.0), DARK)
    return p


def wall_segment(seed=26):
    p = Piece("SM_Wall", seed)
    L, T, H = 10.0, 1.6, 5.0
    p.box((0, 0, H / 2), (L, T, H), STONE[0])
    for i in range(7):  # crenellations
        p.box((-L / 2 + 0.7 + i * (L - 1.4) / 6, -T / 2 + 0.25, H + 0.45), (0.8, 0.5, 0.9), STONE[1])
    return p


def market_stall(seed=27):
    p = Piece("SM_MarketStall", seed)
    p.box((0, 0, 0.45), (2.6, 1.4, 0.9), WOOD)
    for x in (-1.2, 1.2):
        for y in (-0.6, 0.6):
            p.cylinder((x, y, 0), 0.06, 2.3, WOOD, segments=5)
    for i in range(4):  # striped canopy
        p.box((-1.05 + 0.7 * i, 0, 2.35), (0.7, 1.7, 0.08), CLOTH[i % 2])
    p.blob((-0.6, 0, 1.0), 0.25, "#c8a53c", squash=0.6)
    p.blob((0.5, 0.1, 1.0), 0.25, "#8f3b2a", squash=0.6)
    return p


def banner(seed=28):
    p = Piece("SM_Banner", seed)
    p.cylinder((0, 0, 0), 0.07, 6.0, WOOD, segments=6)
    p.team_color(True)
    p.box((0.75, 0, 4.6), (1.4, 0.05, 2.4), CLOTH[2])  # team slot: the owner's colour in game
    p.team_color(False)
    p.cone((0, 0, 6.0), 0.15, 0.4, "#c9a43c", segments=6)
    return p


PIECES = [house_a, house_b, house_c, house_boarded, palace, monument, granary, temple, landmark_generic, wall_segment,
          market_stall, banner]
