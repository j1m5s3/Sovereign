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
        self.colors = {}  # face index -> (r, g, b)
        self.rng = random.Random(seed)
        self.height = 1.0

    # ------------------------------------------------------------------ solids
    def _tag(self, faces, color, jitter=0.04):
        c = srgb(color)
        for f in faces:
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
        self.bm.to_mesh(mesh)
        self.bm.free()
        attr = mesh.color_attributes.new(name="Col", type="BYTE_COLOR", domain="CORNER")
        for poly, base in zip(mesh.polygons, colors):
            for li in poly.loop_indices:
                z = mesh.vertices[mesh.loops[li].vertex_index].co.z
                t = (z - z0) / self.height
                # Painted light: darker at the foot, brighter at the crown; faces turned up glow a little.
                shade = 0.72 + 0.36 * t + (0.08 if poly.normal.z > 0.6 else 0.0) - (0.06 if poly.normal.z < -0.6 else 0.0)
                attr.data[li].color = (min(1, base[0] * shade), min(1, base[1] * shade), min(1, base[2] * shade), 1.0)
        for poly in mesh.polygons:
            poly.use_smooth = False
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
