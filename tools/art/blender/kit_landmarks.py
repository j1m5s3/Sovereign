"""Landmarks kit: map tokens for single world wonders, each modelled as itself (the Wonders kit draws the rest
by kind). A piece is named after its wonder's rules id in CamelCase (BUILDING_TEMPLE_OF_ARTEMIS:
SM_TempleOfArtemis); the map actor looks for it first and falls back to the wonder's kind. Footprints
about 9 m across and up to 10 m tall, like the Wonders kit.

It covers every wonder in the rules.
"""
import math

import kitlib
from kitlib import Piece
from kit_wonders import BRONZE, DARK, DOME, GOLD, IRON, LAWN, LEAF, MARBLE, PAVING, ROOF, SAND, STONE, WALL, WATER, _base, _hemi

ROSE = ["#c9876a", "#b9775c", "#d69a7d"]  # Petra's rose sandstone
MUD = ["#b48a5c", "#a57d52"]               # mud brick (Etemenanki)
GLAZE = "#2f5f9e"                          # glazed blue brick
CLAY = ["#9c7b5c", "#8c6d50"]              # terracotta
CEDAR = "#6b4a2e"
SLATE = "#5b6a6e"
TEAL = "#4e8f86"

kitlib.patterns({"stone": ROSE + [SLATE], "adobe": MUD, "rooftile": CLAY + [GLAZE], "wood": [CEDAR], "metal": [TEAL]})


def _mound(p, center, radius, color, squash=1.0, wobble=0.15):
    """A rounded hill or rock mass cut off at the ground (z = 0)."""
    for q in p.blob(center, radius, color, squash=squash, subdiv=2, wobble=wobble):
        q.co.z = max(q.co.z, 0.0)


def _colonnade(p, x0, x1, y, z, height, color=MARBLE[0], n=6, radius=0.22):
    """A row of columns along X at depth y, standing on z."""
    for i in range(n):
        x = x0 + (x1 - x0) * i / max(1, n - 1)
        p.cylinder((x, y, z), radius, height, color, segments=8)


def _temple(p, w, d, z, column_h, color=MARBLE[0], cols_front=6, cols_side=8, roof=MARBLE[1]):
    """A Greek temple on a stepped base at height z: a peristyle of columns, entablature and a low gable."""
    for k in range(3):
        p.box((0, 0, z + 0.15 + 0.3 * k), (w + 1.2 - 0.4 * k, d + 1.2 - 0.4 * k, 0.3), STONE[2] if k == 0 else MARBLE[1])
    top = z + 0.9
    p.box((0, 0, top + column_h / 2), (w - 1.4, d - 1.4, column_h), WALL[0])  # the cella behind the columns
    for y in (-d / 2 + 0.3, d / 2 - 0.3):
        _colonnade(p, -w / 2 + 0.3, w / 2 - 0.3, y, top, column_h, color, cols_front)
    for x in (-w / 2 + 0.3, w / 2 - 0.3):
        for i in range(1, cols_side - 1):
            y = -d / 2 + 0.3 + (d - 0.6) * i / (cols_side - 1)
            p.cylinder((x, y, top), 0.22, column_h, color, segments=8)
    p.box((0, 0, top + column_h + 0.25), (w, d, 0.5), MARBLE[1])
    p.gable((0, 0, top + column_h + 0.5), d, w, 0.9, roof, overhang=0.15, rot_z=math.pi / 2)
    return top + column_h + 1.4


def pyramids(seed=101):
    """The three pyramids of Giza in a row, the Sphinx before them."""
    p = Piece("SM_Pyramids", seed)
    _base(p, color=SAND[2])
    p.cone((-1.6, 1.4, 0.2), 3.4, 6.2, SAND[0], segments=4)
    p.cone((2.2, 2.4, 0.2), 2.4, 4.4, SAND[1], segments=4)
    p.cone((3.0, -1.0, 0.2), 1.3, 2.4, SAND[0], segments=4)
    # The Sphinx, couchant, facing -Y.
    p.box((-2.2, -3.0, 0.55), (0.9, 2.2, 0.7), SAND[1])
    p.box((-2.2, -3.95, 0.95), (0.7, 0.6, 0.8), SAND[1])
    p.box((-2.2, -3.75, 1.45), (1.0, 0.5, 0.3), SAND[2])  # the headdress
    for x in (-2.45, -1.95):
        p.box((x, -4.3, 0.35), (0.25, 0.9, 0.3), SAND[1])  # forepaws
    return p


def jebel_barkal(seed=102):
    """The flat-topped holy mountain with its pinnacle, steep Nubian pyramids at its foot."""
    p = Piece("SM_JebelBarkal", seed)
    _base(p, color=SAND[2])
    p.box((0, 2.0, 1.9), (7.6, 3.8, 3.4), ROSE[1], taper=0.88)
    p.box((0, 2.0, 3.75), (6.6, 3.2, 0.3), ROSE[2])
    p.cylinder((-3.2, 0.6, 0.2), 0.55, 4.6, ROSE[0], segments=7, radius_top=0.35)  # the pinnacle
    for k, x in enumerate((-2.4, -0.4, 1.6, 3.4)):
        h = 2.6 - 0.3 * k
        p.cone((x, -2.2, 0.2), 0.8, h, SAND[k % 2], segments=4)
        p.box((x, -3.1, 0.55), (0.6, 0.5, 0.7), SAND[1])  # each pyramid's little chapel
    p.box((0.8, -0.4, 0.6), (3.0, 1.4, 0.8), WALL[1])  # the temple of Amun
    return p


def great_bath(seed=103):
    """A sunken brick pool with steps at both ends, inside a colonnaded court of baked brick."""
    p = Piece("SM_GreatBath", seed)
    _base(p, color=MUD[1])
    p.box((0, 0, 0.35), (8.4, 6.8, 0.3), MUD[0])
    p.box((0, 0, 0.45), (5.0, 2.8, 0.15), WATER)
    for y in (-1.8, 1.8):
        for k in range(3):
            p.box((0, y + (0.25 * k if y > 0 else -0.25 * k), 0.55 + 0.12 * k), (1.4, 0.3, 0.12), MUD[1])
    for x in (-3.9, 3.9):
        for i in range(5):
            p.box((x, -3.0 + 1.5 * i, 1.4), (0.4, 0.4, 1.8), MUD[1])
    for y in (-3.1, 3.1):
        for i in range(6):
            p.box((-3.9 + 1.56 * i, y, 1.4), (0.4, 0.4, 1.8), MUD[1])
    for x, y, w, d in ((0, -3.1, 8.2, 0.6), (0, 3.1, 8.2, 0.6), (-3.9, 0, 0.6, 6.8), (3.9, 0, 0.6, 6.8)):
        p.box((x, y, 2.45), (w, d, 0.3), MUD[0])
    p.box((0, 3.6, 1.4), (3.0, 1.0, 2.6), MUD[0])  # the granary hall behind
    return p


def temple_of_artemis(seed=104):
    """A vast Ionic temple, a forest of columns two deep under a long roof."""
    p = Piece("SM_TempleOfArtemis", seed)
    _base(p, color=PAVING)
    top = _temple(p, 5.6, 8.0, 0.2, 3.6, cols_front=8, cols_side=10)
    _colonnade(p, -2.3, 2.3, -3.2, 1.1, 3.6, MARBLE[0], 6)  # the second row before the porch
    p.blob((0, -4.3, 0.5), 0.35, GOLD, subdiv=1, wobble=0.0)  # the altar's fire bowl
    p.box((0, -4.3, 0.3), (1.0, 0.8, 0.4), MARBLE[1])
    return p


def oracle(seed=105):
    """Apollo's temple on a terrace in the mountain's side, the round tholos below and the theatre above."""
    p = Piece("SM_Oracle", seed)
    _mound(p, (0, 2.6, 0.0), 4.6, STONE[1], squash=0.7, wobble=0.12)  # the mountainside behind
    _base(p, color=STONE[2])
    p.box((0, -0.2, 0.9), (6.0, 3.6, 1.4), STONE[0])  # the terrace wall
    _temple(p, 3.0, 4.2, 1.6, 2.2, cols_front=6, cols_side=7)
    # The tholos: a ring of columns round a drum.
    p.cylinder((-3.0, -3.2, 0.2), 1.3, 0.3, MARBLE[1], segments=16)
    p.cylinder((-3.0, -3.2, 0.5), 0.75, 1.6, WALL[0], segments=12)
    for i in range(10):
        a = 2 * math.pi * i / 10
        p.cylinder((-3.0 + 1.1 * math.cos(a), -3.2 + 1.1 * math.sin(a), 0.5), 0.12, 1.6, MARBLE[0], segments=6)
    p.cone((-3.0, -3.2, 2.1), 1.3, 0.6, ROOF[0], segments=16)
    # The theatre cut into the slope.
    p.loft([(2.4, 2.6, 1.6, 0.0, 3.4), (3.4, 3.2, 2.2, 0.0, 3.4)], STONE[2], segments=12, arc=(0.0, math.pi), two_sided=True)
    p.box((3.4, -2.8, 0.8), (1.0, 1.0, 1.2), MARBLE[1])  # a treasury
    return p


def great_lighthouse(seed=106):
    """The Pharos: a square tower, an octagon above it, a round lantern with its fire, on a harbour mole."""
    p = Piece("SM_GreatLighthouse", seed)
    _base(p, color=WATER)
    p.box((0, 0, 0.35), (5.2, 5.2, 0.5), STONE[2])
    p.box((0, 0, 2.6), (3.0, 3.0, 4.0), MARBLE[0], taper=0.85)
    p.box((0, 0, 4.7), (3.0, 3.0, 0.25), MARBLE[1])
    p.cylinder((0, 0, 4.8), 1.15, 2.4, MARBLE[0], segments=8, radius_top=0.95)
    p.cylinder((0, 0, 7.2), 1.1, 0.2, MARBLE[1], segments=8)
    for i in range(8):
        a = 2 * math.pi * i / 8
        p.cylinder((0.7 * math.cos(a), 0.7 * math.sin(a), 7.4), 0.08, 1.0, MARBLE[0], segments=5)
    p.blob((0, 0, 7.8), 0.45, "#e8a33a", subdiv=1, wobble=0.1)  # the fire
    p.cone((0, 0, 8.4), 0.85, 0.7, MARBLE[1], segments=8)
    p.segment((0, 0, 9.1), (0, 0, 9.9), 0.12, 0.08, BRONZE[0], segments=6)  # the statue on top
    for x in (-1.0, 1.0):
        p.box((x, -1.52, 2.0), (0.4, 0.06, 0.7), DARK)  # windows
    return p


def colossus(seed=107):
    """The bronze sun god striding the harbour mouth on two plinths, a torch raised, a ship below."""
    p = Piece("SM_Colossus", seed)
    _base(p, color=WATER)
    for x in (-2.4, 2.4):
        p.box((x, 0, 0.8), (2.2, 2.2, 1.2), STONE[0])
        p.box((x, 0, 1.5), (1.4, 1.4, 0.4), MARBLE[1])
    # Legs apart, one foot on each plinth.
    p.segment((-2.1, 0, 1.7), (-0.5, 0, 5.2), 0.34, 0.42, BRONZE[0], segments=8)
    p.segment((2.1, 0, 1.7), (0.5, 0, 5.2), 0.34, 0.42, BRONZE[0], segments=8)
    p.loft([(5.0, 0.8, 0.5), (6.2, 0.75, 0.5), (7.2, 0.62, 0.42)], BRONZE[1], segments=10)
    p.blob((0, 0, 7.75), 0.42, BRONZE[0], subdiv=2, wobble=0.0)
    for i in range(7):  # the sun crown's rays
        a = math.pi * (i + 0.5) / 7
        p.segment((0.42 * math.cos(a), 0, 7.75 + 0.42 * math.sin(a)), (0.85 * math.cos(a), 0, 7.75 + 0.85 * math.sin(a)), 0.05, 0.02, GOLD, segments=4, caps=False)
    p.segment((0.7, 0, 7.0), (1.2, 0, 9.0), 0.17, 0.14, BRONZE[0], segments=6)
    p.cone((1.22, 0, 9.0), 0.22, 0.5, GOLD, segments=6)
    p.segment((-0.7, 0, 7.0), (-0.9, 0, 5.6), 0.17, 0.14, BRONZE[0], segments=6)
    p.box((0, -0.4, 0.4), (2.6, 0.7, 0.4), CEDAR, taper=0.8)  # a galley passing between
    p.box((0, -0.4, 1.2), (0.06, 0.06, 1.2), CEDAR)
    p.team_color(True)
    p.box((0, -0.43, 1.35), (1.0, 0.03, 0.8), "#f0ece2")
    p.team_color(False)
    return p


def petra(seed=108):
    """The Treasury: a two-storey temple front carved into a rose cliff, a tholos in its upper storey."""
    p = Piece("SM_Petra", seed)
    p.box((0, 1.6, 4.2), (9.0, 5.6, 8.4), ROSE[1], taper=0.9)  # the cliff
    p.blob((-3.6, 1.0, 6.5), 2.2, ROSE[0], squash=1.4, wobble=0.2)
    p.blob((3.8, 1.4, 6.0), 2.0, ROSE[2], squash=1.5, wobble=0.2)
    p.box((0, -1.5, 0.1), (9.0, 2.8, 0.2), SAND[0])  # the gorge floor
    # The facade, standing out of the cliff face (y = -1.2).
    p.box((0, -1.25, 1.7), (5.0, 0.3, 3.0), ROSE[2])
    _colonnade(p, -2.2, 2.2, -1.55, 0.2, 2.8, ROSE[2], 6, radius=0.18)
    p.box((0, -1.5, 3.2), (5.4, 0.6, 0.4), ROSE[0])
    p.gable((0, -1.5, 3.4), 0.5, 3.0, 0.6, ROSE[0], overhang=0.05)
    for x in (-2.0, 2.0):  # the broken pediment's halves either side of the tholos
        p.box((x, -1.45, 4.6), (1.2, 0.5, 2.2), ROSE[2])
        p.box((x, -1.45, 5.85), (1.4, 0.6, 0.3), ROSE[0])
    p.cylinder((0, -1.6, 3.6), 0.75, 2.4, ROSE[2], segments=10)
    p.cone((0, -1.6, 6.0), 0.85, 0.6, ROSE[0], segments=10)
    p.blob((0, -1.6, 6.75), 0.22, ROSE[0], subdiv=1, wobble=0.0)  # the urn
    p.box((0, -1.42, 1.2), (0.9, 0.1, 1.8), DARK)  # the doorway
    return p


def colosseum(seed=109):
    """The Flavian amphitheatre: four tiers of arches, the outer wall broken down on one side."""
    p = Piece("SM_Colosseum", seed)
    _base(p)
    p.loft([(0.2, 4.2, 3.5), (2.6, 4.2, 3.5), (2.6, 3.1, 2.4), (0.6, 2.2, 1.5)], WALL[1], segments=28, cap_top=False)
    p.loft([(2.6, 4.2, 3.5, 0.0, 0.0), (4.6, 4.2, 3.5, 0.0, 0.0)], WALL[0], segments=28, arc=(-0.6, math.pi + 0.6), two_sided=True)
    p.box((0, 0, 0.7), (3.6, 2.2, 0.15), SAND[0])  # the arena floor
    for row, z in enumerate((0.9, 1.9, 3.0)):
        n = 22
        for i in range(n):
            a = 2 * math.pi * (i + 0.5) / n
            if row == 2 and not (-0.6 < a < math.pi + 0.6):
                continue  # the broken side has no top tier
            p.box((4.22 * math.cos(a), 3.52 * math.sin(a), z), (0.5, 0.25, 0.75), DARK, rot_z=a + math.pi / 2)
    return p


def great_library(seed=110):
    """The Library and Museum: a long colonnaded hall round a garden court, scrolls stacked in its wings."""
    p = Piece("SM_GreatLibrary", seed)
    _base(p)
    p.box((0, 1.8, 1.7), (8.0, 3.0, 3.0), WALL[0])  # the main hall
    p.box((0, 1.8, 3.35), (8.2, 3.2, 0.3), MARBLE[1])
    p.gable((0, 1.8, 3.5), 8.2, 3.2, 0.8, ROOF[0], overhang=0.1)
    _colonnade(p, -3.6, 3.6, 0.15, 0.2, 3.0, MARBLE[0], 9)
    for x in (-3.4, 3.4):  # the wings
        p.box((x, -1.6, 1.2), (1.6, 3.6, 2.0), WALL[1])
        p.gable((x, -1.6, 2.2), 3.6, 1.6, 0.6, ROOF[1], overhang=0.1, rot_z=math.pi / 2)
    p.box((0, -1.6, 0.3), (4.4, 3.4, 0.1), LAWN)  # the court
    for x in (-1.2, 1.2):
        p.blob((x, -1.8, 0.9), 0.6, LEAF[0], squash=1.2)
    p.box((0, -1.6, 0.6), (0.6, 0.6, 0.6), MARBLE[1])
    p.box((0, 0.28, 1.4), (1.4, 0.1, 2.4), DARK)  # the great door
    return p


def mahabodhi_temple(seed=111):
    """A tall pyramidal tower of many storeys on a broad plinth, four small towers at its corners, the Bodhi tree."""
    p = Piece("SM_MahabodhiTemple", seed)
    _base(p)
    p.box((0, 0, 0.7), (6.0, 6.0, 1.0), STONE[0])
    z = 1.2
    w = 3.4
    for k in range(9):
        p.box((0, 0, z + 0.45), (w, w, 0.9), STONE[k % 2])
        z += 0.9
        w *= 0.88
    p.cone((0, 0, z), w * 0.6, 0.8, STONE[2], segments=8)
    p.cylinder((0, 0, z + 0.8), 0.08, 0.6, GOLD, segments=5)
    for x in (-2.4, 2.4):
        for y in (-2.4, 2.4):
            p.box((x, y, 1.9), (0.8, 0.8, 1.4), STONE[1], taper=0.7)
            p.cone((x, y, 2.6), 0.45, 0.6, STONE[2], segments=8)
    p.box((0, -1.75, 1.7), (0.7, 0.1, 1.0), DARK)
    p.segment((3.2, 3.2, 0.2), (3.0, 3.1, 1.6), 0.18, 0.14, CEDAR, segments=6, caps=False)  # the Bodhi tree
    p.blob((3.0, 3.1, 2.3), 1.1, LEAF[2], squash=0.8)
    return p


def terracotta_army(seed=112):
    """Ranks of clay soldiers in their pit, horses and a chariot at the head."""
    p = Piece("SM_TerracottaArmy", seed)
    p.box((0, 0, 0.1), (9.0, 9.0, 0.2), SAND[1])
    p.box((0, 0, 0.15), (7.6, 7.0, 0.1), CLAY[1])  # the pit floor
    for x, w in ((-3.9, 0.4), (3.9, 0.4)):
        p.box((x, 0, 0.5), (w, 7.4, 0.8), SAND[0])
    for y, d in ((-3.6, 0.4), (3.6, 0.4)):
        p.box((0, y, 0.5), (8.2, d, 0.8), SAND[0])
    for col in range(-3, 4):
        x = col * 1.0
        if col < 3:
            p.box((x + 0.5, 0.5, 0.45), (0.25, 6.0, 0.6), SAND[2])  # earth walls between files
        for r in range(6):
            y = 2.6 - r * 0.9
            p.cylinder((x, y, 0.2), 0.16, 0.9, CLAY[r % 2], segments=6, radius_top=0.13)
            p.blob((x, y, 1.22), 0.13, CLAY[0], subdiv=1, wobble=0.0)
    p.box((0, -3.0, 0.7), (1.2, 0.7, 0.4), CLAY[1])  # the chariot
    for x in (-0.6, 0.6):
        p.disc((x, -3.0, 0.5), 0.3, 0.06, CEDAR, segments=10)
    for x in (-0.4, 0.4):
        p.box((x, -3.9, 0.85), (0.3, 0.9, 0.45), CLAY[0])
        p.segment((x, -4.3, 1.0), (x, -4.5, 1.35), 0.09, 0.07, CLAY[0], segments=5)
    return p


def machu_picchu(seed=113):
    """A town of grey stone and thatch on terraces along a saddle, the sugarloaf peak behind."""
    p = Piece("SM_MachuPicchu", seed)
    _mound(p, (2.6, 2.8, 0.0), 2.2, STONE[1], squash=3.0)  # Huayna Picchu
    _mound(p, (0, 0, 0.0), 4.6, LEAF[2], squash=0.32, wobble=0.1)
    z = 0.4
    for k, w in enumerate((8.4, 7.0, 5.6)):
        p.box((-0.6, -0.6 - 0.3 * k, z + 0.3), (w, w * 0.62, 0.6), STONE[k % 2])
        p.box((-0.6, -0.6 - 0.3 * k, z + 0.62), (w - 0.2, w * 0.62 - 0.2, 0.06), LAWN)
        z += 0.6
    for i, (x, y) in enumerate(((-2.6, -1.0), (-1.4, -1.6), (-0.2, -0.8), (1.0, -1.6), (-1.8, 0.2), (0.4, 0.4))):
        p.box((x, y, z + 0.45), (0.9, 0.7, 0.9), STONE[0])
        p.gable((x, y, z + 0.9), 0.9, 0.7, 0.55, "#b9a46a", overhang=0.1)
    p.box((-3.0, 0.8, z + 0.5), (1.0, 1.0, 1.0), STONE[2])  # the Intihuatana's rise
    p.box((-3.0, 0.8, z + 1.15), (0.3, 0.3, 0.3), STONE[0])
    return p


def statue_of_zeus(seed=114):
    """Zeus enthroned in gold and ivory inside his Doric temple, seen through the open front."""
    p = Piece("SM_StatueOfZeus", seed)
    _base(p, color=PAVING)
    for k in range(2):
        p.box((0, 0.4, 0.35 + 0.3 * k), (7.2 - 0.4 * k, 6.2 - 0.4 * k, 0.3), STONE[2] if k == 0 else MARBLE[1])
    for x in (-3.0, 3.0):  # the side walls
        p.box((x, 0.6, 2.6), (0.4, 5.0, 3.8), WALL[0])
    p.box((0, 3.0, 2.6), (6.4, 0.4, 3.8), WALL[0])
    _colonnade(p, -2.8, 2.8, -2.3, 0.8, 3.8, MARBLE[0], 6)
    p.box((0, 0.4, 4.75), (6.8, 6.0, 0.5), MARBLE[1])
    p.gable((0, 0.4, 5.0), 6.0, 6.8, 1.0, ROOF[0], overhang=0.15, rot_z=math.pi / 2)
    # The god on his throne, within.
    p.box((0, 1.6, 1.3), (2.0, 1.4, 1.0), CEDAR)
    p.box((0, 2.2, 2.4), (2.0, 0.3, 2.4), CEDAR)
    p.box((0, 1.4, 2.1), (1.2, 1.0, 0.6), GOLD)  # the robe over his knees
    p.loft([(2.4, 0.5, 0.35, 0.0, 1.8), (3.2, 0.55, 0.38, 0.0, 1.8), (3.7, 0.4, 0.3, 0.0, 1.8)], "#efe4c8", segments=10)
    p.blob((0, 1.8, 4.0), 0.3, "#efe4c8", subdiv=2, wobble=0.0)
    p.segment((0.6, 1.6, 3.3), (0.9, 1.0, 2.8), 0.1, 0.09, "#efe4c8", segments=5)
    p.segment((-0.6, 1.6, 3.3), (-0.8, 1.2, 4.2), 0.1, 0.09, "#efe4c8", segments=5)
    p.cylinder((-0.85, 1.2, 1.8), 0.05, 2.8, GOLD, segments=5)  # the sceptre
    return p


def apadana(seed=115):
    """The audience hall of Persepolis: a forest of slender columns with bull capitals on a terrace with stairs."""
    p = Piece("SM_Apadana", seed)
    p.box((0, 0.4, 0.6), (8.6, 8.0, 1.2), STONE[0])  # the terrace
    for x in (-2.2, 2.2):  # the double stair
        for q in p.box((x, -3.9, 0.6), (1.4, 1.2, 1.2), STONE[2]):
            if q.co.y < -3.9:
                q.co.z = min(q.co.z, 0.0)
    for i in range(6):
        for j in range(6):
            x, y = -3.0 + 1.2 * i, -2.2 + 1.1 * j
            p.cylinder((x, y, 1.2), 0.12, 4.0, STONE[2], segments=7)
            p.box((x, y, 5.3), (0.55, 0.22, 0.25), DARK)  # the bull capitals
    p.box((0, 0.55, 5.55), (7.6, 6.4, 0.3), CEDAR)  # the cedar roof
    for x in (-3.9, 3.9):
        for y in (-2.6, 3.4):
            p.box((x, y, 3.1), (0.9, 0.9, 3.8), MUD[0])  # the corner towers
    p.box((0, -3.25, 0.65), (6.0, 0.12, 0.7), GLAZE)  # the tribute reliefs along the stair
    return p


def mausoleum_at_halicarnassus(seed=116):
    """A tall podium, a ring of Ionic columns, a stepped pyramid roof and the four-horse chariot on top."""
    p = Piece("SM_MausoleumAtHalicarnassus", seed)
    _base(p)
    p.box((0, 0, 1.9), (5.2, 4.4, 3.4), MARBLE[1])  # the podium
    p.box((0, 0, 3.75), (5.6, 4.8, 0.3), MARBLE[0])
    for y in (-2.0, 2.0):
        _colonnade(p, -2.4, 2.4, y, 3.9, 2.2, MARBLE[0], 6, radius=0.16)
    for x in (-2.4, 2.4):
        for i in (1, 2, 3):
            p.cylinder((x, -2.0 + i, 3.9), 0.16, 2.2, MARBLE[0], segments=8)
    p.box((0, 0, 4.9), (3.8, 3.0, 2.0), WALL[0])
    p.box((0, 0, 6.25), (5.4, 4.6, 0.3), MARBLE[1])
    z, w, d = 6.4, 5.0, 4.2
    for k in range(6):  # the stepped pyramid
        p.box((0, 0, z + 0.2), (w, d, 0.4), MARBLE[k % 2])
        z += 0.4
        w -= 0.7
        d -= 0.6
    p.box((0, 0, z + 0.25), (1.4, 1.0, 0.5), BRONZE[0])  # the quadriga
    for x in (-0.45, -0.15, 0.15, 0.45):
        p.box((x, -0.7, z + 0.65), (0.18, 0.7, 0.35), BRONZE[1])
    p.segment((0, 0.2, z + 0.5), (0, 0.2, z + 1.2), 0.12, 0.1, BRONZE[0], segments=6)
    return p


def etemenanki(seed=117):
    """The ziggurat of Babylon: seven mud-brick terraces, a long stair up the front, a blue-glazed shrine on top."""
    p = Piece("SM_Etemenanki", seed)
    _base(p, color=SAND[2])
    z = 0.2
    w = 8.0
    for k in range(6):
        p.box((0, 0.4, z + 0.55), (w, w, 1.1), MUD[k % 2])
        z += 1.1
        w -= 1.0
    p.box((0, 0.4, z + 0.6), (1.8, 1.8, 1.2), GLAZE)
    p.box((0, 0.4, z + 1.3), (2.0, 2.0, 0.2), GOLD)
    for q in p.box((0, -3.7, 1.8), (1.2, 2.6, 0.3), MUD[0]):  # the great stair rising to the third terrace
        q.co.z += (q.co.y + 4.9) * 1.25
    for x in (-2.6, 2.6):
        for q in p.box((x, -3.3, 1.2), (0.8, 2.0, 0.3), MUD[1]):
            q.co.z += (q.co.y + 4.2) * 1.0
    return p


# ---------------------------------------------------------------- Medieval and Renaissance


def _stair(p, x, y0, y1, z0, z1, width, color):
    """A straight flight from (x, y0) at z0 up to (x, y1) at z1: a solid wedge from the ground up to its sloped top."""
    for q in p.box((x, (y0 + y1) / 2, 0.0), (width, abs(y1 - y0), 1.0), color):
        q.co.z = z0 + (z1 - z0) * (q.co.y - y0) / (y1 - y0) if q.co.z > 0 else z0 - 0.05


def hagia_sophia(seed=118):
    """A broad low dome on a square mass, half-domes falling away front and back, four minarets."""
    p = Piece("SM_HagiaSophia", seed)
    _base(p)
    p.box((0, 0, 1.6), (6.0, 6.4, 2.8), "#d9a88a")
    for y in (-2.2, 2.2):
        p.box((0, y, 3.1), (3.4, 1.6, 0.4), "#c99a7c")
        _hemi(p, 0, y, 3.3, 1.5, DOME[0], squash=0.55)
    p.cylinder((0, 0, 3.0), 2.1, 0.9, "#c99a7c", segments=20)
    _hemi(p, 0, 0, 3.9, 2.2, DOME[0], squash=0.5)
    p.cylinder((0, 0, 5.0), 0.05, 0.5, GOLD, segments=5)
    for x in (-3.7, 3.7):
        for y in (-3.9, 3.9):
            p.cylinder((x, y, 0.2), 0.28, 6.4, WALL[0], segments=8)
            p.cylinder((x, y, 4.6), 0.38, 0.12, WALL[1], segments=8)
            p.cone((x, y, 6.6), 0.3, 1.2, SLATE, segments=8)
    p.box((0, -3.25, 1.1), (1.2, 0.1, 1.8), DARK)
    return p


def alhambra(seed=119):
    """Red fortress walls and square towers on a hill, a pool court with arcades within."""
    p = Piece("SM_Alhambra", seed)
    _mound(p, (0, 0, 0.0), 4.7, LEAF[2], squash=0.25)
    red = ["#b5623f", "#a5573a"]
    s = 3.4
    for x, y, w, d in ((0, -s, 2 * s, 0.5), (0, s, 2 * s, 0.5), (-s, 0, 0.5, 2 * s), (s, 0, 0.5, 2 * s)):
        p.box((x, y, 2.0), (w, d, 1.8), red[0])
    for x, y, h in ((-s, -s, 3.6), (s, -s, 3.0), (-s, s, 3.0), (s, s, 3.2), (0, s, 4.4), (-1.2, -s, 3.4)):
        p.box((x, y, 1.1 + h / 2), (1.2, 1.2, h), red[1])
        p.box((x, y, 1.15 + h), (1.3, 1.3, 0.1), red[0])
    p.box((0, 0.2, 1.25), (4.6, 4.6, 0.1), PAVING)
    p.box((0, 0.2, 1.32), (1.0, 3.6, 0.06), WATER)  # the Court of the Myrtles' pool
    for x in (-1.0, 1.0):
        p.box((x, 0.2, 1.4), (0.3, 3.6, 0.12), LEAF[0])
    for y in (-2.0, 2.4):
        p.box((0, y, 2.0), (3.2, 0.6, 1.4), WALL[0])
        for i in range(6):
            p.box((-1.25 + 0.5 * i, y - 0.31, 1.75), (0.3, 0.04, 0.8), DARK)
    return p


def chichen_itza(seed=120):
    """El Castillo: nine steep terraces, a stair up each side, the temple on top."""
    p = Piece("SM_ChichenItza", seed)
    _base(p, color=LAWN)
    z, w = 0.2, 7.6
    for k in range(9):
        p.box((0, 0, z + 0.32), (w, w, 0.64), STONE[k % 2])
        z += 0.64
        w -= 0.6
    _stair(p, 0, -4.4, -1.4, 0.2, z, 1.3, STONE[2])
    _stair(p, 0, 4.4, 1.4, 0.2, z, 1.3, STONE[2])
    p.box((0, 0, z + 0.7), (2.0, 2.0, 1.4), WALL[1])
    p.box((0, 0, z + 1.5), (2.3, 2.3, 0.2), STONE[0])
    p.box((0, -1.02, z + 0.5), (0.8, 0.06, 1.0), DARK)
    return p


def mont_st_michel(seed=121):
    """A tidal island: a rocky mount ringed by walls, houses climbing to the abbey and its spire."""
    p = Piece("SM_MontStMichel", seed)
    _base(p, color="#c8b98f")
    _mound(p, (0, 0.4, 0.2), 3.6, STONE[1], squash=0.9, wobble=0.15)
    p.cylinder((0, 0.4, 0.2), 3.9, 0.8, STONE[0], segments=16)
    for i in range(8):
        a = 2 * math.pi * i / 8 + 0.3
        x, y = 3.0 * math.cos(a), 0.4 + 3.0 * math.sin(a)
        p.box((x, y, 1.6), (0.8, 0.7, 0.9), WALL[i % 2])
        p.gable((x, y, 2.05), 0.8, 0.7, 0.4, SLATE, overhang=0.05, rot_z=a)
    p.box((0, 0.6, 3.9), (3.0, 1.4, 1.6), STONE[2])
    p.gable((0, 0.6, 4.7), 3.0, 1.4, 0.7, SLATE, overhang=0.1)
    p.box((0, 0.6, 5.6), (0.8, 0.8, 1.4), STONE[2])
    p.cone((0, 0.6, 6.3), 0.5, 2.6, SLATE, segments=8)
    p.cylinder((0, 0.6, 8.9), 0.05, 0.5, GOLD, segments=5)
    return p


def meenakshi_temple(seed=122):
    """Tall tapering gopuram gateways painted in many colours round a walled court and its golden shrine."""
    p = Piece("SM_MeenakshiTemple", seed)
    _base(p)
    paint = ["#c96f4a", "#d9a94a", "#5d8fb0", "#7aa05a"]
    for x, y, w, d in ((0, -3.6, 7.6, 0.4), (0, 3.6, 7.6, 0.4), (-3.6, 0, 0.4, 7.6), (3.6, 0, 0.4, 7.6)):
        p.box((x, y, 0.9), (w, d, 1.4), WALL[0])
    for k, (x, y, h, rot) in enumerate(((0, -3.6, 7.4, 0.0), (0, 3.6, 6.0, 0.0), (-3.6, 0, 5.2, math.pi / 2), (3.6, 0, 5.6, math.pi / 2))):
        z, w = 0.2, 2.6
        for i in range(int(h / 0.7)):
            p.box((x, y, z + 0.35), (w, w * 0.6, 0.7), paint[(i + k) % 4], rot_z=rot)
            z += 0.7
            w *= 0.9
        p.box((x, y, z + 0.25), (w * 1.1, 0.4, 0.5), paint[k % 4], rot_z=rot)
    p.box((0, 0.6, 1.1), (2.0, 2.0, 1.8), WALL[1])
    p.cone((0, 0.6, 2.0), 1.1, 1.6, GOLD, segments=4)
    p.box((-1.8, -1.2, 0.3), (1.6, 1.6, 0.1), WATER)  # the tank of the golden lotus
    return p


def university_of_sankore(seed=123):
    """A Sahelian mosque of mud brick: a stepped minaret bristling with wooden beams, a walled court."""
    p = Piece("SM_UniversityOfSankore", seed)
    _base(p, color=SAND[0])
    p.box((0, 0.8, 1.2), (7.0, 5.0, 2.0), MUD[0])
    for x in (-3.5, -1.75, 0, 1.75, 3.5):
        p.cone((x, -1.7, 2.2), 0.25, 0.7, MUD[1], segments=4)  # the parapet's pinnacles
    p.box((0, -2.5, 0.8), (7.0, 0.3, 1.2), MUD[1])  # the court's wall
    z, w = 0.2, 2.6
    for k in range(5):
        p.box((2.4, 1.6, z + 0.6), (w, w, 1.2), MUD[k % 2], taper=0.9)
        for d in (-1, 1):
            for e in (-0.3, 0.3):
                p.segment((2.4 + d * w * 0.45, 1.6 + e * w, z + 0.6), (2.4 + d * (w * 0.5 + 0.35), 1.6 + e * w, z + 0.6), 0.04, 0.04, CEDAR,
                          segments=4, caps=False)
        z += 1.2
        w *= 0.82
    p.cone((2.4, 1.6, z), w * 0.6, 0.6, MUD[0], segments=4)
    p.box((-1.6, 0.8, 2.3), (1.4, 1.4, 0.4), MUD[1])
    p.box((-1.0, -1.7, 0.9), (0.9, 0.1, 1.2), DARK)
    return p


def huey_teocalli(seed=124):
    """The Templo Mayor: four terraces, a double stair, twin shrines on top, red and blue."""
    p = Piece("SM_HueyTeocalli", seed)
    _base(p, color=STONE[2])
    z, w = 0.2, 8.0
    for k in range(4):
        p.box((0, 0.4, z + 0.7), (w, w * 0.9, 1.4), WALL[k % 2])
        z += 1.4
        w -= 1.4
    for x in (-0.9, 0.9):
        _stair(p, x, -4.4, -1.3, 0.2, z, 1.4, STONE[0])
    for x, roof in ((-1.0, "#a33b2c"), (1.0, "#3f6fa8")):
        p.box((x, 0.4, z + 0.6), (1.6, 1.8, 1.2), WALL[0])
        p.box((x, 0.4, z + 1.45), (1.8, 2.0, 0.5), roof)
        p.box((x, -0.52, z + 0.45), (0.6, 0.06, 0.8), DARK)
    return p


def kotoku_in(seed=125):
    """The great bronze Buddha seated in meditation on a lotus, in an open temple yard with pines."""
    p = Piece("SM_KotokuIn", seed)
    _base(p, color="#c4b896")
    p.box((0, 0.6, 0.5), (4.6, 4.0, 0.6), STONE[0])
    p.cylinder((0, 0.6, 0.8), 1.9, 0.5, TEAL, segments=16)  # the lotus
    p.loft([(1.3, 1.9, 1.5, 0.0, 0.6), (1.8, 1.8, 1.4, 0.0, 0.6), (2.0, 1.3, 1.0, 0.0, 0.6)], TEAL, segments=14)  # crossed legs
    p.loft([(1.9, 1.1, 0.8, 0.0, 0.8), (3.2, 1.2, 0.85, 0.0, 0.8), (4.3, 0.95, 0.7, 0.0, 0.8), (4.7, 0.5, 0.45, 0.0, 0.8)], TEAL, segments=14)
    p.blob((0, 0.7, 5.3), 0.72, TEAL, squash=1.1, subdiv=2, wobble=0.0)
    p.blob((0, 0.75, 6.0), 0.35, TEAL, subdiv=1, wobble=0.0)  # the ushnisha
    p.box((0, -0.1, 2.3), (0.9, 0.5, 0.3), TEAL)  # hands in the lap
    for x, y in ((-3.6, -3.0), (3.6, -2.4), (-3.4, 3.4), (3.8, 3.2)):
        p.segment((x, y, 0.2), (x + 0.2, y, 2.2), 0.14, 0.1, CEDAR, segments=6, caps=False)
        p.blob((x + 0.2, y, 2.6), 0.9, LEAF[2], squash=0.5)
    return p


def angkor_wat(seed=126):
    """Five lotus-bud towers in a quincunx over galleried terraces, the moat before."""
    p = Piece("SM_AngkorWat", seed)
    p.box((0, 0, 0.1), (9.0, 9.0, 0.2), WATER)
    p.box((0, 0.3, 0.35), (7.8, 7.6, 0.5), STONE[2])
    p.box((0, -3.9, 0.25), (1.0, 1.4, 0.3), STONE[0])  # the causeway
    z = 0.6
    for k, w in enumerate((7.0, 5.4, 3.8)):
        p.box((0, 0.3, z + 0.5), (w, w, 1.0), STONE[k % 2])
        p.box((0, 0.3, z + 1.05), (w + 0.2, w + 0.2, 0.1), STONE[2])
        z += 1.0

    def bud(x, y, h, r):
        p.loft([(z, r, r, x, y), (z + h * 0.35, r * 1.1, r * 1.1, x, y), (z + h * 0.7, r * 0.7, r * 0.7, x, y), (z + h, 0.05, 0.05, x, y)], STONE[0], segments=8)

    bud(0, 0.3, 4.6, 0.9)
    for x in (-1.5, 1.5):
        for y in (-1.2, 1.8):
            bud(x, y, 3.0, 0.6)
    return p


def venetian_arsenal(seed=127):
    """Brick shipyard sheds either side of a basin, a galley on the slip, the water gate between its towers."""
    p = Piece("SM_VenetianArsenal", seed)
    _base(p, color="#8d877c")
    p.box((0, -0.6, 0.25), (4.2, 5.0, 0.1), WATER)
    brick = ["#a65f43", "#97563c"]
    for x in (-3.3, 3.3):
        p.box((x, 0.4, 1.3), (2.0, 7.0, 2.2), brick[0])
        for i in range(4):
            p.gable((x, -2.2 + 1.75 * i, 2.4), 1.6, 2.0, 0.7, ROOF[1], overhang=0.05, rot_z=math.pi / 2)
        face = x - math.copysign(1.02, x)  # the slip openings face the basin
        for i in range(5):
            p.box((face, -2.4 + 1.4 * i, 1.1), (0.06, 0.8, 1.4), DARK)
    for x in (-1.6, 1.6):
        p.box((x, -3.6, 2.0), (0.9, 0.9, 3.6), brick[1])
        for dx in (-0.3, 0.3):
            p.box((x + dx, -3.6, 4.0), (0.25, 0.9, 0.4), brick[1])
    p.box((0, -3.6, 3.2), (2.4, 0.5, 0.5), brick[0])
    p.box((0, 1.0, 0.6), (1.0, 3.6, 0.6), CEDAR, taper=0.7)  # a galley on the slip
    p.cylinder((0, 1.0, 0.9), 0.06, 2.0, CEDAR, segments=5)
    return p


def great_zimbabwe(seed=128):
    """The Great Enclosure: a curving dry-stone wall round a conical tower, the hill ruins behind."""
    p = Piece("SM_GreatZimbabwe", seed)
    _base(p, color=LAWN)
    _mound(p, (2.6, 3.2, 0.0), 2.4, STONE[1], squash=0.9)
    p.loft([(0.2, 3.8, 3.2, -0.6, -0.4), (2.4, 3.6, 3.0, -0.6, -0.4)], STONE[0], segments=24, arc=(0.0, 2 * math.pi - 0.35), two_sided=True)
    p.loft([(0.2, 2.6, 2.0, -0.4, -0.6), (1.8, 2.5, 1.9, -0.4, -0.6)], STONE[2], segments=18, arc=(0.5, 2 * math.pi - 0.2), two_sided=True)
    p.cylinder((-1.0, -0.2, 0.2), 0.8, 3.4, STONE[0], segments=12, radius_top=0.55)
    for i in range(6):  # the chevron course along the top
        a = 0.3 + i * 0.25
        p.box((-0.6 + 3.65 * math.cos(a), -0.4 + 3.05 * math.sin(a), 2.5), (0.5, 0.2, 0.2), STONE[2], rot_z=a + math.pi / 2)
    p.box((3.0, 3.0, 2.4), (1.6, 0.5, 1.0), STONE[0])
    return p


def forbidden_city(seed=129):
    """The Hall of Supreme Harmony: double yellow roofs on a red hall over white marble terraces, a walled court."""
    p = Piece("SM_ForbiddenCity", seed)
    _base(p, color=PAVING)
    red = "#a8382c"
    yellow = ["#d9a93a", "#c99a30"]
    for x, y, w, d in ((0, -4.1, 8.6, 0.4), (0, 4.1, 8.6, 0.4), (-4.1, 0, 0.4, 8.6), (4.1, 0, 0.4, 8.6)):
        p.box((x, y, 0.9), (w, d, 1.4), red)
    for k in range(3):
        p.box((0, 0.8, 0.4 + 0.3 * k), (6.4 - 0.6 * k, 4.6 - 0.5 * k, 0.3), MARBLE[k % 2])
    p.box((0, 0.8, 2.1), (4.4, 2.6, 1.6), red)
    p.hip((0, 0.8, 2.9), 5.4, 3.4, 0.6, yellow[0], overhang=0.3)
    p.box((0, 0.8, 3.4), (3.6, 2.0, 0.6), red)
    p.hip((0, 0.8, 3.7), 4.6, 2.8, 1.0, yellow[1], overhang=0.3)
    p.box((0, -3.1, 1.6), (2.4, 1.6, 1.2), red)  # the gate hall
    p.hip((0, -3.1, 2.2), 3.0, 2.0, 0.6, yellow[0], overhang=0.2)
    return p


def kilwa_kisiwani(seed=130):
    """The Husuni Kubwa: a coral-stone palace above the sea, its great mosque under rows of small domes, a dhow."""
    p = Piece("SM_KilwaKisiwani", seed)
    _base(p, color=WATER)
    coral = ["#ddd2bb", "#cfc2a7"]
    p.box((0, 1.0, 0.6), (8.4, 6.8, 1.0), SAND[0])
    p.box((-1.6, 1.6, 1.7), (4.4, 3.6, 1.2), coral[0])
    for i in range(4):
        for j in range(3):
            _hemi(p, -3.1 + i * 1.0, 0.6 + j * 1.0, 2.3, 0.42, coral[1])
    p.box((2.6, 1.6, 2.2), (2.2, 3.2, 2.2), coral[1])
    p.box((2.6, 1.6, 3.4), (2.4, 3.4, 0.2), coral[0])
    p.box((2.6, -0.1, 1.2), (1.0, 0.1, 1.2), DARK)
    p.box((0, -2.8, 0.35), (1.6, 0.6, 0.3), CEDAR, taper=0.7)
    p.segment((0, -2.8, 0.5), (0.2, -2.8, 2.4), 0.04, 0.03, CEDAR, segments=5, caps=False)
    p.team_color(True)
    p.slab([(-0.9, 0.7), (0.3, 2.3), (0.5, 0.7)], 0.03, "#efe7d4", plane="XZ", offset=-2.8)  # the lateen sail
    p.team_color(False)
    return p


def orsz_gh_z(seed=131):
    """The Parliament on the Danube: a long neo-Gothic front of spires, a red ribbed dome at its centre."""
    p = Piece("SM_OrszGhZ", seed)
    _base(p, color=WATER)
    p.box((0, 1.2, 0.4), (9.0, 5.6, 0.6), STONE[2])
    p.box((0, 1.4, 2.0), (8.4, 3.2, 2.6), WALL[0])
    p.box((0, 1.4, 3.6), (8.6, 3.4, 0.6), "#a33b2c")
    p.cylinder((0, 1.4, 3.3), 1.3, 1.6, WALL[1], segments=16)
    _hemi(p, 0, 1.4, 4.9, 1.3, "#a33b2c", squash=1.3)
    p.cone((0, 1.4, 6.5), 0.3, 1.8, WALL[1], segments=8)
    for x in (-3.9, -2.6, -1.3, 1.3, 2.6, 3.9):
        p.cylinder((x, -0.25, 0.7), 0.16, 3.6, WALL[1], segments=6)
        p.cone((x, -0.25, 4.3), 0.2, 0.9, SLATE, segments=6)
    for x in (-2.0, 2.0):
        p.box((x, 1.4, 4.4), (0.8, 0.8, 1.6), WALL[0])
        p.cone((x, 1.4, 5.2), 0.5, 1.6, SLATE, segments=4)
    for i in range(12):
        p.box((-3.6 + 0.65 * i, -0.22, 2.0), (0.22, 0.06, 1.4), DARK)
    return p


# ---------------------------------------------------------------- Renaissance to Information


def casa_de_contratacion(seed=132):
    """Seville's trading house: a long arcaded front round a patio, a corner tower, a galleon at the quay."""
    p = Piece("SM_CasaDeContrataciN", seed)
    _base(p, color=PAVING)
    p.box((0, -3.6, 0.15), (9.0, 1.8, 0.1), WATER)
    p.box((0, 1.2, 1.5), (7.6, 5.0, 2.6), WALL[0])
    p.box((0, 1.2, 2.9), (7.8, 5.2, 0.2), ROOF[0])
    for row, z in enumerate((0.9, 2.1)):
        for i in range(9):
            p.box((-3.4 + 0.85 * i, -1.32, z), (0.45, 0.06, 0.8), DARK)
    p.box((3.2, 1.2, 3.3), (1.4, 1.4, 1.4), WALL[1])
    p.hip((3.2, 1.2, 4.0), 1.6, 1.6, 0.8, ROOF[1], overhang=0.1)
    p.box((-1.8, -3.6, 0.45), (2.6, 0.9, 0.6), CEDAR, taper=0.75)  # a galleon
    for x in (-2.6, -1.8, -1.0):
        p.cylinder((x, -3.6, 0.7), 0.05, 2.2 if x == -1.8 else 1.6, CEDAR, segments=5)
    p.team_color(True)
    for x, h in ((-2.6, 1.0), (-1.8, 1.4), (-1.0, 1.0)):
        p.box((x, -3.6, 1.0 + h * 0.6), (0.06, 0.9, h * 0.7), "#efe7d4")
    p.team_color(False)
    return p


def panama_canal(seed=133):
    """Lock chambers stepping down between concrete walls, a gate and a ship in the lower lock."""
    p = Piece("SM_PanamaCanal", seed)
    _mound(p, (0, 0, 0.0), 4.8, LEAF[2], squash=0.3)
    concrete = ["#b7b3aa", "#a8a49b"]
    for k, (y, z) in enumerate(((-2.8, 0.3), (0.0, 0.9), (2.8, 1.5))):
        p.box((0, y, z / 2 + 0.05), (3.6, 2.8, z + 0.1), concrete[k % 2])
        p.box((0, y, z + 0.08), (2.2, 2.6, 0.06), WATER)
        p.box((0, y + 1.4, z + 0.5), (2.2, 0.2, 1.0), IRON[1])  # the gates
    for x in (-1.5, 1.5):
        p.box((x, 0, 1.2), (0.6, 8.6, 2.4), concrete[0])
    p.box((0, -2.8, 0.75), (1.0, 2.0, 0.6), "#7a3b2c", taper=0.85)  # a steamer in the lower lock
    p.box((0, -2.6, 1.3), (0.6, 0.7, 0.5), WALL[0])
    p.cylinder((0, -2.3, 1.5), 0.15, 0.6, DARK, segments=6)
    p.box((2.6, -1.0, 1.9), (0.8, 0.8, 1.4), WALL[0])  # the control house
    return p


def st_basil_s_cathedral(seed=134):
    """Nine chapels crowned with colourful onion domes on tall drums round a central tented spire."""
    p = Piece("SM_StBasilSCathedral", seed)
    _base(p, color=PAVING)
    brick = "#a5503a"
    p.box((0, 0, 1.2), (6.0, 6.0, 2.0), brick)
    p.cylinder((0, 0, 2.2), 1.0, 3.6, brick, segments=8)
    p.cone((0, 0, 5.8), 1.1, 2.6, "#d9b04a", segments=8)
    p.cylinder((0, 0, 8.4), 0.05, 0.6, GOLD, segments=5)
    domes = ["#3f8f5a", "#c9473a", "#3f6fa8", "#d9a93a", "#7a3f8f", "#d97a3a", "#3fa8a0", "#c93a6a"]
    for i in range(8):
        a = 2 * math.pi * i / 8
        r = 2.4 if i % 2 == 0 else 2.0
        x, y = r * math.cos(a), r * math.sin(a)
        h = 2.6 if i % 2 == 0 else 1.8
        p.cylinder((x, y, 2.2), 0.55, h, brick if i % 2 else WALL[0], segments=8)
        z = 2.2 + h
        p.loft([(z, 0.55, 0.55, x, y), (z + 0.4, 0.75, 0.75, x, y), (z + 0.9, 0.5, 0.5, x, y), (z + 1.4, 0.04, 0.04, x, y)], domes[i], segments=10, folds=6,
               fold_depth=0.12)
    return p


def taj_mahal(seed=135):
    """A white marble tomb on a plinth: a tall onion dome, four chhatris, four minarets, the long pool before it."""
    p = Piece("SM_TajMahal", seed)
    _base(p, color=LAWN)
    white = ["#f2efe8", "#e6e2d8"]
    p.box((0, 0.8, 0.5), (6.6, 6.6, 0.6), white[1])
    p.box((0, -3.0, 0.25), (0.9, 3.0, 0.1), WATER)
    p.box((0, 0.8, 2.0), (3.8, 3.8, 2.4), white[0])
    for x in (-1.0, 1.0):
        p.box((x * 0.0, -1.12, 1.9), (1.2, 0.1, 1.8), DARK)
    p.cylinder((0, 0.8, 3.2), 1.25, 0.9, white[0], segments=16)
    p.loft([(4.1, 1.25, 1.25, 0.0, 0.8), (4.8, 1.55, 1.55, 0.0, 0.8), (5.7, 1.2, 1.2, 0.0, 0.8), (6.5, 0.15, 0.15, 0.0, 0.8)], white[0], segments=16)
    p.cylinder((0, 0.8, 6.5), 0.05, 0.6, GOLD, segments=5)
    for x in (-1.5, 1.5):
        for y in (-0.7, 2.3):
            p.cylinder((x, y, 3.2), 0.4, 0.5, white[1], segments=8)
            _hemi(p, x, y, 3.7, 0.42, white[0], squash=1.1)
    for x in (-3.0, 3.0):
        for y in (-2.2, 3.8):
            p.cylinder((x, y, 0.8), 0.22, 4.4, white[0], segments=8)
            _hemi(p, x, y, 5.2, 0.28, white[0])
    return p


def torre_de_bel_m(seed=136):
    """The Belem Tower: a Manueline bastion in the river, a tall square tower with turreted corners above."""
    p = Piece("SM_TorreDeBelM", seed)
    _base(p, color=WATER)
    lime = ["#e3dccb", "#d6cdb8"]
    p.box((0, -1.0, 0.7), (4.6, 3.6, 1.2), lime[1])
    p.loft([(0.2, 2.3, 1.6, 0.0, -2.8), (1.3, 2.3, 1.6, 0.0, -2.8)], lime[1], segments=12, arc=(math.pi, 2 * math.pi))
    for i in range(7):  # the bastion's crenellations
        p.box((-1.8 + 0.6 * i, -2.8, 1.45), (0.3, 0.3, 0.3), lime[0])
    p.box((0, 1.0, 3.4), (2.6, 2.6, 6.0), lime[0])
    for x in (-1.3, 1.3):
        for y in (-0.3, 2.3):
            p.cylinder((x, y, 6.2), 0.25, 0.8, lime[1], segments=8)
            _hemi(p, x, y, 7.0, 0.27, lime[1], squash=1.4)
    p.box((0, 1.0, 6.5), (2.8, 2.8, 0.2), lime[1])
    for i in range(3):
        p.box((-0.7 + 0.7 * i, -0.32, 4.2), (0.35, 0.06, 0.8), DARK)
    return p


def potala_palace(seed=137):
    """The White and Red Palaces climbing a hillside in sloping walls, rows of windows, gold roofs on top."""
    p = Piece("SM_PotalaPalace", seed)
    _mound(p, (0, 1.4, 0.0), 4.6, STONE[1], squash=0.55)
    white = ["#f0ece2", "#e3ddd0"]
    red = "#8e2f2a"
    p.box((0, -0.6, 1.6), (8.2, 3.2, 3.0), white[0], taper=0.92)
    p.box((0, 1.0, 3.6), (5.6, 3.2, 3.0), white[1], taper=0.92)
    p.box((0, 1.4, 5.6), (2.8, 2.4, 2.0), red, taper=0.94)
    p.box((0, 1.4, 6.65), (3.0, 2.6, 0.12), "#5a2a24")
    for x in (-0.8, 0.8):
        p.hip((x, 1.4, 6.7), 0.9, 0.9, 0.5, GOLD, overhang=0.1)
    for row, (y, z, w) in enumerate(((-2.22, 1.0, 7.4), (-2.22, 2.2, 7.0), (-0.62, 3.2, 5.0), (-0.62, 4.3, 4.6))):
        n = int(w / 0.6)
        for i in range(n):
            p.box((-w / 2 + 0.3 + 0.6 * i, y, z), (0.22, 0.06, 0.35), DARK)
    return p


def ruhr_valley(seed=138):
    """A coal mine's headframe over brick works, smoking chimneys and a blast furnace."""
    p = Piece("SM_RuhrValley", seed)
    _base(p, color="#6e6a62")
    brick = ["#8e4c38", "#7e4432"]
    p.box((-1.8, 1.2, 1.4), (4.6, 3.6, 2.4), brick[0])
    for i in range(3):
        p.gable((-3.3 + 1.5 * i, 1.2, 2.6), 3.6, 1.5, 0.7, SLATE, overhang=0.05, rot_z=math.pi / 2)
    for x, h in ((-3.2, 6.4), (-0.6, 5.4)):
        p.cylinder((x, 3.4, 0.2), 0.4, h, brick[1], segments=8, radius_top=0.28)
        p.blob((x + 0.3, 3.4, h + 0.6), 0.5, "#9a9894", squash=0.7)
    for x in (1.8, 3.6):  # the headframe's legs
        p.segment((x, -2.6, 0.2), (2.7, -1.6, 5.6), 0.12, 0.08, "#4e7a5a", segments=5, caps=False)
        p.segment((x, -0.6, 0.2), (2.7, -1.6, 5.6), 0.12, 0.08, "#4e7a5a", segments=5, caps=False)
    p.disc((2.7, -1.6, 5.6), 0.6, 0.12, IRON[0], segments=12)
    p.cylinder((2.4, 2.4, 0.2), 0.9, 3.2, IRON[0], segments=10, radius_top=0.6)  # the blast furnace
    p.box((0, -3.4, 0.35), (3.0, 0.8, 0.3), DARK)  # a coal heap
    return p


def bolshoi_theatre(seed=139):
    """A neoclassical theatre: an eight-column portico under a pediment, Apollo's quadriga on top."""
    p = Piece("SM_BolshoiTheatre", seed)
    _base(p, color=PAVING)
    cream = ["#e8dcc0", "#dccfb0"]
    p.box((0, 1.2, 2.4), (6.4, 6.0, 4.4), cream[0])
    p.gable((0, 1.2, 4.6), 6.4, 6.0, 1.0, "#6b7a76", overhang=0.1, rot_z=math.pi / 2)
    p.box((0, -2.4, 0.5), (5.4, 1.4, 0.6), cream[1])
    _colonnade(p, -2.3, 2.3, -2.6, 0.8, 3.4, WALL[0], 8, radius=0.2)
    p.box((0, -2.3, 4.4), (5.6, 1.6, 0.4), cream[1])
    p.gable((0, -2.3, 4.6), 1.6, 5.6, 0.9, cream[1], overhang=0.05)
    p.box((0, -2.6, 5.6), (1.0, 0.6, 0.4), BRONZE[0])  # the quadriga
    for x in (-0.35, -0.12, 0.12, 0.35):
        p.box((x, -3.0, 5.95), (0.14, 0.5, 0.3), BRONZE[1])
    for i in range(5):
        p.box((-2.0 + i, -1.82, 2.3), (0.5, 0.06, 1.6), DARK)
    return p


def oxford_university(seed=140):
    """Colleges round green quads: the Radcliffe Camera's drum and dome, spires and a gatehouse tower."""
    p = Piece("SM_OxfordUniversity", seed)
    _base(p, color=LAWN)
    stone = ["#d6c49a", "#c9b68a"]
    for x, y, w, d in ((0, 3.4, 8.4, 1.6), (-3.4, 0, 1.6, 5.2), (3.4, 0, 1.6, 5.2)):
        p.box((x, y, 1.3), (w, d, 2.2), stone[0])
        p.gable((x, y, 2.4), w if d < w else d, d if d < w else w, 0.8, SLATE, overhang=0.05, rot_z=0.0 if d < w else math.pi / 2)
    p.cylinder((0, -0.4, 0.2), 1.8, 2.6, stone[1], segments=16)  # the Radcliffe Camera
    p.cylinder((0, -0.4, 2.8), 1.4, 0.8, stone[0], segments=16)
    _hemi(p, 0, -0.4, 3.6, 1.4, "#6b7a76", squash=1.0)
    p.cylinder((0, -0.4, 5.0), 0.25, 0.6, stone[0], segments=8)
    for x in (-3.4, 3.4):
        p.box((x, 3.4, 3.0), (0.9, 0.9, 3.6), stone[1])
        p.cone((x, 3.4, 4.8), 0.6, 2.2, stone[1], segments=8)
    p.box((0, 3.4, 3.4), (1.4, 1.4, 1.8), stone[1])  # Tom Tower
    p.cone((0, 3.4, 4.3), 0.75, 1.4, "#6b7a76", segments=8)
    return p


def statue_of_liberty(seed=141):
    """Liberty in green copper on her star-shaped fort and tall pedestal, the torch held high."""
    p = Piece("SM_StatueOfLiberty", seed)
    _base(p, color=WATER)
    copper = ["#6fa595", "#5f9585"]
    for i in range(11):  # the star fort
        a = 2 * math.pi * i / 11
        p.box((1.8 * math.cos(a), 1.8 * math.sin(a), 0.5), (1.6, 1.0, 0.6), STONE[0], rot_z=a)
    p.box((0, 0, 0.5), (3.2, 3.2, 0.6), STONE[0])
    p.box((0, 0, 1.9), (1.8, 1.8, 2.2), STONE[2], taper=0.85)
    p.box((0, 0, 3.15), (1.9, 1.9, 0.3), STONE[1])
    p.loft([(3.3, 0.55, 0.45), (4.6, 0.62, 0.5), (6.0, 0.55, 0.42), (6.9, 0.45, 0.35)], copper[0], segments=12, folds=7, fold_depth=0.06)
    p.blob((0, 0, 7.3), 0.3, copper[0], subdiv=2, wobble=0.0)
    for i in range(7):  # the crown's rays
        a = math.pi * (i + 0.5) / 7
        p.segment((0.3 * math.cos(a), -0.05, 7.4 + 0.15 * math.sin(a)), (0.6 * math.cos(a), -0.1, 7.5 + 0.35 * math.sin(a)), 0.03, 0.01, copper[1], segments=4,
                  caps=False)
    p.segment((0.45, 0, 6.7), (0.6, 0, 8.6), 0.12, 0.1, copper[0], segments=6)
    p.cylinder((0.6, 0, 8.6), 0.14, 0.3, copper[1], segments=6)
    p.blob((0.6, 0, 9.05), 0.18, GOLD, subdiv=1, wobble=0.0)  # the flame
    p.box((-0.4, -0.35, 5.6), (0.3, 0.12, 0.45), copper[1])  # the tablet
    return p


def big_ben(seed=142):
    """The Palace of Westminster along the river, the clock tower at its end with four faces and a spire."""
    p = Piece("SM_BigBen", seed)
    _base(p, color=WATER)
    stone = ["#d0bb8a", "#c2ad7c"]
    p.box((-0.8, 1.0, 0.4), (7.4, 5.0, 0.4), PAVING)
    p.box((-1.4, 1.4, 1.6), (5.8, 2.4, 2.2), stone[0])
    for i in range(9):
        p.cone((-4.0 + 0.65 * i, 0.2, 2.7), 0.12, 0.7, stone[1], segments=4)
    p.box((-3.6, 2.4, 3.0), (1.4, 1.4, 4.6), stone[1])  # the Victoria Tower
    p.box((2.8, 0.6, 3.4), (1.3, 1.3, 6.4), stone[0])
    p.box((2.8, 0.6, 7.0), (1.5, 1.5, 1.0), stone[1])
    p.box((2.8, -0.16, 7.0), (0.9, 0.04, 0.9), "#efe7d4")  # the clock faces
    p.box((2.8, 1.36, 7.0), (0.9, 0.04, 0.9), "#efe7d4")
    p.box((2.04, 0.6, 7.0), (0.04, 0.9, 0.9), "#efe7d4")
    p.box((3.56, 0.6, 7.0), (0.04, 0.9, 0.9), "#efe7d4")
    p.cone((2.8, 0.6, 7.5), 0.85, 2.0, SLATE, segments=4)
    p.cylinder((2.8, 0.6, 9.5), 0.05, 0.6, GOLD, segments=5)
    return p


def hermitage(seed=143):
    """The Winter Palace: a long green-and-white baroque front, its roofline crowded with statues, on the river."""
    p = Piece("SM_Hermitage", seed)
    _base(p, color=WATER)
    green = "#5f9e86"
    p.box((0, 1.0, 0.35), (9.0, 6.0, 0.3), PAVING)
    p.box((0, 1.2, 1.9), (8.6, 4.0, 2.8), green)
    p.box((0, 1.2, 3.4), (8.8, 4.2, 0.2), WALL[0])
    for row, z in enumerate((1.2, 2.4)):
        for i in range(14):
            x = -4.0 + 0.62 * i
            p.box((x, -0.82, z), (0.08, 0.06, 1.0), WALL[0])  # white columns between the windows
            p.box((x + 0.31, -0.82, z), (0.24, 0.06, 0.6), DARK)
    for i in range(15):
        p.cylinder((-4.2 + 0.6 * i, -0.7, 3.5), 0.07, 0.5, WALL[0], segments=5)
        p.blob((-4.2 + 0.6 * i, -0.7, 4.05), 0.08, WALL[0], subdiv=1, wobble=0.0)
    p.box((0, -0.85, 2.0), (1.4, 0.2, 3.0), WALL[0])
    p.gable((0, -0.85, 3.5), 0.4, 1.6, 0.5, green, overhang=0.05)
    return p


def eiffel_tower(seed=144):
    """Puddled iron: four arched legs to the first platform, a tapering shaft to the second and the top."""
    p = Piece("SM_EiffelTower", seed)
    _base(p, 8.0, 8.0, LAWN)
    brown = ["#6e5a48", "#5e4c3c"]
    for x in (-1, 1):
        for y in (-1, 1):
            p.segment((x * 3.0, y * 3.0, 0.2), (x * 1.5, y * 1.5, 2.6), 0.32, 0.24, brown[0], segments=5, caps=False)
            p.segment((x * 1.5, y * 1.5, 2.6), (x * 0.65, y * 0.65, 5.4), 0.22, 0.16, brown[0], segments=5, caps=False)
            p.segment((x * 0.65, y * 0.65, 5.4), (x * 0.12, y * 0.12, 9.6), 0.14, 0.06, brown[0], segments=5, caps=False)
    for x in (-1, 1):  # the arches between the legs
        p.loft([(1.6, 2.2, 0.08, 0, x * 2.2), (2.6, 1.7, 0.08, 0, x * 1.7)], brown[1], segments=12, arc=(math.pi, 2 * math.pi), two_sided=True)
    p.box((0, 0, 2.7), (3.6, 3.6, 0.3), brown[1])
    p.box((0, 0, 5.5), (1.7, 1.7, 0.25), brown[1])
    p.box((0, 0, 9.0), (0.5, 0.5, 0.3), brown[1])
    p.cylinder((0, 0, 9.6), 0.05, 0.7, brown[0], segments=5)
    return p


def broadway(seed=145):
    """A Times Square block: theatres with lit marquees and billboards under taller towers."""
    p = Piece("SM_Broadway", seed)
    _base(p, color="#5e5c58")
    lights = ["#e8c84a", "#d94a5a", "#4ab0d9", "#e88a3a"]
    for k, (x, y, w, d, h) in enumerate(((-3.0, 1.6, 2.6, 3.4, 4.2), (0.0, 2.0, 2.8, 3.0, 6.8), (3.0, 1.4, 2.6, 3.6, 5.2), (-1.8, -2.4, 2.4, 2.4, 3.2),
                                         (2.2, -2.4, 2.6, 2.4, 3.6))):
        p.box((x, y, 0.2 + h / 2), (w, d, h), WALL[k % 2] if k % 2 else "#8a8478")
        for i in range(int(h / 0.6)):
            p.box((x, y - d / 2 - 0.02, 0.6 + 0.6 * i), (w * 0.8, 0.04, 0.25), DARK)
        p.box((x, y - d / 2 - 0.3, 1.4), (w * 0.9, 0.6, 0.35), lights[k % 4])  # the marquee
        p.box((x + w * 0.25, y - d / 2 - 0.05, h * 0.75), (w * 0.4, 0.06, h * 0.25), lights[(k + 1) % 4])  # a billboard
    p.box((0, -0.3, 0.25), (8.8, 0.8, 0.06), "#3a3936")  # the street
    return p


def cristo_redentor(seed=146):
    """Christ the Redeemer, arms spread wide, on the green summit of Corcovado."""
    p = Piece("SM_CristoRedentor", seed)
    _mound(p, (0, 0, 0.0), 4.4, LEAF[1], squash=0.55, wobble=0.15)
    _mound(p, (0.3, 0.2, 0.0), 1.8, STONE[1], squash=1.9, wobble=0.1)
    soap = ["#e3ddd0", "#d6cfc0"]
    p.box((0.3, 0.2, 3.7), (1.0, 1.0, 0.6), soap[1])  # the pedestal
    p.loft([(4.0, 0.42, 0.3, 0.3, 0.2), (5.4, 0.36, 0.26, 0.3, 0.2), (6.6, 0.4, 0.28, 0.3, 0.2), (7.0, 0.3, 0.22, 0.3, 0.2)], soap[0], segments=10, folds=5,
           fold_depth=0.05)
    p.segment((-1.9, 0.2, 6.75), (2.5, 0.2, 6.75), 0.13, 0.13, soap[0], segments=6)  # the arms
    p.blob((0.3, 0.2, 7.3), 0.24, soap[0], subdiv=2, wobble=0.0)
    return p


def golden_gate_bridge(seed=147):
    """International-orange towers and catenary cables over the strait, the deck between rocky headlands."""
    p = Piece("SM_GoldenGateBridge", seed)
    _base(p, color=WATER)
    orange = ["#c0482c", "#b0402a"]
    _mound(p, (-4.4, 0, 0.0), 1.6, STONE[1], squash=0.7)
    _mound(p, (4.4, 0, 0.0), 1.6, LEAF[2], squash=0.7)
    p.box((0, 0, 1.6), (9.0, 0.9, 0.2), orange[1])  # the deck
    for x in (-2.6, 2.6):
        for y in (-0.4, 0.4):
            p.box((x, y, 3.9), (0.25, 0.22, 7.4), orange[0])
        for z in (3.0, 5.0, 7.2):
            p.box((x, 0, z), (0.25, 0.9, 0.3), orange[0])
    for y in (-0.4, 0.4):
        pts = []
        for i in range(17):
            x = -4.4 + 8.8 * i / 16
            if abs(x) <= 2.6:
                z = 7.4 - 5.4 * (1 - (x / 2.6) ** 2)
            else:
                z = 7.4 - 5.2 * (abs(x) - 2.6) / 1.8
            pts.append((x, y, max(z, 1.7)))
        for a, b in zip(pts, pts[1:]):
            p.segment(a, b, 0.05, 0.05, orange[0], segments=4, caps=False)
    return p


def amundsen_scott_research_station(seed=148):
    """The elevated station on stilts over the snow, the old geodesic dome beside it, flags round the pole."""
    p = Piece("SM_AmundsenScottResearchStation", seed)
    _base(p, color="#eef2f4")
    p.box((0.6, 1.2, 2.2), (6.4, 2.6, 1.6), "#9aa4a8")
    p.box((0.6, 1.2, 3.1), (6.6, 2.8, 0.2), "#7d878b")
    for x in (-2.2, 0.6, 3.4):
        for y in (0.3, 2.1):
            p.cylinder((x, y, 0.2), 0.12, 1.3, IRON[0], segments=6)
    for i in range(10):
        p.box((-2.4 + 0.6 * i, -0.12, 2.3), (0.3, 0.06, 0.4), DARK)
    _hemi(p, -2.4, -2.4, 0.2, 1.6, "#c8ccd0", squash=0.9)
    p.cylinder((2.4, -2.4, 0.2), 0.05, 1.2, "#c9a43c", segments=6)  # the ceremonial pole
    p.blob((2.4, -2.4, 1.45), 0.18, "#d9dde0", subdiv=2, wobble=0.0)
    p.team_color(True)
    for i in range(6):
        a = 2 * math.pi * i / 6
        p.cylinder((2.4 + 0.9 * math.cos(a), -2.4 + 0.9 * math.sin(a), 0.2), 0.03, 1.0, "#efe7d4", segments=4)
        p.box((2.4 + 0.9 * math.cos(a) + 0.15, -2.4 + 0.9 * math.sin(a), 1.05), (0.3, 0.02, 0.18), "#efe7d4")
    p.team_color(False)
    return p


def est_dio_do_maracan(seed=149):
    """The Maracana: a great oval bowl under a white ring roof, the green pitch inside."""
    p = Piece("SM_EstDioDoMaracan", seed)
    _base(p, color=PAVING)
    p.loft([(0.2, 4.3, 3.6), (2.0, 4.3, 3.6), (2.0, 3.4, 2.7), (0.5, 2.4, 1.7)], "#b7b3aa", segments=28, cap_top=False)
    p.box((0, 0, 0.55), (3.2, 2.0, 0.1), LAWN)
    p.box((0, 0, 0.62), (0.04, 2.0, 0.03), "#f2f2f2")  # the halfway line
    p.loft([(2.0, 4.4, 3.7), (2.4, 4.4, 3.7), (2.6, 3.3, 2.6)], "#eceae4", segments=28, cap_bottom=False, cap_top=False)  # the ring roof
    for i in range(14):
        a = 2 * math.pi * i / 14
        p.box((4.35 * math.cos(a), 3.65 * math.sin(a), 1.1), (0.3, 0.3, 1.8), "#a8a49b", rot_z=a)
    return p


def biosph_re(seed=150):
    """The Montreal Biosphere: a great geodesic sphere of steel struts, mostly above a low ring, among trees."""
    p = Piece("SM_BiosphRe", seed)
    _base(p, color=LAWN)
    p.cylinder((0, 0, 0.2), 3.2, 0.5, "#b7b3aa", segments=20)
    v = p.blob((0, 0, 3.4), 3.6, "#c8d4d8", squash=1.0, subdiv=2, wobble=0.0)
    for q in v:
        q.co.z = max(q.co.z, 0.6)
    p.blob((0, 0, 3.4), 3.2, "#7fa0aa", squash=1.0, subdiv=1, wobble=0.0)  # the glazed inner shell
    for x, y in ((-3.9, -3.6), (3.8, -3.4), (-3.7, 3.8), (3.9, 3.6)):
        p.blob((x, y, 1.0), 0.8, LEAF[0], squash=1.2)
    return p


def sydney_opera_house(seed=151):
    """White shell sails in three rows on a granite podium jutting into the harbour."""
    p = Piece("SM_SydneyOperaHouse", seed)
    _base(p, color=WATER)
    p.box((0, 0.6, 0.45), (6.0, 7.4, 0.7), "#b59a7a")
    white = ["#f4f2ec", "#e6e3da"]

    def shell(x, y, h, w, length):
        # A sail: two curved side profiles that lean together into a ridge, its point thrust forward (-Y).
        profile = [(y + length * (1 - t), 0.8 + h * math.sin(math.pi / 2 * t) ** 0.8) for t in (0.0, 0.25, 0.5, 0.75, 1.0)]
        profile = [(y + length, 0.8)] + profile[1:] + [(y - 0.15 * length, 0.8 + h * 0.98), (y - 0.05 * length, 0.8 + h * 0.6), (y, 0.8)]
        for side in (-1, 1):
            for q in p.slab(profile, 0.08, white[0], plane="YZ", offset=0.0):
                q.co.x += x + side * w * (1.0 - (q.co.z - 0.8) / h) * 0.5

    for x in (-1.4, 1.4):
        for y, h, w in ((-3.0, 3.0, 2.0), (-1.5, 2.6, 1.9), (-0.1, 2.1, 1.7), (1.1, 1.6, 1.4)):
            shell(x, y, h, w, 1.9)
    p.box((0, 3.4, 1.3), (1.2, 1.0, 1.0), white[1])
    for y, h in ((2.6, 1.3), (3.4, 0.9)):
        shell(0, y, h, 1.0, 1.0)
    return p


PIECES = [pyramids, jebel_barkal, great_bath, temple_of_artemis, oracle, great_lighthouse, colossus, petra, colosseum, great_library,
          mahabodhi_temple, terracotta_army, machu_picchu, statue_of_zeus, apadana, mausoleum_at_halicarnassus, etemenanki,
          hagia_sophia, alhambra, chichen_itza, mont_st_michel, meenakshi_temple, university_of_sankore, huey_teocalli, kotoku_in,
          angkor_wat, venetian_arsenal, great_zimbabwe, forbidden_city, kilwa_kisiwani, orsz_gh_z,
          casa_de_contratacion, panama_canal, st_basil_s_cathedral, taj_mahal, torre_de_bel_m, potala_palace, ruhr_valley, bolshoi_theatre,
          oxford_university, statue_of_liberty, big_ben, hermitage, eiffel_tower, broadway, cristo_redentor, golden_gate_bridge,
          amundsen_scott_research_station, est_dio_do_maracan, biosph_re, sydney_opera_house]
