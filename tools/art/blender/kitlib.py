"""Small modelling library for Sovereign's scripted art (run inside Blender).

Pieces are built from simple solids in metres, with the pivot at the base centre.
Every face gets vertex colours "painted" by script: the base colour, darkened toward
the ground and lightened toward the top, with a little seeded variation. Together with
flat shading this gives the simplified, hand-painted stylized-realism look the art doc
asks for (specs/sovereign/leaders-and-art-style.md) without textures.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector


# Painted detail patterns (textures.py builds the sheet in this order): the pattern's tile index goes
# into the vertex colour's alpha, and the face's UVs repeat it every so many metres.
PATTERNS = ["plain", "cloth", "leather", "metal", "skin", "hair", "wood", "stone", "plaster", "rooftile", "foliage", "thatch"]
REPEAT = {"plain": 1.0, "cloth": 0.45, "leather": 0.5, "metal": 0.6, "skin": 0.5, "hair": 0.35, "wood": 1.2,
          "stone": 1.6, "plaster": 2.4, "rooftile": 1.4, "foliage": 1.4, "thatch": 1.1}
GRID = 4  # the sheet holds GRID x GRID tiles
# Which pattern a colour wears, by its '#rrggbb' (kits fill this in; unknown colours are plain).
PATTERN_OF = {}


def patterns(mapping):
    """A kit registers its colours' patterns: {pattern: [colours]}."""
    for name, colors in mapping.items():
        for c in colors if isinstance(colors, (list, tuple)) else [colors]:
            PATTERN_OF[c.lower()] = name


def srgb(hex_or_tuple):
    """'#RRGGBB' or (r, g, b) in 0..1 -> (r, g, b) in 0..1."""
    if isinstance(hex_or_tuple, str):
        h = hex_or_tuple.lstrip("#")
        return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
    return tuple(hex_or_tuple)


class Piece:
    """Collects solids into one mesh with per-corner colours."""

    def __init__(self, name, seed=1):
        self.name = name
        self.bm = bmesh.new()
        self.colors = {}  # face -> (r, g, b)
        self.team = set()  # faces on material slot 1 (tinted with the owner's colour in game)
        self.tint_next = False
        self.rng = random.Random(seed)
        self.height = 1.0
        # Smooth pieces (figures) shade softly; each solid also keeps its own height range so the paint
        # darkens toward the foot of every part, as a painter shades each fold and limb.
        self.smooth = False
        self.part_z = {}  # face -> (z0, z1) of the solid it belongs to
        self.part_shade = 0.0  # how much of the shading follows each part rather than the whole piece
        self.shade_low, self.shade_range = 0.72, 0.36  # paint at the foot, and the rise to the crown
        self.pattern = {}  # face -> pattern name
        self.pattern_next = None  # a pattern for the solids added next, over the colour's own
        self.scale = 1.0  # how small the patterns repeat (figures: smaller)

    # ------------------------------------------------------------------ solids
    def team_color(self, on=True):
        """Solids added while on go to the team slot (owner colour); paint them light."""
        self.tint_next = on

    def wear(self, pattern=None):
        """Solids added next wear this pattern whatever their colour (None: back to the colour's own)."""
        self.pattern_next = pattern

    def _tag(self, faces, color, jitter=0.04):
        c = srgb(color)
        pat = self.pattern_next or (PATTERN_OF.get(color.lower(), "plain") if isinstance(color, str) else "plain")
        for f in faces:
            self.pattern[f] = pat
        zs = [v.co.z for f in faces for v in f.verts] or [0.0]
        span = (min(zs), max(zs))
        for f in faces:
            self.part_z[f] = span
            if self.tint_next:
                self.team.add(f)
            j = 1.0 + self.rng.uniform(-jitter, jitter)
            self.colors[f] = tuple(min(1.0, max(0.0, v * j)) for v in c)

    def box(self, center, size, color, rot_z=0.0, taper=1.0):
        """Axis box (taper < 1 narrows the top)."""
        new = bmesh.ops.create_cube(self.bm, size=1.0)
        verts = new["verts"]
        for v in verts:
            if v.co.z > 0 and taper != 1.0:
                v.co.x *= taper
                v.co.y *= taper
            v.co.x *= size[0]
            v.co.y *= size[1]
            v.co.z *= size[2]
        self._place(verts, center, rot_z)
        self._tag(self._faces_of(verts), color)
        return verts

    def cylinder(self, center, radius, height, color, segments=10, radius_top=None, rot_z=0.0):
        rt = radius if radius_top is None else radius_top
        new = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius, radius2=rt, depth=height)
        verts = new["verts"]
        self._place(verts, (center[0], center[1], center[2] + height / 2), rot_z)
        self._tag(self._faces_of(verts), color)
        return verts

    def disc(self, center, radius, thickness, color, segments=12):
        """A flat round plate standing upright, facing -Y (shields)."""
        new = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius, depth=thickness)
        verts = new["verts"]
        bmesh.ops.transform(self.bm, matrix=Matrix.Rotation(math.pi / 2, 4, "X"), verts=verts)
        self._place(verts, center, 0.0)
        self._tag(self._faces_of(verts), color)
        return verts

    def cone(self, center, radius, height, color, segments=10):
        return self.cylinder(center, radius, height, color, segments, radius_top=0.0)

    def blob(self, center, radius, color, squash=1.0, subdiv=1, wobble=0.12):
        """Low-poly rounded mass (foliage, rocks, heads)."""
        new = bmesh.ops.create_icosphere(self.bm, subdivisions=subdiv, radius=radius)
        verts = new["verts"]
        for v in verts:
            k = 1.0 + self.rng.uniform(-wobble, wobble)
            v.co *= k
            v.co.z *= squash
        self._place(verts, center, 0.0)
        self._tag(self._faces_of(verts), color, jitter=0.07)
        return verts

    def gable(self, center, length, width, rise, color, overhang=0.3, rot_z=0.0):
        """Pitched roof: a triangular prism along X, sitting on `center` (its eave height)."""
        L, W = length / 2 + overhang, width / 2 + overhang
        pts = [(-L, -W, 0), (L, -W, 0), (L, W, 0), (-L, W, 0), (-L, 0, rise), (L, 0, rise)]
        vs = [self.bm.verts.new(p) for p in pts]
        faces = [
            self.bm.faces.new((vs[0], vs[1], vs[5], vs[4])),
            self.bm.faces.new((vs[2], vs[3], vs[4], vs[5])),
            self.bm.faces.new((vs[0], vs[4], vs[3])),
            self.bm.faces.new((vs[1], vs[2], vs[5])),
            self.bm.faces.new((vs[0], vs[3], vs[2], vs[1])),
        ]
        self._place(vs, center, rot_z)
        self._tag(faces, color)
        return vs

    def hip(self, center, length, width, rise, color, overhang=0.3, rot_z=0.0):
        """Four-sided roof."""
        L, W = length / 2 + overhang, width / 2 + overhang
        ridge = max(0.0, L - W)
        pts = [(-L, -W, 0), (L, -W, 0), (L, W, 0), (-L, W, 0), (-ridge, 0, rise), (ridge, 0, rise)]
        vs = [self.bm.verts.new(p) for p in pts]
        faces = [
            self.bm.faces.new((vs[0], vs[1], vs[5], vs[4])),
            self.bm.faces.new((vs[2], vs[3], vs[4], vs[5])),
            self.bm.faces.new((vs[0], vs[4], vs[3])),
            self.bm.faces.new((vs[1], vs[2], vs[5])),
            self.bm.faces.new((vs[0], vs[3], vs[2], vs[1])),
        ]
        self._place(vs, center, rot_z)
        self._tag(faces, color)
        return vs

    def loft(self, rings, color, segments=16, cap_bottom=True, cap_top=True, arc=None, folds=0, fold_depth=0.0, two_sided=False):
        """A shape through stacked elliptical rings, bottom to top: each ring is (z, rx, ry) or
        (z, rx, ry, cx, cy). `arc` (a0, a1) in radians keeps only part of the ring (an open sheet,
        e.g. a cloak; angle 0 is +X, pi/2 is +Y, the back); `folds` ripples the outline like cloth;
        `two_sided` adds the back faces of an open sheet so it shows from behind."""
        closed = arc is None
        a0, a1 = (0.0, 2 * math.pi) if closed else arc
        n = segments if closed else segments + 1
        grid = []
        for ring in rings:
            z, rx, ry = ring[0], ring[1], ring[2]
            cx, cy = (ring[3], ring[4]) if len(ring) > 4 else (0.0, 0.0)
            row = []
            for i in range(n):
                a = a0 + (a1 - a0) * i / (segments if closed else segments)
                k = 1.0 + (fold_depth * math.sin(folds * a) if folds else 0.0)
                row.append(self.bm.verts.new((cx + rx * k * math.cos(a), cy + ry * k * math.sin(a), z)))
            grid.append(row)
        faces = []
        # The back of a two-sided sheet needs its own vertices (a face may not repeat another's).
        back = [[self.bm.verts.new(v.co) for v in row] for row in grid] if two_sided else None
        for r in range(len(grid) - 1):
            lo, hi = grid[r], grid[r + 1]
            for i in range(n if closed else n - 1):
                j = (i + 1) % n
                faces.append(self.bm.faces.new((lo[i], lo[j], hi[j], hi[i])))
                if two_sided:
                    blo, bhi = back[r], back[r + 1]
                    faces.append(self.bm.faces.new((bhi[i], bhi[j], blo[j], blo[i])))
        if closed and cap_bottom:
            faces.append(self.bm.faces.new(list(reversed(grid[0]))))
        if closed and cap_top:
            faces.append(self.bm.faces.new(grid[-1]))
        self._tag(faces, color)
        return [v for row in grid for v in row]

    def segment(self, p0, p1, r0, r1, color, segments=10, caps=True):
        """A tapered limb between two points (radius r0 at p0, r1 at p1), with rounded ends."""
        a, b = Vector(p0), Vector(p1)
        d = b - a
        # With rounded ends the flat caps would poke through the spheres at the joints.
        new = bmesh.ops.create_cone(self.bm, cap_ends=not caps, segments=segments, radius1=r0, radius2=r1, depth=d.length)
        verts = new["verts"]
        rot = d.normalized().to_track_quat("Z", "Y").to_matrix().to_4x4()
        bmesh.ops.transform(self.bm, matrix=Matrix.Translation((a + b) / 2) @ rot, verts=verts)
        self._tag(self._faces_of(verts), color)
        if caps:
            self.blob(tuple(a), r0 * 1.03, color, subdiv=2, wobble=0.0)
            self.blob(tuple(b), r1 * 1.03, color, subdiv=2, wobble=0.0)
        return verts

    def slab(self, profile, thickness, color, plane="YZ", offset=0.0):
        """A flat shape: a 2D outline (u, v) extruded `thickness` across the plane. In "YZ" the outline lies
        along Y (u) and Z (v) and is centred on x = offset; in "XZ" along X and Z, centred on y = offset."""
        front, back = [], []
        for u, v in profile:
            for side, out in ((-0.5, front), (0.5, back)):
                w = offset + side * thickness
                out.append(self.bm.verts.new((w, u, v) if plane == "YZ" else (u, w, v)))
        faces = [self.bm.faces.new(front), self.bm.faces.new(list(reversed(back)))]
        n = len(profile)
        for i in range(n):
            j = (i + 1) % n
            faces.append(self.bm.faces.new((front[i], back[i], back[j], front[j])))
        self._tag(faces, color)
        return front + back

    def plane_quad(self, corners, color):
        vs = [self.bm.verts.new(c) for c in corners]
        f = self.bm.faces.new(vs)
        self._tag([f], color)
        return vs

    # ------------------------------------------------------------------ helpers
    def _place(self, verts, center, rot_z):
        m = Matrix.Translation(Vector(center)) @ Matrix.Rotation(rot_z, 4, "Z")
        bmesh.ops.transform(self.bm, matrix=m, verts=verts)

    def _faces_of(self, verts):
        vset = set(verts)
        return [f for f in self.bm.faces if all(v in vset for v in f.verts) and f not in self.colors]

    # ------------------------------------------------------------------ output
    def build(self):
        """Creates the Blender object with painted corner colours; returns it."""
        self.bm.verts.ensure_lookup_table()
        zs = [v.co.z for v in self.bm.verts] or [0.0]
        z0, z1 = min(zs), max(zs)
        self.height = max(z1 - z0, 0.01)
        mesh = bpy.data.meshes.new(self.name)
        # Remember colours by face position before writing (bmesh face identity is lost on to_mesh).
        order = list(self.bm.faces)
        colors = [self.colors.get(f, (0.8, 0.8, 0.8)) for f in order]
        team = [f in self.team for f in order]
        spans = [self.part_z.get(f, (z0, z1)) for f in order]
        pats = [self.pattern.get(f, "plain") for f in order]
        self.bm.to_mesh(mesh)
        self.bm.free()
        attr = mesh.color_attributes.new(name="Col", type="BYTE_COLOR", domain="CORNER")
        w = self.part_shade
        uv = mesh.uv_layers.new(name="UVMap")
        for poly, base, (p0, p1), pat in zip(mesh.polygons, colors, spans, pats):
            # The pattern's tile in the alpha; UVs by box projection on the face's main axis, in metres.
            alpha = PATTERNS.index(pat) / float(GRID * GRID - 1)
            rep = REPEAT[pat] * self.scale
            n = poly.normal
            axis = max(range(3), key=lambda k: abs(n[k]))
            for li in poly.loop_indices:
                co = mesh.vertices[mesh.loops[li].vertex_index].co
                u, v = (co.y, co.z) if axis == 0 else (co.x, co.z) if axis == 1 else (co.x, co.y)
                uv.data[li].uv = (u / rep, v / rep)
                z = mesh.vertices[mesh.loops[li].vertex_index].co.z
                t = (z - z0) / self.height
                tp = (z - p0) / max(p1 - p0, 0.01)
                # Painted light: darker at the foot, brighter at the crown (of the piece, and with part_shade of
                # each solid); faces turned up glow a little.
                grad = (1.0 - w) * t + w * tp
                shade = self.shade_low + self.shade_range * grad + (0.08 if poly.normal.z > 0.6 else 0.0) - (0.06 if poly.normal.z < -0.6 else 0.0)
                attr.data[li].color = (min(1, base[0] * shade), min(1, base[1] * shade), min(1, base[2] * shade), alpha)
        for poly in mesh.polygons:
            poly.use_smooth = self.smooth
        # Slot 0: the piece's own colours; slot 1 (only when used): team colour.
        mesh.materials.append(bpy.data.materials.new("Base"))
        if any(team):
            mesh.materials.append(bpy.data.materials.new("Team"))
            for poly, t in zip(mesh.polygons, team):
                poly.material_index = 1 if t else 0
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def export_fbx(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        object_types={"MESH"},
        mesh_smooth_type="FACE",
        colors_type="SRGB",
        apply_unit_scale=True,
        bake_space_transform=True,
        add_leaf_bones=False,
    )


def ring(n, radius, z=0.0, start=0.0):
    """Points around a circle."""
    return [(radius * math.cos(start + 2 * math.pi * i / n), radius * math.sin(start + 2 * math.pi * i / n), z) for i in range(n)]
