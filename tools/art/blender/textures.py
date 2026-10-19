"""The kit detail sheet (run inside Blender, from generate.py): an 8x8 sheet of greyscale patterns that the kit
material multiplies over the vertex colours, and a matching sheet of normals for their relief
(specs/sovereign/leaders-and-art-style.md: hand-painted, simplified textures). Mid grey is "no change";
brighter lightens, darker deepens (the material doubles the value).

Buildings and paving (masonry, brick, plaster, roof tiles, thatch, adobe, flagstones, cobbles) come from CC0
photo scans (sources.py), their colour taken out so the kit's palette stays in charge; the rest are painted here
from periodic noise (white noise filtered in the frequency domain), and so is any scan not downloaded.

Each cell is 512 px: one period of its pattern (480 px) wrapped 16 px past each edge, so the material can stay
inside the cell and still tile without a seam. The cell a face uses is chosen by kitlib (vertex colour alpha).
"""
import os

import numpy as np

import sources

# Where tools/art/fetch_textures.py put the scans (build_art.py's build folder).
BUILD = os.environ.get("SOV_ART_BUILD") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "art", "build")
TILE = 256  # the painted patterns' own size; every pattern is resampled to PERIOD
GRID = 8
CELL = 512
PAD = 16
PERIOD = CELL - 2 * PAD
# Pattern name -> cell index (kitlib.PATTERNS mirrors this order).
NAMES = ["plain", "cloth", "leather", "metal", "skin", "hair", "wood", "stone", "plaster", "rooftile", "foliage", "thatch",
         "water", "grass", "sand", "masonry", "brick", "adobe", "flagstones", "cobbles"]
# How deep each painted pattern's relief reads (0: flat). Scans use their own normal maps.
RELIEF = {"water": 0.8, "cloth": 0.6, "leather": 0.8, "metal": 0.5, "wood": 1.0, "stone": 2.0, "plaster": 0.8, "rooftile": 2.0,
          "foliage": 1.2, "thatch": 1.5, "masonry": 2.0, "brick": 2.0, "adobe": 1.0, "flagstones": 2.0, "cobbles": 2.0}


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


def masonry():
    """Dressed blocks in courses, each its own shade, with sunken joints (painted in place of the scan)."""
    x, y = _grid()
    rows, cols = 6, 3
    h, w = TILE / rows, TILE / cols
    r = np.floor(y / h)
    xs = (x + (r % 2) * w / 2) % TILE
    c = np.floor(xs / w)
    joint = (np.minimum(y % h, h - y % h) < 2.5) | (np.minimum(xs % w, w - xs % w) < 2.5)
    shade = (np.sin(r * 12.9898 + c * 78.233) * 43758.5453) % 1.0 - 0.5
    return np.where(joint, -0.3, shade * 0.12) + _noise(151, 40) * 0.05


def brick():
    """Running-bond bricks, the mortar lighter than the brick."""
    x, y = _grid()
    rows, cols = 16, 4
    h, w = TILE / rows, TILE / cols
    r = np.floor(y / h)
    xs = (x + (r % 2) * w / 2) % TILE
    c = np.floor(xs / w)
    mortar = (np.minimum(y % h, h - y % h) < 1.5) | (np.minimum(xs % w, w - xs % w) < 1.5)
    shade = (np.sin(r * 12.9898 + c * 78.233) * 43758.5453) % 1.0 - 0.5
    return np.where(mortar, 0.18, shade * 0.14) + _noise(161, 60) * 0.04


def adobe():
    """Mud plaster: broad smears and a few fine cracks."""
    return _noise(171, 4) * 0.08 + _noise(172, 30) * 0.05 - np.clip(np.abs(_noise(173, 10)) < 0.02, 0, 1) * 0.12


def flagstones():
    return masonry()


def cobbles():
    """Rounded setts in a grid with dark gaps between."""
    x, y = _grid()
    n = 12
    cx = (x % (TILE / n)) / (TILE / n) - 0.5
    cy = (y % (TILE / n)) / (TILE / n) - 0.5
    return np.clip((0.25 - (cx ** 2 + cy ** 2)) * 1.2, -0.25, 0.2) + _noise(181, 50) * 0.05


PATTERNS = [plain, cloth, leather, metal, skin, hair, wood, stone, plaster, rooftile, foliage, thatch, water, grass, sand,
            masonry, brick, adobe, flagstones, cobbles]


def _resample(a, n):
    """A periodic image resampled to n x n through its spectrum, so it stays seamless."""
    m = a.shape[0]
    if m == n:
        return a.copy()
    f = np.fft.fftshift(np.fft.fft2(a))
    if n < m:
        lo = (m - n) // 2
        f = f[lo:lo + n, lo:lo + n]
    else:
        lo = (n - m) // 2
        g = np.zeros((n, n), dtype=complex)
        g[lo:lo + m, lo:lo + m] = f
        f = g
    return np.real(np.fft.ifft2(np.fft.ifftshift(f))) * (n * n) / float(m * m)


def _blur(a, cycles):
    """Only what varies slower than about `cycles` per tile (a periodic Gaussian)."""
    m = a.shape[0]
    f = np.fft.fftfreq(m) * m
    FX, FY = np.meshgrid(f, f)
    return np.real(np.fft.ifft2(np.fft.fft2(a) * np.exp(-(FX ** 2 + FY ** 2) / (2 * cycles ** 2))))


def _relief(h, depth):
    """Normals (x, y, z in -1..1, OpenGL: +y up the image) from a height field whose rows run from the top."""
    gx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5
    gy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5
    k = depth * PERIOD / 32.0
    n = np.stack([-gx * k, gy * k, np.ones_like(h)], axis=-1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def _load(path):
    """An image as float RGB, rows from the top (Blender stores them from the bottom)."""
    import bpy

    img = bpy.data.images.load(path)
    img.colorspace_settings.name = "Non-Color"  # the raw bytes: sRGB colour for the scan, encoded vectors for normals
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    return np.flipud(px.reshape(h, w, 4)[:, :, :3]).astype(np.float64)


def _scan(name, build):
    """The pattern's tile and normals from its photo scan, or None when the scan was not downloaded."""
    src = sources.SOURCES.get(name)
    if not src or not build:
        return None
    folder = os.path.join(build, "Sources", src["id"])
    paths = [os.path.join(folder, f) for f in sources.MAPS.values()]
    if not all(os.path.exists(p) for p in paths):
        return None
    lum = _load(paths[0]) @ np.array([0.2126, 0.7152, 0.0722])
    # Light and shade against the neighbourhood, so the scan's own lighting and stains fade and its colour drops out.
    rel = lum / np.maximum(_blur(lum, 2.0), 1e-3) - 1.0
    tile = _resample(rel, PERIOD) * 0.5 * src["contrast"]
    nrm = _load(paths[1]) * 2.0 - 1.0
    nx = _resample(nrm[:, :, 0], PERIOD) * src["relief"]
    ny = _resample(nrm[:, :, 1], PERIOD) * src["relief"]
    nz = np.sqrt(np.clip(1.0 - nx ** 2 - ny ** 2, 0.05, 1.0))
    n = np.stack([nx, ny, nz], axis=-1)
    return tile, n / np.linalg.norm(n, axis=-1, keepdims=True)


def cells(build=BUILD):
    """Each pattern's tile (centred on 0) and normals at PERIOD px: from its scan where there is one, else painted."""
    out = []
    for name, make in zip(NAMES, PATTERNS):
        got = _scan(name, build)
        print("detail %s: %s" % (name, "scan " + sources.SOURCES[name]["id"] if got else "painted"))
        if got is None:
            tile = _resample(make(), PERIOD)
            got = (tile, _relief(tile, RELIEF.get(name, 0.0)))
        out.append(got)
    return out


def atlas(build=BUILD):
    """The detail sheet (0..1, 0.5 = no change) and the normal sheet (OpenGL, encoded 0..1), rows from the top,
    each cell its pattern wrapped PAD px past its edges."""
    n = CELL * GRID
    sheet = np.full((n, n), 0.5)
    normals = np.zeros((n, n, 3))
    normals[:, :] = (0.5, 0.5, 1.0)
    for i, (tile, nrm) in enumerate(cells(build)):
        y0, x0 = (i // GRID) * CELL, (i % GRID) * CELL
        sheet[y0:y0 + CELL, x0:x0 + CELL] = np.pad(np.clip(0.5 + tile * 0.95, 0.0, 1.0), PAD, mode="wrap")
        normals[y0:y0 + CELL, x0:x0 + CELL] = np.pad(nrm * 0.5 + 0.5, ((PAD, PAD), (PAD, PAD), (0, 0)), mode="wrap")
    return sheet, normals


def _write(name, rgb, path):
    import bpy

    n = rgb.shape[0]
    img = bpy.data.images.new(name, width=n, height=n, alpha=False, float_buffer=False)
    rgba = np.ones((n, n, 4), dtype=np.float32)
    rgba[:, :, :3] = np.flipud(rgb)  # Blender's pixel rows start at the bottom
    img.colorspace_settings.name = "Non-Color"  # before the pixels: changing it reloads the image
    img.pixels.foreach_set(rgba.ravel())
    img.update()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img


def save(path, build=BUILD):
    """Writes the detail sheet to `path` and the normal sheet beside it as T_SovDetailN.png; returns both images.
    `build` is the art build folder whose Sources/ holds the fetched scans."""
    sheet, normals = atlas(build)
    detail = _write("T_SovDetail", np.repeat(sheet[:, :, None], 3, axis=2), path)
    normal = _write("T_SovDetailN", normals, os.path.join(os.path.dirname(path), "T_SovDetailN.png"))
    return detail, normal
