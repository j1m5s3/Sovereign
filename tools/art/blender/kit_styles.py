"""Styles kit: the early city centres of the launch roster's other architectural styles
(specs/sovereign/leaders-and-art-style.md, two civs per style), in the Classical kit's sizes:
  MiddleEast (Persia, Arabia): mud-brick and plaster cubes with flat roofs and a parapet, a domed hall, an iwan palace.
  Asian (China, Japan): timber halls on stone plinths with curved, tiled hip roofs, a pagoda, a tiered palace.
  African (Egypt, Mali): mud-brick with protruding beams (toron), a pylon gate hall, a Sahelian palace with buttresses.
  American (Aztec, Inca): fitted-stone houses with thatch, a stepped shrine, a terraced palace.
Mediterranean civs use the Classical kit and European ones the Towns kit's Medieval pieces (SovArt::EraPiece).
"""
import math

import kitlib
from kitlib import Piece

MUD = ["#c9a878", "#bf9b69", "#d4b789"]
PLASTER = ["#e9e0cc", "#ded2b7"]
TILE_BLUE = ["#2f6f8f", "#3b84a3"]
DARK = "#2e2722"
WOOD = ["#6b4c33", "#5a3f2b"]
RED_LACQUER = "#9a3324"
ROOF_GREY = ["#4d5560", "#5a626c"]
ROOF_GREEN = "#3f6e5a"
STONE = ["#a39c8f", "#938c80", "#b1aa9d"]
SANDSTONE = ["#d8c08c", "#cdb27a"]
THATCH = ["#b89b5e", "#a88c52"]
INCA_STONE = ["#8f8a80", "#7f7a71"]
GOLD = "#c9a43c"
PAINT = ["#a8452f", "#3f6fa0", "#d8b040"]

kitlib.patterns({"plaster": MUD + PLASTER + SANDSTONE, "rooftile": ROOF_GREY + [ROOF_GREEN, RED_LACQUER], "wood": WOOD,
                 "stone": STONE + INCA_STONE, "thatch": THATCH, "metal": TILE_BLUE + [GOLD], "cloth": PAINT})


def _door(p, x, y, h=2.0, w=1.0):
    p.box((x, y, h / 2), (w, 0.08, h), DARK)


# ---------------------------------------------------------------- Middle Eastern

def _flat(p, w, d, h, wall, z0=0.0):
    p.box((0, 0, z0 + h / 2), (w, d, h), wall)
    p.box((0, 0, z0 + h + 0.2), (w + 0.1, d + 0.1, 0.4), wall)  # parapet
    p.box((0, 0, z0 + h + 0.05), (w - 0.5, d - 0.5, 0.3), MUD[1])  # roof terrace


def _dome(p, x, y, z, r, color):
    v = p.blob((x, y, z), r, color, squash=1.15, subdiv=2, wobble=0.0)
    for q in v:
        q.co.z = max(q.co.z, z)


def middleeast_house_a(seed=201):
    p = Piece("SM_MiddleEast_House_A", seed)
    _flat(p, 6.0, 5.0, 3.4, MUD[0])
    _door(p, 0, -2.53)
    for x in (-2.0, 2.0):
        p.box((x, -2.53, 2.4), (0.6, 0.08, 0.8), DARK)
    return p


def middleeast_house_b(seed=202):
    p = Piece("SM_MiddleEast_House_B", seed)
    _flat(p, 5.0, 5.0, 5.8, PLASTER[0])
    p.box((0, -2.6, 4.2), (2.4, 0.5, 1.2), WOOD[0])  # a latticed oriel window
    _door(p, 0, -2.53)
    p.box((1.5, 1.5, 6.8), (1.0, 1.0, 1.4), MUD[2])  # wind tower
    return p


def middleeast_house_c(seed=203):
    p = Piece("SM_MiddleEast_House_C", seed)
    _flat(p, 7.0, 4.5, 3.0, MUD[2])
    _flat(p, 3.0, 3.0, 2.4, MUD[0], z0=3.0)
    _door(p, -1.5, -2.28)
    return p


def middleeast_hall(seed=204):
    """A domed hall with a tiled portal and a slender minaret."""
    p = Piece("SM_MiddleEast_Hall", seed)
    p.box((0, 0, 0.3), (12.0, 10.0, 0.6), SANDSTONE[1])
    p.box((0, 0.5, 0.6 + 3.0), (9.0, 8.0, 6.0), PLASTER[0])
    p.cylinder((0, 0.5, 6.6), 3.2, 1.0, PLASTER[1], segments=16)
    _dome(p, 0, 0.5, 7.6, 3.2, TILE_BLUE[0])
    p.box((0, -3.6, 3.8), (4.0, 1.0, 7.6), TILE_BLUE[1])  # the portal
    p.box((0, -4.12, 2.2), (2.0, 0.1, 4.4), DARK)
    p.cylinder((5.2, 3.8, 0.6), 0.6, 13.0, PLASTER[1], segments=10, radius_top=0.45)
    p.cylinder((5.2, 3.8, 10.0), 0.8, 0.4, SANDSTONE[0], segments=10)
    p.cone((5.2, 3.8, 13.6), 0.5, 1.4, TILE_BLUE[0], segments=10)
    return p


def middleeast_palace(seed=205):
    """A palace round a great iwan, with domes and corner towers."""
    p = Piece("SM_MiddleEast_Palace", seed)
    p.box((0, 0, 0.4), (20.0, 15.0, 0.8), SANDSTONE[1])
    p.box((0, 1.5, 0.8 + 4.0), (18.0, 10.0, 8.0), PLASTER[0])
    p.box((0, -3.9, 0.8 + 5.5), (6.0, 1.2, 11.0), TILE_BLUE[1])  # iwan frame
    p.box((0, -4.55, 0.8 + 3.5), (3.6, 0.1, 7.0), DARK)  # the vault's shadow
    for x in (-6.0, 6.0):
        _dome(p, x, 2.0, 8.8, 2.4, TILE_BLUE[0])
    for x in (-9.2, 9.2):
        for y in (-3.6, 6.6):
            p.cylinder((x, y, 0.8), 0.9, 10.0, SANDSTONE[0], segments=10)
            _dome(p, x, y, 10.8, 0.9, GOLD)
    return p


# ---------------------------------------------------------------- Asian

def _curved_hip(p, center, length, width, rise, color, overhang=0.6):
    """A hip roof whose eaves turn up at the corners."""
    cx, cy, cz = center
    v = p.hip((cx, cy, cz), length, width, rise, color, overhang=overhang)
    L, W = length / 2 + overhang, width / 2 + overhang
    for q in v:
        dx, dy = abs(q.co.x - cx), abs(q.co.y - cy)
        if dx > L * 0.95 and dy > W * 0.95:
            q.co.z += rise * 0.35  # upturned corners
    return v


def _timber_hall(p, w, d, h, wall, roof, z0=0.6, rise=1.8):
    p.box((0, 0, z0 / 2), (w + 1.0, d + 1.0, z0), STONE[2])  # plinth
    p.box((0, 0, z0 + h / 2), (w, d, h), wall)
    for x in (-w / 2, -w / 6, w / 6, w / 2):
        p.box((x, -d / 2 - 0.05, z0 + h / 2), (0.3, 0.12, h), RED_LACQUER)  # columns
    _curved_hip(p, (0, 0, z0 + h), w, d, rise, roof)


def asian_house_a(seed=211):
    p = Piece("SM_Asian_House_A", seed)
    _timber_hall(p, 6.0, 4.6, 2.8, PLASTER[0], ROOF_GREY[0])
    _door(p, 0, -2.36, 2.0, 1.2)
    return p


def asian_house_b(seed=212):
    p = Piece("SM_Asian_House_B", seed)
    _timber_hall(p, 5.0, 5.0, 2.6, WOOD[1], ROOF_GREY[1])
    _curved_hip(p, (0, 0, 5.4), 3.6, 3.6, 1.4, ROOF_GREY[0], overhang=0.4)
    p.box((0, 0, 4.6), (3.4, 3.4, 1.4), WOOD[1])  # upper storey
    return p


def asian_house_c(seed=213):
    p = Piece("SM_Asian_House_C", seed)
    _timber_hall(p, 7.0, 4.4, 2.6, PLASTER[1], ROOF_GREY[0], rise=1.5)
    p.box((-4.6, 0, 1.0), (0.3, 4.4, 2.0), PLASTER[1])  # a courtyard wall
    return p


def asian_hall(seed=214):
    """A five-storey pagoda."""
    p = Piece("SM_Asian_Hall", seed)
    p.box((0, 0, 0.4), (9.0, 9.0, 0.8), STONE[2])
    z, s = 0.8, 6.0
    for i in range(5):
        h = 2.6 - 0.2 * i
        p.box((0, 0, z + h / 2), (s, s, h), RED_LACQUER if i == 0 else WOOD[0])
        _curved_hip(p, (0, 0, z + h), s, s, 0.9, ROOF_GREY[i % 2], overhang=0.9)
        z += h + 0.5
        s *= 0.82
    p.cylinder((0, 0, z - 0.3), 0.12, 3.0, GOLD, segments=8)  # spire
    return p


def asian_palace(seed=215):
    """A great hall on a three-tier terrace, a gate before it."""
    p = Piece("SM_Asian_Palace", seed)
    for i, (w, d) in enumerate(((22.0, 16.0), (20.0, 14.0), (18.0, 12.0))):
        p.box((0, 1.0, 0.45 + 0.9 * i), (w, d, 0.9), STONE[i % 3])
    p.box((0, 2.0, 2.7 + 2.8), (14.0, 8.0, 5.6), RED_LACQUER)
    for x in (-6.0, -3.0, 0.0, 3.0, 6.0):
        p.box((x, -2.1, 2.7 + 2.8), (0.5, 0.3, 5.6), WOOD[1])
    _curved_hip(p, (0, 2.0, 8.3), 14.0, 8.0, 2.4, GOLD, overhang=1.2)
    p.box((0, 2.0, 10.4), (10.0, 4.4, 1.6), RED_LACQUER)
    _curved_hip(p, (0, 2.0, 12.0), 10.0, 4.4, 2.0, GOLD, overhang=1.0)
    p.box((0, -6.5, 2.5), (6.0, 1.6, 5.0), RED_LACQUER)  # the gate
    _curved_hip(p, (0, -6.5, 5.0), 6.0, 1.6, 1.2, ROOF_GREY[0], overhang=0.8)
    return p


# ---------------------------------------------------------------- African

def _toron(p, w, d, h, wall, z0=0.0, step=1.2):
    """Mud-brick with protruding beam ends on the front and side."""
    p.box((0, 0, z0 + h / 2), (w, d, h), wall)
    for z in (z0 + h * 0.4, z0 + h * 0.8):
        for x in [(-w / 2 + step * (i + 0.5)) for i in range(int(w / step))]:
            p.box((x, -d / 2 - 0.25, z), (0.12, 0.5, 0.12), WOOD[0])


def african_house_a(seed=221):
    p = Piece("SM_African_House_A", seed)
    _toron(p, 6.0, 5.0, 3.4, MUD[1])
    p.box((0, 0, 3.6), (6.2, 5.2, 0.4), MUD[0])
    for x in (-2.8, 2.8):
        p.cone((x, -2.4, 3.8), 0.35, 0.9, MUD[0], segments=6)  # pinnacles
    _door(p, 0, -2.53)
    return p


def african_house_b(seed=222):
    p = Piece("SM_African_House_B", seed)
    p.cylinder((0, 0, 0), 2.6, 2.8, MUD[2], segments=12)  # a round house
    p.cone((0, 0, 2.8), 3.3, 2.6, THATCH[0], segments=12)
    _door(p, 0, -2.6)
    return p


def african_house_c(seed=223):
    p = Piece("SM_African_House_C", seed)
    _flat(p, 7.0, 4.6, 3.0, SANDSTONE[0])
    for x in (-2.0, 0.0, 2.0):
        p.box((x, -2.34, 2.0), (0.6, 0.08, 1.0), DARK)
    p.box((0, -2.36, 3.1), (7.2, 0.1, 0.3), PAINT[0])  # a painted band
    return p


def african_hall(seed=224):
    """A temple gate: two battered pylons and a doorway, a court behind."""
    p = Piece("SM_African_Hall", seed)
    p.box((0, 0, 0.3), (14.0, 12.0, 0.6), SANDSTONE[1])
    for x in (-3.4, 3.4):
        p.box((x, -3.0, 0.6 + 4.0), (5.0, 2.4, 8.0), SANDSTONE[0], taper=0.82)  # pylons
        p.box((x, -4.25, 5.0), (3.6, 0.1, 4.0), PAINT[1])  # painted relief
    p.box((0, -3.0, 0.6 + 3.0), (1.8, 2.0, 6.0), SANDSTONE[1])
    _door(p, 0, -4.05, 4.2, 1.4)
    p.box((0, 2.5, 0.6 + 2.5), (10.0, 6.0, 5.0), SANDSTONE[1])
    for x in (-4.0, -2.0, 0.0, 2.0, 4.0):
        p.cylinder((x, -0.6, 0.6), 0.45, 5.0, SANDSTONE[0], segments=8)
    return p


def african_palace(seed=225):
    """A Sahelian palace: a great buttressed mud-brick hall with conical towers and beam ends."""
    p = Piece("SM_African_Palace", seed)
    p.box((0, 0, 0.4), (20.0, 15.0, 0.8), MUD[0])
    p.box((0, 1.0, 0.8 + 4.5), (17.0, 11.0, 9.0), MUD[1])
    for x in [-8.5 + 17.0 * i / 6 for i in range(7)]:
        p.box((x, -4.6, 0.8 + 5.0), (1.2, 0.8, 10.0), MUD[2], taper=0.6)  # buttresses
        p.cone((x, -4.6, 10.8), 0.5, 1.8, MUD[2], segments=6)
    for z in (4.0, 7.0):
        for x in [-8.0 + 1.6 * i for i in range(11)]:
            p.box((x, -4.9, 0.8 + z), (0.15, 0.7, 0.15), WOOD[0])
    for x in (-5.0, 5.0):
        p.box((x, 4.0, 0.8 + 6.5), (3.4, 3.4, 13.0), MUD[2], taper=0.75)
        p.cone((x, 4.0, 13.8), 1.4, 3.0, MUD[1], segments=8)
    _door(p, 0, -4.53, 3.6, 2.0)
    return p


# ---------------------------------------------------------------- American

def _stone_house(p, w, d, h, roof=THATCH[0], stone=INCA_STONE[0]):
    p.box((0, 0, h / 2), (w, d, h), stone, taper=0.94)  # walls lean in a little
    p.hip((0, 0, h), w * 0.95, d * 0.95, 2.4, roof, overhang=0.35)
    p.box((0, -d / 2 - 0.02, 1.0), (0.9, 0.08, 2.0), DARK, taper=0.75)  # trapezoid door


def american_house_a(seed=231):
    p = Piece("SM_American_House_A", seed)
    _stone_house(p, 6.0, 4.6, 2.8)
    return p


def american_house_b(seed=232):
    p = Piece("SM_American_House_B", seed)
    p.box((0, 0, 0.5), (7.0, 6.0, 1.0), STONE[1])  # a platform
    p.box((0, 0, 1.0 + 1.4), (5.0, 4.0, 2.8), PLASTER[0])
    p.hip((0, 0, 3.8), 5.0, 4.0, 2.2, THATCH[1], overhang=0.4)
    p.box((0, -2.02, 2.0), (0.9, 0.08, 1.8), DARK)
    p.box((0, -2.04, 3.4), (5.0, 0.06, 0.3), PAINT[0])
    return p


def american_house_c(seed=233):
    p = Piece("SM_American_House_C", seed)
    _stone_house(p, 7.5, 4.0, 2.6, THATCH[1], INCA_STONE[1])
    p.box((-4.6, 0, 0.6), (1.6, 4.0, 1.2), INCA_STONE[0])  # a terrace step
    return p


def american_hall(seed=234):
    """A stepped pyramid with a twin shrine on top and a stair up the front."""
    p = Piece("SM_American_Hall", seed)
    z = 0.0
    for i, s in enumerate((12.0, 10.0, 8.0, 6.2)):
        p.box((0, 0, z + 0.9), (s, s, 1.8), STONE[i % 3], taper=0.94)
        z += 1.8
    for x in (-1.4, 1.4):
        p.box((x, 0.4, z + 1.2), (2.4, 3.2, 2.4), PLASTER[0])
        p.box((x, 0.4, z + 2.7), (2.6, 3.4, 0.6), PAINT[x > 0 and 1 or 0])
        p.box((x, -1.23, z + 1.0), (0.9, 0.08, 1.8), DARK)
    for q in p.box((0, -4.5, 3.6), (2.6, 4.5, 0.4), STONE[2]):
        q.co.z += (q.co.y + 4.5) * 1.55  # the stair
    return p


def american_palace(seed=235):
    """A terraced palace of fitted stone round a court, a sun temple at its head."""
    p = Piece("SM_American_Palace", seed)
    for i, (w, d) in enumerate(((22.0, 16.0), (19.0, 13.0))):
        p.box((0, 1.0, 0.9 + 1.8 * i), (w, d, 1.8), INCA_STONE[i % 2])
    for x, y, w, d in ((-6.5, 2.0, 5.0, 8.0), (6.5, 2.0, 5.0, 8.0), (0.0, 5.0, 8.0, 3.0)):
        p.box((x, y, 3.6 + 1.8), (w, d, 3.6), INCA_STONE[0], taper=0.95)
        p.hip((x, y, 7.2), w * 0.95, d * 0.95, 2.4, THATCH[0], overhang=0.35)
    p.cylinder((0, 0.5, 3.6), 2.6, 4.0, INCA_STONE[1], segments=14)  # the round sun temple
    p.cone((0, 0.5, 7.6), 3.0, 2.4, THATCH[1], segments=14)
    p.box((0, -1.95, 4.9), (1.0, 0.1, 2.0), DARK, taper=0.75)
    p.box((0, -5.5, 1.8 + 1.6), (3.0, 1.0, 3.2), INCA_STONE[0])  # the gate
    p.box((0, -6.02, 2.8), (1.2, 0.1, 2.2), DARK, taper=0.75)
    return p


PIECES = [middleeast_house_a, middleeast_house_b, middleeast_house_c, middleeast_hall, middleeast_palace,
          asian_house_a, asian_house_b, asian_house_c, asian_hall, asian_palace,
          african_house_a, african_house_b, african_house_c, african_hall, african_palace,
          american_house_a, american_house_b, american_house_c, american_hall, american_palace]
