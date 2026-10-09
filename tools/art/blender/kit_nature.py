"""Nature kit: trees, bushes and rocks shared by every civ and era (world doc, The model sets)."""
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


PIECES = [tree_broadleaf, tree_conifer, bush, rocks]
