"""The UI icon set (plan D), drawn by script: simple pictograms with a dark outline, 64 px, saved as PNG
for Slate to load at run time (unreal/Content/Slate/Icons/, staged with the game).

  blender -b --python tools/ui/icons.py -- unreal/Content/Slate/Icons

Shapes are rasterised with numpy at 4x and averaged down, so edges are anti-aliased. Coordinates are
in a 0..1 square with y down. Yield icons are coloured; action icons are parchment white so they read
on any button.
"""
import math
import os
import sys

import numpy as np

S = 64          # icon size in pixels
SS = 4          # supersampling
N = S * SS

_y, _x = np.mgrid[0:N, 0:N].astype(np.float64)
X, Y = (_x + 0.5) / N, (_y + 0.5) / N


def circle(cx, cy, r):
    return (X - cx) ** 2 + (Y - cy) ** 2 <= r * r


def ellipse(cx, cy, rx, ry, angle=0.0):
    c, s = math.cos(angle), math.sin(angle)
    u, v = (X - cx) * c + (Y - cy) * s, -(X - cx) * s + (Y - cy) * c
    return (u / rx) ** 2 + (v / ry) ** 2 <= 1.0


def rect(cx, cy, w, h, angle=0.0):
    c, s = math.cos(angle), math.sin(angle)
    u, v = (X - cx) * c + (Y - cy) * s, -(X - cx) * s + (Y - cy) * c
    return (np.abs(u) <= w / 2) & (np.abs(v) <= h / 2)


def poly(points):
    inside = np.zeros((N, N), dtype=bool)
    n = len(points)
    for i in range(n):
        x0, y0 = points[i]
        x1, y1 = points[(i + 1) % n]
        if y0 == y1:
            continue
        crosses = ((Y >= min(y0, y1)) & (Y < max(y0, y1)))
        xi = x0 + (Y - y0) * (x1 - x0) / (y1 - y0)
        inside ^= crosses & (X < xi)
    return inside


def line(x0, y0, x1, y1, w):
    dx, dy = x1 - x0, y1 - y0
    L2 = dx * dx + dy * dy or 1e-9
    t = np.clip(((X - x0) * dx + (Y - y0) * dy) / L2, 0, 1)
    px, py = x0 + t * dx, y0 + t * dy
    return (X - px) ** 2 + (Y - py) ** 2 <= (w / 2) ** 2


def arc(cx, cy, r, w, a0, a1):
    """A thick arc from angle a0 to a1 (radians, y down)."""
    ang = np.arctan2(Y - cy, X - cx)
    d = np.sqrt((X - cx) ** 2 + (Y - cy) ** 2)
    a = (ang - a0) % (2 * math.pi)
    return (np.abs(d - r) <= w / 2) & (a <= (a1 - a0) % (2 * math.pi))


def hexcolor(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


class Icon:
    def __init__(self):
        self.rgb = np.zeros((N, N, 3))
        self.a = np.zeros((N, N))

    def paint(self, mask, color):
        c = hexcolor(color)
        self.rgb[mask] = c
        self.a[mask] = 1.0
        return self

    def erase(self, mask):
        self.a[mask] = 0.0
        return self

    def finish(self, outline="#1b140e", width=2):
        """A dark outline around everything painted, then down to S px."""
        a = self.a > 0
        grown = a.copy()
        r = width * SS
        for dy in range(-r, r + 1, SS // 2 or 1):
            for dx in range(-r, r + 1, SS // 2 or 1):
                if dx * dx + dy * dy <= r * r:
                    grown |= np.roll(np.roll(a, dy, 0), dx, 1)
        ring = grown & ~a
        rgb = self.rgb.copy()
        rgb[ring] = hexcolor(outline)
        alpha = np.where(grown, 1.0, 0.0) * np.where(a, self.a, 1.0)
        rgb = rgb.reshape(S, SS, S, SS, 3).mean(axis=(1, 3))
        alpha = alpha.reshape(S, SS, S, SS).mean(axis=(1, 3))
        return rgb, alpha


WHITE = "#efe6d2"


# ------------------------------------------------------------------------------------------- yields

def food():
    i = Icon()
    i.paint(line(0.5, 0.92, 0.5, 0.25, 0.06), "#c9a24a")  # stalk
    for k in range(4):
        y = 0.3 + k * 0.14
        i.paint(ellipse(0.39, y, 0.1, 0.055, -0.6), "#e2bf55").paint(ellipse(0.61, y, 0.1, 0.055, 0.6), "#e2bf55")
    i.paint(ellipse(0.5, 0.18, 0.06, 0.1), "#e2bf55")
    return i


def production():
    i = Icon()
    i.paint(rect(0.42, 0.6, 0.11, 0.62, 0.7), "#8a5a33")  # handle
    i.paint(rect(0.62, 0.3, 0.44, 0.18, 0.7), "#b8b8b8")  # head
    i.paint(rect(0.62, 0.3, 0.44, 0.06, 0.7), "#e0e0e0")
    return i


def gold():
    i = Icon()
    i.paint(circle(0.5, 0.5, 0.4), "#e0b030").paint(circle(0.5, 0.5, 0.3), "#f2cc4a")
    i.paint(rect(0.5, 0.5, 0.08, 0.36), "#c4921e").paint(rect(0.5, 0.5, 0.3, 0.08), "#c4921e")
    return i


def science():
    i = Icon()
    i.paint(poly([(0.38, 0.12), (0.62, 0.12), (0.62, 0.42), (0.86, 0.86), (0.14, 0.86), (0.38, 0.42)]), "#cfe3f2")
    i.paint(poly([(0.27, 0.66), (0.73, 0.66), (0.86, 0.86), (0.14, 0.86)]), "#4aa3e0")
    i.paint(circle(0.45, 0.76, 0.04), "#d8f0ff").paint(circle(0.58, 0.72, 0.03), "#d8f0ff")
    return i


def culture():
    i = Icon()
    i.paint(ellipse(0.5, 0.5, 0.34, 0.4), "#b07ad8")
    i.erase(ellipse(0.38, 0.42, 0.07, 0.05, 0.3)).erase(ellipse(0.62, 0.42, 0.07, 0.05, -0.3))
    i.erase(arc(0.5, 0.56, 0.17, 0.06, 0.25, math.pi - 0.25))
    return i


def faith():
    i = Icon()
    i.paint(poly([(0.5, 0.08), (0.7, 0.42), (0.66, 0.78), (0.5, 0.92), (0.34, 0.78), (0.3, 0.42)]), "#e8f4ff")
    i.paint(poly([(0.5, 0.36), (0.6, 0.58), (0.56, 0.8), (0.5, 0.86), (0.44, 0.8), (0.4, 0.58)]), "#9fd0f5")
    return i


def favor():
    i = Icon()
    i.paint(poly([(0.3, 0.1), (0.42, 0.1), (0.56, 0.42), (0.44, 0.42)]), "#3d6fb0")
    i.paint(poly([(0.7, 0.1), (0.58, 0.1), (0.44, 0.42), (0.56, 0.42)]), "#c04848")
    i.paint(circle(0.5, 0.64, 0.26), "#e0b030").paint(circle(0.5, 0.64, 0.17), "#f2cc4a")
    return i


def tourism():
    i = Icon()
    i.paint(circle(0.5, 0.5, 0.4), "#48a888")
    i.erase(arc(0.5, 0.5, 0.2, 0.05, 0, 2 * math.pi)).erase(rect(0.5, 0.5, 0.05, 0.8)).erase(rect(0.5, 0.5, 0.8, 0.05))
    return i


def housing():
    i = Icon()
    i.paint(poly([(0.5, 0.12), (0.9, 0.48), (0.1, 0.48)]), "#b45f42").paint(rect(0.5, 0.68, 0.6, 0.4), "#e3d9c3")
    i.paint(rect(0.5, 0.76, 0.16, 0.24), "#5a3d26")
    return i


def amenity():
    i = Icon()
    i.paint(circle(0.5, 0.5, 0.38), "#f2cc4a")
    i.erase(circle(0.37, 0.42, 0.05)).erase(circle(0.63, 0.42, 0.05)).erase(arc(0.5, 0.52, 0.2, 0.06, 0.4, math.pi - 0.4))
    return i


# ------------------------------------------------------------------------------------------- unit stats

def strength():
    i = Icon()
    i.paint(rect(0.5, 0.42, 0.1, 0.6, 0.785), WHITE).paint(rect(0.5, 0.42, 0.04, 0.6, 0.785), "#c8c8c8")
    i.paint(rect(0.33, 0.6, 0.3, 0.07, -0.785), "#c9a24a").paint(line(0.22, 0.78, 0.3, 0.7, 0.08), "#8a5a33")
    return i


def ranged():
    i = Icon()
    i.paint(arc(0.5, 0.5, 0.36, 0.07, -2.4, 0.8), "#c9a24a")
    i.paint(line(0.24, 0.76, 0.76, 0.24, 0.04), WHITE).paint(poly([(0.84, 0.16), (0.66, 0.22), (0.78, 0.34)]), WHITE)
    return i


def moves():
    i = Icon()
    i.paint(poly([(0.3, 0.12), (0.52, 0.12), (0.52, 0.6), (0.86, 0.72), (0.86, 0.88), (0.3, 0.88)]), "#9a6a3e")
    i.paint(rect(0.58, 0.88, 0.56, 0.06), "#5a3d26")
    return i


def health():
    i = Icon()
    i.paint(circle(0.35, 0.38, 0.19), "#d04848").paint(circle(0.65, 0.38, 0.19), "#d04848")
    i.paint(poly([(0.17, 0.45), (0.83, 0.45), (0.5, 0.86)]), "#d04848")
    return i


def experience():
    i = Icon()
    pts = []
    for k in range(10):
        r = 0.42 if k % 2 == 0 else 0.18
        a = -math.pi / 2 + k * math.pi / 5
        pts.append((0.5 + r * math.cos(a), 0.54 + r * math.sin(a)))
    return i.paint(poly(pts), "#f2cc4a")


# ------------------------------------------------------------------------------------------- actions

def a_skip():
    i = Icon()
    i.paint(poly([(0.18, 0.2), (0.52, 0.5), (0.18, 0.8)]), WHITE).paint(poly([(0.48, 0.2), (0.82, 0.5), (0.48, 0.8)]), WHITE)
    return i


def a_fortify():
    i = Icon()
    i.paint(poly([(0.5, 0.08), (0.86, 0.2), (0.82, 0.55), (0.5, 0.92), (0.18, 0.55), (0.14, 0.2)]), WHITE)
    i.paint(poly([(0.5, 0.2), (0.74, 0.28), (0.71, 0.53), (0.5, 0.78)]), "#b8ad98")
    return i


def a_sleep():
    i = Icon()
    i.paint(circle(0.46, 0.52, 0.36), WHITE).erase(circle(0.62, 0.42, 0.3))
    return i


def a_found():
    i = Icon()
    i.paint(rect(0.5, 0.62, 0.56, 0.5), WHITE)
    for x in (0.28, 0.43, 0.57, 0.72):
        i.paint(rect(x, 0.33, 0.1, 0.12), WHITE)
    i.erase(rect(0.5, 0.74, 0.14, 0.26))
    i.paint(line(0.5, 0.06, 0.5, 0.3, 0.04), WHITE).paint(poly([(0.52, 0.06), (0.74, 0.12), (0.52, 0.18)]), "#d04848")
    return i


def a_build():
    i = production()
    return i


def a_promote():
    i = Icon()
    for k, y in enumerate((0.3, 0.52, 0.74)):
        i.paint(poly([(0.14, y + 0.12), (0.5, y - 0.12), (0.86, y + 0.12), (0.86, y + 0.24), (0.5, y), (0.14, y + 0.24)]), "#f2cc4a" if k == 0 else WHITE)
    return i


def a_trade():
    i = Icon()
    i.paint(arc(0.5, 0.62, 0.32, 0.07, math.pi, 2 * math.pi), WHITE)
    i.paint(circle(0.18, 0.66, 0.1), "#f2cc4a").paint(circle(0.82, 0.66, 0.1), "#f2cc4a")
    return i


def a_religion():
    return faith()


def a_great_person():
    return experience()


def a_gear():
    i = Icon()
    i.paint(poly([(0.2, 0.2), (0.8, 0.2), (0.76, 0.62), (0.5, 0.86), (0.24, 0.62)]), "#b8b8b8")
    i.paint(rect(0.5, 0.42, 0.42, 0.06), "#7a7a7a")
    return i


def a_link():
    i = Icon()
    i.paint(ellipse(0.36, 0.5, 0.22, 0.13, -0.6), WHITE).erase(ellipse(0.36, 0.5, 0.13, 0.05, -0.6))
    i.paint(ellipse(0.64, 0.5, 0.22, 0.13, -0.6), WHITE).erase(ellipse(0.64, 0.5, 0.13, 0.05, -0.6))
    return i


def a_streets():
    return housing()


def a_attack():
    i = strength()
    i.paint(rect(0.5, 0.42, 0.1, 0.6, -0.785), WHITE)
    return i


def end_turn():
    i = Icon()
    i.paint(poly([(0.14, 0.36), (0.56, 0.36), (0.56, 0.16), (0.88, 0.5), (0.56, 0.84), (0.56, 0.64), (0.14, 0.64)]), WHITE)
    return i


def menu():
    i = Icon()
    for y in (0.28, 0.5, 0.72):
        i.paint(rect(0.5, y, 0.66, 0.1), WHITE)
    return i


def research():
    return science()


def civic():
    return culture()


def government():
    i = Icon()
    i.paint(rect(0.5, 0.82, 0.76, 0.08), WHITE).paint(poly([(0.5, 0.1), (0.88, 0.32), (0.12, 0.32)]), WHITE)
    for x in (0.24, 0.41, 0.59, 0.76):
        i.paint(rect(x, 0.57, 0.08, 0.42), WHITE)
    return i


def era():
    i = Icon()
    i.paint(poly([(0.24, 0.12), (0.76, 0.12), (0.5, 0.5)]), "#e2bf55").paint(poly([(0.5, 0.5), (0.76, 0.88), (0.24, 0.88)]), "#e2bf55")
    i.paint(line(0.2, 0.1, 0.8, 0.1, 0.06), WHITE).paint(line(0.2, 0.9, 0.8, 0.9, 0.06), WHITE)
    return i


ICONS = {
    "food": food, "production": production, "gold": gold, "science": science, "culture": culture, "faith": faith,
    "favor": favor, "tourism": tourism, "housing": housing, "amenity": amenity,
    "strength": strength, "ranged": ranged, "moves": moves, "health": health, "experience": experience,
    "skip": a_skip, "fortify": a_fortify, "sleep": a_sleep, "found": a_found, "build": a_build, "promote": a_promote,
    "trade": a_trade, "religion": a_religion, "greatperson": a_great_person, "gear": a_gear, "link": a_link,
    "streets": a_streets, "attack": a_attack, "endturn": end_turn, "menu": menu, "research": research, "civic": civic,
    "government": government, "era": era,
}


def save_png(path, rgb, alpha):
    import bpy

    img = bpy.data.images.new(os.path.basename(path), width=S, height=S, alpha=True)
    rgba = np.zeros((S, S, 4), dtype=np.float32)
    rgba[:, :, :3] = rgb
    rgba[:, :, 3] = alpha
    img.pixels.foreach_set(np.flipud(rgba).ravel())  # Blender's rows start at the bottom
    img.update()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(args[0] if args else "unreal/Content/Slate/Icons")
    os.makedirs(out, exist_ok=True)
    for name, make in ICONS.items():
        rgb, alpha = make().finish()
        save_png(os.path.join(out, name + ".png"), rgb, alpha)
    print("icons written: %d to %s" % (len(ICONS), out))


if __name__ == "__main__":
    main()
