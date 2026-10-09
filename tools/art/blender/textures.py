"""The kit detail sheet (run inside Blender, from generate.py): a 4x4 sheet of greyscale painted
patterns, each tile 256 px and seamless on its own, that the kit material multiplies over the vertex
colours (specs/sovereign/leaders-and-art-style.md: hand-painted, simplified textures). Mid grey is
"no change"; brighter lightens, darker deepens (the material doubles the value).

Every pattern is built from periodic noise (white noise filtered in the frequency domain), so it
tiles without seams. The tile a face uses is chosen by kitlib (vertex colour alpha).
"""
import numpy as np

TILE = 256
GRID = 4
# Pattern name -> tile index (kitlib.PATTERNS mirrors this order).
NAMES = ["plain", "cloth", "leather", "metal", "skin", "hair", "wood", "stone", "plaster", "rooftile", "foliage", "thatch",
         "water", "grass", "sand"]


def _noise(seed, cutoff, aniso=(1.0, 1.0)):
    """Periodic smooth noise in [-1, 1]: frequencies above `cutoff` (cycles per tile) fall away;
    `aniso` stretches it (a larger x keeps it smooth across x, i.e. streaks along x)."""
    rng = np.random.default_rng(seed)
    white = rng.standard_normal((TILE, TILE))
    fy = np.fft.fftfreq(TILE) * TILE
    fx = np.fft.fftfreq(TILE) * TILE
    FX, FY = np.meshgrid(fx, fy)
    r = np.sqrt((FX * aniso[0]) ** 2 + (FY * aniso[1]) ** 2)
    filt = np.exp(-(r / cutoff) ** 2)
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * filt))
    out -= out.mean()
    return out / (np.abs(out).max() + 1e-9)


def _grid():
    y, x = np.mgrid[0:TILE, 0:TILE].astype(np.float64)
    return x, y


def plain():
    return np.zeros((TILE, TILE))


def cloth():
    """Plain weave: threads over and under, a little slub, soft painted blotches."""
    x, y = _grid()
    t = 8.0
    warp = np.cos(np.pi * (x % t) / t * 2 - np.pi) * 0.5 + 0.5
    weft = np.cos(np.pi * (y % t) / t * 2 - np.pi) * 0.5 + 0.5
    over = ((np.floor(x / t) + np.floor(y / t)) % 2) == 0
    weave = np.where(over, warp * 0.6 + 0.4, weft * 0.6 + 0.4) - 0.7
    return weave * 0.22 + _noise(11, 6) * 0.08 + _noise(12, 60) * 0.04


def leather():
    return _noise(21, 40) * 0.07 + _noise(22, 5) * 0.08 + np.clip(_noise(23, 90, (1.0, 6.0)) - 0.55, 0, 1) * 0.5


def metal():
    """Brushed streaks, darker dents and worn bright edges."""
    streak = _noise(31, 50, (8.0, 1.0)) * 0.1
    wear = np.clip(_noise(32, 6) - 0.15, 0, 1) * 0.4  # rubbed bright where hands and weather wear it
    tarnish = -np.clip(-_noise(34, 4) - 0.2, 0, 1) * 0.35  # darker patina in the hollows
    dents = -np.clip(_noise(33, 18) - 0.55, 0, 1) * 0.6
    return streak + wear + tarnish + dents


def skin():
    return _noise(41, 4) * 0.05 + _noise(42, 70) * 0.025


def hair():
    return _noise(51, 60, (0.6, 6.0)) * 0.22 + _noise(52, 4) * 0.06


def wood():
    """Grain: stretched rings bent by noise, and a few knots."""
    x, y = _grid()
    bend = _noise(61, 3, (1.0, 0.4)) * 0.9
    grain = np.sin(2 * np.pi * (x / TILE * 9 + bend))
    return grain * 0.09 + _noise(62, 40, (0.3, 5.0)) * 0.08


def stone():
    base = _noise(71, 8) * 0.1 + _noise(72, 30) * 0.07 + _noise(73, 90) * 0.03
    cracks = -np.clip(np.abs(_noise(74, 12)) < 0.035, 0, 1) * 0.18
    return base + cracks


def plaster():
    """Whitewash: broad brush blotches and faint horizontal strokes."""
    return _noise(81, 3) * 0.06 + _noise(82, 25, (0.4, 3.0)) * 0.035


def rooftile():
    """Rows of curved terracotta tiles with dark gaps and a lighter crown on each."""
    x, y = _grid()
    rows, cols = 8, 6
    v = (y % (TILE / rows)) / (TILE / rows)
    u = (x % (TILE / cols)) / (TILE / cols)
    curve = np.sin(np.pi * u) * 0.12 - 0.06
    gap = -np.clip(0.12 - v, 0, 1) * 1.6
    return curve + gap + _noise(91, 20) * 0.05


def foliage():
    clumps = _noise(101, 14) * 0.16 + _noise(102, 45) * 0.1
    return clumps - np.clip(-_noise(103, 25) - 0.4, 0, 1) * 0.4


def thatch():
    return _noise(111, 70, (0.3, 8.0)) * 0.2 + _noise(112, 5) * 0.06


def water():
    """Soft ripples running across the tile, broken up so they do not read as stripes."""
    x, y = _grid()
    waves = np.sin((y / TILE) * 2 * np.pi * 6 + _noise(121, 6) * 2.5) * 0.08
    return waves + _noise(122, 18, (2.5, 1.0)) * 0.07 + _noise(123, 4) * 0.05


def grass():
    """Fine blades in tufts over gentle patches of lighter and darker ground (the map's open land)."""
    blades = _noise(131, 90, (6.0, 0.4)) * 0.035
    tufts = _noise(132, 16) * 0.13
    return blades + tufts + _noise(133, 4) * 0.08


def sand():
    """Wind ripples on dunes: long soft bands and a fine grain."""
    x, y = _grid()
    dunes = np.sin((x + y * 0.35) / TILE * 2 * np.pi * 3 + _noise(141, 5) * 1.8) * 0.07
    return dunes + _noise(142, 110) * 0.05 + _noise(143, 6) * 0.06


PATTERNS = [plain, cloth, leather, metal, skin, hair, wood, stone, plaster, rooftile, foliage, thatch, water, grass, sand]


def atlas():
    """The sheet as values in 0..1 (0.5 = no change), rows from the bottom as Blender images store them."""
    sheet = np.full((TILE * GRID, TILE * GRID), 0.5)
    for i, make in enumerate(PATTERNS):
        col, row = i % GRID, i // GRID
        tile = np.clip(0.5 + make() * 0.95, 0.0, 1.0)
        # Blender's pixel rows start at the bottom; tile row 0 is drawn at the top of the image.
        top = (GRID - 1 - row) * TILE
        sheet[top:top + TILE, col * TILE:(col + 1) * TILE] = tile
    return sheet


def save(path):
    import bpy

    sheet = atlas()
    n = TILE * GRID
    img = bpy.data.images.new("T_SovDetail", width=n, height=n, alpha=False, float_buffer=False)
    rgba = np.ones((n, n, 4), dtype=np.float32)
    for c in range(3):
        rgba[:, :, c] = sheet
    img.colorspace_settings.name = "Non-Color"  # before the pixels: changing it reloads the image
    img.pixels.foreach_set(rgba.ravel())
    img.update()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img
