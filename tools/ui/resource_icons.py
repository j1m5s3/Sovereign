"""Resource icons (plan F): one pictogram per resource on a round badge coloured by its class, as Civ VI floats over
each plot: green for bonus, violet for luxury, rust for strategic. Drawn by script with the UI icon set's shapes
(icons.py), 64 px PNG, to unreal/Content/Slate/Resources/<name>.png (the id without RESOURCE_, lower case).

  blender -b --python tools/ui/resource_icons.py -- unreal/Content/Slate/Resources

The class comes from data/rules/resources.json, so a mod's or a new resource's badge follows the rules; a resource
with no pictogram here gets its class badge with its initial.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from icons import Icon, arc, circle, ellipse, line, poly, rect, save_png  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
BADGE = {"BONUS": ("#3d6b2c", "#8fbf5a"), "LUXURY": ("#5a3478", "#b48ad6"), "STRATEGIC": ("#7a3a1c", "#e08a4a")}

# Palette.
WHEAT, STRAW, BROWN, DARK, WHITE, GREY, IRON = "#e6c25a", "#c9a24a", "#7a4f2c", "#2a1d14", "#f2ecdc", "#9a9a9a", "#6f7478"
GREEN, LEAF, RED, ORANGE, YELLOW, BLUE, PINK = "#5aa83c", "#3f8a2e", "#c03a2a", "#e88a2a", "#f0d040", "#4a8ad0", "#e0a0b0"


def badge(cls):
    """The round badge every resource icon stands on."""
    fill, rim = BADGE.get(cls, BADGE["BONUS"])
    i = Icon()
    i.paint(circle(0.5, 0.5, 0.48), rim).paint(circle(0.5, 0.5, 0.42), fill)
    return i


# ------------------------------------------------------------------------------------------- food and animals

def wheat(i):
    i.paint(line(0.5, 0.86, 0.5, 0.3, 0.05), STRAW)
    for k in range(4):
        y = 0.34 + k * 0.12
        i.paint(ellipse(0.41, y, 0.08, 0.045, -0.6), WHEAT).paint(ellipse(0.59, y, 0.08, 0.045, 0.6), WHEAT)
    i.paint(ellipse(0.5, 0.24, 0.05, 0.08), WHEAT)


def rice(i):
    i.paint(ellipse(0.5, 0.66, 0.3, 0.14), WHITE).paint(rect(0.5, 0.58, 0.6, 0.06), BROWN)  # bowl of grain
    i.paint(poly([(0.2, 0.62), (0.8, 0.62), (0.68, 0.82), (0.32, 0.82)]), BROWN)
    for x in (0.38, 0.5, 0.62):
        i.paint(ellipse(x, 0.5, 0.05, 0.03), WHITE)
    i.paint(line(0.6, 0.45, 0.78, 0.2, 0.03), STRAW).paint(line(0.66, 0.46, 0.84, 0.24, 0.03), STRAW)  # chopsticks


def maize(i):
    i.paint(ellipse(0.5, 0.5, 0.13, 0.3), YELLOW)
    for y in (0.32, 0.42, 0.52, 0.62):
        i.paint(rect(0.5, y, 0.22, 0.02), STRAW)
    i.paint(poly([(0.37, 0.55), (0.25, 0.85), (0.47, 0.72)]), GREEN).paint(poly([(0.63, 0.55), (0.75, 0.85), (0.53, 0.72)]), GREEN)


def cattle(i):
    i.paint(ellipse(0.5, 0.56, 0.24, 0.2), BROWN)  # head
    i.paint(ellipse(0.5, 0.7, 0.14, 0.09), PINK)  # muzzle
    i.paint(arc(0.28, 0.36, 0.1, 0.05, math.pi * 0.9, math.pi * 1.6), WHITE).paint(arc(0.72, 0.36, 0.1, 0.05, math.pi * 1.4, math.pi * 0.1), WHITE)
    i.paint(circle(0.41, 0.5, 0.035), DARK).paint(circle(0.59, 0.5, 0.035), DARK)


def sheep(i):
    for (x, y) in [(0.36, 0.48), (0.5, 0.42), (0.64, 0.48), (0.4, 0.6), (0.6, 0.6), (0.5, 0.56)]:
        i.paint(circle(x, y, 0.12), WHITE)
    i.paint(ellipse(0.26, 0.5, 0.08, 0.1), DARK)  # face
    i.paint(rect(0.42, 0.76, 0.04, 0.12), DARK).paint(rect(0.58, 0.76, 0.04, 0.12), DARK)


def horses(i):
    i.paint(poly([(0.3, 0.82), (0.36, 0.48), (0.48, 0.3), (0.62, 0.22), (0.76, 0.34), (0.72, 0.42), (0.6, 0.4), (0.56, 0.58),
                  (0.66, 0.82)]), BROWN)
    i.paint(poly([(0.48, 0.3), (0.4, 0.3), (0.34, 0.48), (0.42, 0.5)]), DARK)  # mane
    i.paint(circle(0.63, 0.3, 0.025), DARK)


def deer(i):
    i.paint(ellipse(0.5, 0.62, 0.14, 0.18), BROWN).paint(ellipse(0.5, 0.78, 0.07, 0.05), DARK)
    for s in (-1, 1):
        x0 = 0.5 + s * 0.08
        i.paint(line(x0, 0.46, x0 + s * 0.16, 0.2, 0.04), STRAW).paint(line(x0 + s * 0.08, 0.33, x0 + s * 0.24, 0.3, 0.035), STRAW)
        i.paint(ellipse(0.5 + s * 0.17, 0.5, 0.07, 0.035, s * 0.4), BROWN)


def fish(i):
    i.paint(ellipse(0.46, 0.5, 0.26, 0.14), "#7fb0d0").paint(poly([(0.68, 0.5), (0.86, 0.34), (0.86, 0.66)]), "#7fb0d0")
    i.paint(circle(0.32, 0.47, 0.03), DARK).paint(arc(0.46, 0.5, 0.1, 0.025, -0.6, 0.6), "#4a7fa0")


def crabs(i):
    i.paint(ellipse(0.5, 0.58, 0.22, 0.14), RED)
    for s in (-1, 1):
        i.paint(circle(0.5 + s * 0.26, 0.34, 0.08, ), RED).erase(circle(0.5 + s * 0.3, 0.3, 0.04))
        i.paint(line(0.5 + s * 0.14, 0.5, 0.5 + s * 0.24, 0.4, 0.04), RED)
        for k in range(3):
            i.paint(line(0.5 + s * 0.16, 0.62 + k * 0.04, 0.5 + s * 0.3, 0.68 + k * 0.06, 0.03), RED)


def whales(i):
    i.paint(ellipse(0.44, 0.56, 0.3, 0.14), "#4a6f8a").paint(poly([(0.7, 0.52), (0.88, 0.36), (0.84, 0.5), (0.9, 0.62)]), "#4a6f8a")
    i.paint(ellipse(0.44, 0.64, 0.24, 0.05), "#c8d8e0").paint(circle(0.24, 0.52, 0.025), DARK)
    i.paint(line(0.34, 0.4, 0.3, 0.24, 0.03), "#bfe0f0").paint(line(0.34, 0.4, 0.4, 0.26, 0.03), "#bfe0f0")


def turtles(i):
    i.paint(ellipse(0.5, 0.52, 0.22, 0.17), "#3f7a4a").paint(ellipse(0.5, 0.52, 0.14, 0.1), "#6aa060")
    i.paint(circle(0.5, 0.28, 0.07), "#8aa060")
    for (x, y) in [(0.3, 0.38), (0.7, 0.38), (0.32, 0.68), (0.68, 0.68)]:
        i.paint(ellipse(x, y, 0.06, 0.04), "#8aa060")


def bananas(i):
    for k in range(3):  # a hand of three, curving up from the stalk
        x = 0.36 + k * 0.12
        i.paint(arc(x + 0.22, 0.36, 0.3, 0.09, math.pi * 0.62, math.pi * 1.02), YELLOW)
        i.paint(arc(x + 0.22, 0.36, 0.3, 0.025, math.pi * 0.62, math.pi * 0.8), "#c8a020")
    i.paint(rect(0.62, 0.24, 0.06, 0.1, 0.3), BROWN)


def citrus(i):
    i.paint(circle(0.42, 0.56, 0.2), ORANGE).paint(circle(0.62, 0.5, 0.15), YELLOW)
    i.paint(ellipse(0.48, 0.3, 0.1, 0.05, -0.5), LEAF)


def olives(i):
    i.paint(line(0.22, 0.3, 0.8, 0.62, 0.035), BROWN)
    for (x, y, a) in [(0.32, 0.26, -0.3), (0.5, 0.33, 0.5), (0.66, 0.46, -0.2)]:
        i.paint(ellipse(x, y, 0.09, 0.035, a), LEAF)
    for (x, y) in [(0.4, 0.48), (0.58, 0.6), (0.72, 0.7)]:
        i.paint(ellipse(x, y, 0.06, 0.08), "#4a5a2a")


def honey(i):
    for (x, y) in [(0.38, 0.4), (0.62, 0.4), (0.5, 0.6), (0.26, 0.6), (0.74, 0.6)]:
        pts = [(x + 0.12 * math.cos(a), y + 0.12 * math.sin(a)) for a in [math.pi / 6 + k * math.pi / 3 for k in range(6)]]
        i.paint(poly(pts), "#e8a820")
        i.paint(poly([(x + 0.08 * math.cos(a), y + 0.08 * math.sin(a)) for a in [math.pi / 6 + k * math.pi / 3 for k in range(6)]]), "#f6c84a")


def truffles(i):
    i.paint(circle(0.4, 0.56, 0.16), "#8a6448").paint(circle(0.62, 0.6, 0.12), "#a07a58")
    for (x, y) in [(0.36, 0.5), (0.44, 0.6), (0.6, 0.56)]:
        i.paint(circle(x, y, 0.025), "#4a3020")


# ------------------------------------------------------------------------------------------- stone and metal

def stone(i):
    i.paint(poly([(0.2, 0.74), (0.3, 0.46), (0.5, 0.38), (0.62, 0.5), (0.56, 0.74)]), GREY)
    i.paint(poly([(0.48, 0.74), (0.6, 0.44), (0.78, 0.48), (0.82, 0.74)]), "#b4b0a8")


def marble(i):
    """A white column: capital, fluted shaft and base."""
    i.paint(rect(0.5, 0.28, 0.4, 0.08), WHITE).paint(rect(0.5, 0.76, 0.42, 0.08), WHITE)
    i.paint(rect(0.5, 0.52, 0.26, 0.42), "#e8e4dc")
    for x in (0.42, 0.5, 0.58):
        i.paint(rect(x, 0.52, 0.025, 0.38), "#b8b4ac")


def copper(i):
    i.paint(poly([(0.24, 0.66), (0.36, 0.42), (0.64, 0.42), (0.76, 0.66)]), "#c8743a")
    i.paint(poly([(0.36, 0.42), (0.42, 0.32), (0.58, 0.32), (0.64, 0.42)]), "#e09a5a")


def iron(i):
    i.paint(poly([(0.22, 0.62), (0.34, 0.4), (0.66, 0.4), (0.78, 0.62)]), IRON)
    i.paint(poly([(0.34, 0.4), (0.4, 0.3), (0.6, 0.3), (0.66, 0.4)]), "#a0a4a8")
    i.paint(rect(0.5, 0.72, 0.5, 0.06), "#4a4e52")


def silver(i):
    i.paint(poly([(0.24, 0.66), (0.36, 0.42), (0.64, 0.42), (0.76, 0.66)]), "#c8ccd4")
    i.paint(poly([(0.36, 0.42), (0.42, 0.32), (0.58, 0.32), (0.64, 0.42)]), "#eef0f4")
    i.paint(line(0.66, 0.24, 0.72, 0.3, 0.03), WHITE)


def coal(i):
    for (x, y, r) in [(0.38, 0.6, 0.15), (0.6, 0.62, 0.13), (0.5, 0.42, 0.13)]:
        i.paint(circle(x, y, r), "#2a2a2e").paint(circle(x - r * 0.3, y - r * 0.3, r * 0.3), "#4a4a52")


def oil(i):
    i.paint(poly([(0.5, 0.18), (0.72, 0.56), (0.5, 0.82), (0.28, 0.56)]), "#1e1a1a")
    i.paint(circle(0.5, 0.6, 0.22), "#1e1a1a").paint(ellipse(0.42, 0.56, 0.05, 0.09), "#5a5050")


def niter(i):
    i.paint(poly([(0.26, 0.76), (0.5, 0.32), (0.74, 0.76)]), WHITE)
    for (x, y) in [(0.44, 0.58), (0.56, 0.66), (0.5, 0.48)]:
        i.paint(circle(x, y, 0.03), "#c8c0b0")
    i.paint(circle(0.68, 0.32, 0.06), "#e8e0d0")


def aluminum(i):
    i.paint(poly([(0.22, 0.62), (0.4, 0.38), (0.78, 0.38), (0.6, 0.62)]), "#d4dce4")
    i.paint(poly([(0.22, 0.62), (0.6, 0.62), (0.6, 0.72), (0.22, 0.72)]), "#a8b0b8")
    i.paint(poly([(0.6, 0.62), (0.78, 0.38), (0.78, 0.48), (0.6, 0.72)]), "#8a929a")


def uranium(i):
    i.paint(circle(0.5, 0.5, 0.06), "#c8f060")
    for k in range(3):
        a = math.pi / 2 + k * 2 * math.pi / 3
        i.paint(ellipse(0.5 + 0.2 * math.cos(a), 0.5 - 0.2 * math.sin(a), 0.11, 0.07, -a), "#c8f060")
    i.paint(arc(0.5, 0.5, 0.3, 0.04, 0, 2 * math.pi - 0.01), "#c8f060")


def mercury(i):
    for (x, y, r) in [(0.4, 0.58, 0.14), (0.62, 0.52, 0.1), (0.56, 0.72, 0.07)]:
        i.paint(circle(x, y, r), "#c8ccd0").paint(circle(x - r * 0.35, y - r * 0.35, r * 0.3), WHITE)
    i.paint(line(0.5, 0.2, 0.5, 0.36, 0.04), "#c8ccd0")


def gypsum(i):
    for (x, a) in [(0.38, -0.25), (0.5, 0.0), (0.62, 0.25)]:
        i.paint(poly([(x - 0.06, 0.76), (x, 0.28), (x + 0.06, 0.76)]), "#f0ece4")
    i.paint(rect(0.5, 0.78, 0.44, 0.06), "#c8c0b0")


def salt(i):
    i.paint(poly([(0.22, 0.76), (0.5, 0.36), (0.78, 0.76)]), WHITE)
    for k in range(3):
        i.paint(rect(0.34 + k * 0.16, 0.24 + (k % 2) * 0.06, 0.07, 0.07, 0.785), "#f4f0e8")


def jade(i):
    i.paint(arc(0.5, 0.5, 0.2, 0.12, 0, 2 * math.pi - 0.01), "#3fa070")
    i.paint(arc(0.5, 0.5, 0.2, 0.04, math.pi * 1.1, math.pi * 1.5), "#8ad0a8")


def amber(i):
    i.paint(ellipse(0.5, 0.52, 0.24, 0.2), "#d8862a").paint(ellipse(0.46, 0.46, 0.1, 0.07), "#f2b85a")
    i.paint(line(0.52, 0.56, 0.6, 0.6, 0.03), "#5a3a1a")


def diamonds(i):
    i.paint(poly([(0.28, 0.42), (0.38, 0.28), (0.62, 0.28), (0.72, 0.42), (0.5, 0.8)]), "#bfe8f4")
    i.paint(poly([(0.28, 0.42), (0.72, 0.42), (0.5, 0.8)]), "#8ad0e8")
    i.paint(line(0.38, 0.28, 0.44, 0.42, 0.02), WHITE).paint(line(0.62, 0.28, 0.56, 0.42, 0.02), WHITE)


def pearls(i):
    i.paint(ellipse(0.5, 0.62, 0.3, 0.14), "#c8b8a8").paint(ellipse(0.5, 0.58, 0.24, 0.08), "#e8dcd0")
    i.paint(circle(0.5, 0.5, 0.1), WHITE).paint(circle(0.47, 0.47, 0.03), "#ffffff")


def ivory(i):
    i.paint(arc(0.66, 0.36, 0.34, 0.1, math.pi * 0.62, math.pi * 1.0), "#f2ead6")
    i.paint(arc(0.66, 0.36, 0.34, 0.03, math.pi * 0.62, math.pi * 0.8), "#d8ccb0")


# ------------------------------------------------------------------------------------------- plants and goods

def leaf_pair(i, x, y, c=LEAF):
    i.paint(ellipse(x - 0.08, y, 0.1, 0.05, 0.6), c).paint(ellipse(x + 0.08, y, 0.1, 0.05, -0.6), c)


def coffee(i):
    for (x, y) in [(0.4, 0.5), (0.6, 0.6)]:
        i.paint(ellipse(x, y, 0.13, 0.17, 0.4), "#5a3220").paint(line(x - 0.03, y - 0.13, x + 0.03, y + 0.13, 0.025), "#2a160c")


def tea(i):
    i.paint(ellipse(0.5, 0.52, 0.16, 0.3, 0.2), LEAF).paint(line(0.46, 0.26, 0.54, 0.78, 0.025), "#2a6a20")
    for k in range(3):
        y = 0.4 + k * 0.12
        i.paint(line(0.5, y, 0.4, y - 0.06, 0.02), "#2a6a20").paint(line(0.51, y, 0.62, y - 0.05, 0.02), "#2a6a20")


def cocoa(i):
    i.paint(ellipse(0.5, 0.52, 0.17, 0.27, 0.3), "#a85a2a")
    for k in range(3):
        i.paint(arc(0.5 - 0.06 + k * 0.06, 0.52, 0.22, 0.02, math.pi * 1.35, math.pi * 1.65), "#7a3a18")
    i.paint(rect(0.6, 0.24, 0.04, 0.1, 0.3), BROWN)


def cotton(i):
    for (x, y) in [(0.42, 0.42), (0.58, 0.42), (0.5, 0.54), (0.38, 0.56), (0.62, 0.56)]:
        i.paint(circle(x, y, 0.1), WHITE)
    i.paint(poly([(0.36, 0.64), (0.64, 0.64), (0.5, 0.8)]), BROWN)


def sugar(i):
    for (x, a) in [(0.36, -0.15), (0.5, 0.0), (0.64, 0.15)]:
        i.paint(line(x - a * 0.4, 0.82, x + a * 0.4, 0.22, 0.06), "#9ac060")
        for y in (0.4, 0.58):
            i.paint(rect(x, y, 0.08, 0.02), "#6a9040")
    leaf_pair(i, 0.5, 0.3)


def tobacco(i):
    i.paint(ellipse(0.5, 0.5, 0.18, 0.32, 0.5), "#8aa040").paint(line(0.36, 0.76, 0.64, 0.24, 0.03), "#5a7020")
    i.paint(ellipse(0.42, 0.62, 0.1, 0.2, 0.5), "#c8a050")


def spices(i):
    i.paint(poly([(0.24, 0.56), (0.76, 0.56), (0.64, 0.78), (0.36, 0.78)]), BROWN)
    i.paint(ellipse(0.5, 0.52, 0.24, 0.12), RED).paint(ellipse(0.44, 0.48, 0.1, 0.06), ORANGE)


def cinnamon(i):
    for k, x in enumerate((0.36, 0.5, 0.64)):
        i.paint(rect(x, 0.52, 0.1, 0.5, 0.2), "#a8582a").paint(rect(x + 0.02, 0.52, 0.03, 0.46, 0.2), "#7a3a1a")


def cloves(i):
    for (x, y, a) in [(0.38, 0.5, -0.4), (0.5, 0.46, 0.0), (0.62, 0.5, 0.4)]:
        i.paint(line(x, y + 0.04, x + 0.25 * math.sin(a), y + 0.32, 0.04), "#6a3a1a").paint(circle(x, y, 0.07), "#8a4a24")


def incense(i):
    i.paint(poly([(0.3, 0.7), (0.7, 0.7), (0.62, 0.82), (0.38, 0.82)]), "#c8a050")
    for k in range(3):
        i.paint(arc(0.44 + k * 0.06, 0.4 + k * 0.06, 0.12, 0.03, math.pi * 0.5, math.pi * 1.4), "#e8e0f0")


def dyes(i):
    for k, c in enumerate(("#c03a8a", "#3a6ac0", YELLOW)):
        x = 0.32 + k * 0.18
        i.paint(ellipse(x, 0.64, 0.09, 0.08), c).paint(poly([(x, 0.34), (x - 0.06, 0.6), (x + 0.06, 0.6)]), c)


def silk(i):
    i.paint(poly([(0.22, 0.3), (0.78, 0.3), (0.7, 0.5), (0.78, 0.74), (0.22, 0.74), (0.3, 0.5)]), "#e85a7a")
    i.paint(arc(0.5, 0.18, 0.3, 0.04, math.pi * 0.25, math.pi * 0.75), "#f8a8b8")


def wine(i):
    for (x, y) in [(0.44, 0.38), (0.56, 0.38), (0.38, 0.5), (0.5, 0.5), (0.62, 0.5), (0.44, 0.62), (0.56, 0.62), (0.5, 0.74)]:
        i.paint(circle(x, y, 0.07), "#c070c8").paint(circle(x - 0.02, y - 0.02, 0.025), "#e0b0e8")
    i.paint(ellipse(0.62, 0.24, 0.1, 0.06, -0.4), LEAF).paint(line(0.5, 0.32, 0.52, 0.22, 0.03), BROWN)


def furs(i):
    i.paint(poly([(0.3, 0.26), (0.7, 0.26), (0.78, 0.4), (0.7, 0.5), (0.74, 0.76), (0.26, 0.76), (0.3, 0.5), (0.22, 0.4)]), "#9a6a3a")
    i.paint(ellipse(0.5, 0.52, 0.12, 0.18), "#c8965a")


def cosmetics(i):
    i.paint(rect(0.5, 0.62, 0.26, 0.3), "#e8c8d8").paint(rect(0.5, 0.42, 0.14, 0.1), "#c8a050")
    i.paint(rect(0.5, 0.3, 0.1, 0.14), RED)


def perfume(i):
    i.paint(circle(0.5, 0.6, 0.2), "#b8a0e0").paint(rect(0.5, 0.36, 0.08, 0.1), "#c8a050").paint(circle(0.5, 0.28, 0.06), "#c8a050")
    i.paint(circle(0.44, 0.54, 0.05), "#e0d8f8")


def jeans(i):
    i.paint(poly([(0.3, 0.26), (0.7, 0.26), (0.74, 0.8), (0.56, 0.8), (0.5, 0.42), (0.44, 0.8), (0.26, 0.8)]), "#3a5a9a")
    i.paint(rect(0.5, 0.29, 0.4, 0.04), "#c8a050")


def toys(i):
    i.paint(rect(0.36, 0.6, 0.22, 0.22), RED).paint(rect(0.62, 0.62, 0.2, 0.2), BLUE).paint(rect(0.5, 0.4, 0.2, 0.2), YELLOW)


DRAW = {name: fn for name, fn in globals().items() if callable(fn) and not name.startswith("_") and name not in
        ("badge", "leaf_pair", "Icon", "arc", "circle", "ellipse", "line", "poly", "rect", "save_png", "main", "classes")}


def classes():
    with open(os.path.join(REPO, "data", "rules", "resources.json"), encoding="utf-8") as f:
        return {r["id"]: r["class"] for r in json.load(f)["resources"]}


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(args[0] if args else os.path.join(REPO, "unreal", "Content", "Slate", "Resources"))
    os.makedirs(out, exist_ok=True)
    missing = []
    for rid, cls in classes().items():
        name = rid.replace("RESOURCE_", "").lower()
        i = badge(cls)
        if name in DRAW:
            DRAW[name](i)
        else:
            missing.append(name)
            i.paint(circle(0.5, 0.5, 0.14), "#f2ecdc")
        rgb, alpha = i.finish(width=1)
        save_png(os.path.join(out, name + ".png"), rgb, alpha)
    print("resource icons written to %s; no pictogram for: %s" % (out, ", ".join(missing) or "none"))


if __name__ == "__main__":
    main()
