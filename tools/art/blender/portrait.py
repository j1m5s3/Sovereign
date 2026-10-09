"""Renders kit pieces lit for review (Eevee), with the game material's look: vertex colour x team tint x
the painted detail tile (textures.py) the vertex alpha picks.

  blender -b --python tools/art/blender/portrait.py -- <out.png> [team #RRGGBB] [close] [kit=Figures]

Figures: the people, whole or `close` (upper bodies of the captain, soldier and ruler). Other kits
(Classical, Nature): every piece in a row.
"""
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import generate  # noqa: E402
import kit_figures  # noqa: E402
import kitlib  # noqa: E402
import textures  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
out = os.path.abspath(args[0])
team = kitlib.srgb(args[1]) if len(args) > 1 and args[1].startswith("#") else kitlib.srgb("#2f4f9a")
close = "close" in args
kit = next((a.split("=", 1)[1] for a in args if a.startswith("kit=")), "Figures")

kitlib.reset_scene()
scene = bpy.context.scene
sheet = textures.save(os.path.join(tempfile.gettempdir(), "T_SovDetail_preview.png"))


def material(name, tint):
    """As M_SovKit: vertex colour x tint x 2 x the detail sheet at the alpha's tile."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nodes, links = m.node_tree.nodes, m.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    L = links.new

    def math_node(op, a, b=None):
        n = nodes.new("ShaderNodeMath")
        n.operation = op
        for i, v in enumerate((a, b)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                L(v, n.inputs[i])
        return n.outputs[0]

    col = nodes.new("ShaderNodeVertexColor")
    col.layer_name = "Col"
    idx = math_node("ROUND", math_node("MULTIPLY", col.outputs["Alpha"], 15.0))
    column = math_node("FLOORED_MODULO", idx, 4.0)
    row = math_node("FLOOR", math_node("MULTIPLY", idx, 0.25))
    uvmap = nodes.new("ShaderNodeUVMap")
    uvmap.uv_map = "UVMap"
    sep = nodes.new("ShaderNodeSeparateXYZ")
    L(uvmap.outputs["UV"], sep.inputs[0])
    fu = math_node("ADD", math_node("MULTIPLY", math_node("FRACT", sep.outputs["X"]), 0.9375), 0.03125)
    fv = math_node("ADD", math_node("MULTIPLY", math_node("FRACT", sep.outputs["Y"]), 0.9375), 0.03125)
    # Tile row 0 sits at the top of the sheet; Blender's V runs up from the bottom.
    au = math_node("MULTIPLY", math_node("ADD", column, fu), 0.25)
    av = math_node("MULTIPLY", math_node("ADD", math_node("SUBTRACT", 3.0, row), fv), 0.25)
    comb = nodes.new("ShaderNodeCombineXYZ")
    L(au, comb.inputs["X"])
    L(av, comb.inputs["Y"])
    img = nodes.new("ShaderNodeTexImage")
    img.image = sheet
    img.interpolation = "Linear"
    L(comb.outputs[0], img.inputs["Vector"])
    detail = math_node("MULTIPLY", img.outputs["Color"], 2.0)
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    L(col.outputs["Color"], mix.inputs["A"])
    mix.inputs["B"].default_value = (tint[0], tint[1], tint[2], 1.0)
    mix2 = nodes.new("ShaderNodeMix")
    mix2.data_type = "RGBA"
    mix2.blend_type = "MULTIPLY"
    mix2.inputs["Factor"].default_value = 1.0
    L(mix.outputs["Result"], mix2.inputs["A"])
    L(detail, mix2.inputs["B"])
    L(mix2.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.75
    return m


base_mat, team_mat = material("Base", (1, 1, 1)), material("Team", team)
if kit == "Figures":
    makers = [kit_figures.captain, kit_figures.soldier, kit_figures.leader] if close else kit_figures.PEOPLE
else:
    makers = generate.KITS[kit].PIECES
objs, x = [], 0.0
for make in makers:
    obj = make().build()
    obj.data.materials[0] = base_mat
    if len(obj.data.materials) > 1:
        obj.data.materials[1] = team_mat
    xs = [v.co.x for v in obj.data.vertices]
    width = max(xs) - min(xs) if kit != "Figures" else 0.95
    obj.location.x = x + (width / 2 - (max(xs) + min(xs)) / 2 if kit != "Figures" else 0.0)
    x += width + (1.5 if kit != "Figures" else 0.0)
    objs.append(obj)
span = x - (1.5 if kit != "Figures" else 0.0)
for o in objs:
    o.location.x -= span / 2 - (0.95 / 2 if kit == "Figures" else 0.0)

try:
    scene.render.engine = "BLENDER_EEVEE_NEXT"
except TypeError:
    scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = (2000, 900) if close else (2000, 1100)
world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.03, 0.05, 0.1, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
scene.world = world
scene.view_settings.view_transform = "Standard"


def light(name, energy, rot, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, "SUN")
    d.energy = energy
    d.color = color
    d.angle = math.radians(10)
    o = bpy.data.objects.new(name, d)
    o.rotation_euler = tuple(math.radians(a) for a in rot)
    scene.collection.objects.link(o)


light("key", 2.2, (50, 0, -40), (1.0, 0.93, 0.82))
light("fill", 0.45, (70, 0, 140), (0.6, 0.7, 1.0))
light("rim", 2.2, (115, 0, 160), (1.0, 0.88, 0.7))

cam_data = bpy.data.cameras.new("cam")
cam_data.lens = 85 if close else 50
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
if kit != "Figures":
    # Fit the row: a three-quarter view from the front, a little above.
    zs = [(o.matrix_world @ v.co).z for o in objs for v in o.data.vertices]
    top = max(zs)
    dist = max(span, top * 2.2) * 1.3
    target = Vector((0.0, 0.0, top * 0.4))
    cam.location = target + Vector((dist * 0.25, -dist, dist * 0.35))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
elif close:
    cam.location = (0.35, -7.3, 1.5)
    cam.rotation_euler = (math.radians(88), 0, math.radians(3))
else:
    cam.location = (0.7, -7.2, 1.3)
    cam.rotation_euler = (math.radians(86), 0, math.radians(5.5))
scene.camera = cam
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print("portrait written", out)
