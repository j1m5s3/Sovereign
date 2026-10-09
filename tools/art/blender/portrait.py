"""Renders the Figures kit's people as a lit portrait for review (Eevee; team cloth tinted as in game):

  blender -b --python tools/art/blender/portrait.py -- <out.png> [team #RRGGBB] [close]

`close` frames the upper bodies (faces and costume detail); otherwise whole figures.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402

import kit_figures  # noqa: E402
import kitlib  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
out = os.path.abspath(args[0])
team = kitlib.srgb(args[1]) if len(args) > 1 and args[1].startswith("#") else kitlib.srgb("#2f4f9a")
close = "close" in args

kitlib.reset_scene()
scene = bpy.context.scene


def material(name, tint):
    """Vertex colour (the painted look) times a tint, as the game's M_SovKit does."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nodes, links = m.node_tree.nodes, m.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    col = nodes.new("ShaderNodeVertexColor")
    col.layer_name = "Col"
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    links.new(col.outputs["Color"], mix.inputs["A"])
    mix.inputs["B"].default_value = (tint[0], tint[1], tint[2], 1.0)
    links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.75
    return m


base_mat, team_mat = material("Base", (1, 1, 1)), material("Team", team)
gap = 0.95
people = [kit_figures.captain, kit_figures.soldier, kit_figures.leader] if close else kit_figures.PEOPLE
for i, make in enumerate(people):
    obj = make().build()
    obj.data.materials[0] = base_mat
    if len(obj.data.materials) > 1:
        obj.data.materials[1] = team_mat
    obj.location.x = (i - (len(people) - 1) / 2) * gap

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


def light(name, kind, energy, rot, color=(1, 1, 1), size=1.0):
    d = bpy.data.lights.new(name, kind)
    d.energy = energy
    d.color = color
    if kind == "SUN":
        d.angle = math.radians(size * 10)
    o = bpy.data.objects.new(name, d)
    o.rotation_euler = tuple(math.radians(a) for a in rot)
    scene.collection.objects.link(o)


light("key", "SUN", 2.2, (50, 0, -40), (1.0, 0.93, 0.82))
light("fill", "SUN", 0.45, (70, 0, 140), (0.6, 0.7, 1.0))
light("rim", "SUN", 2.2, (115, 0, 160), (1.0, 0.88, 0.7))

cam_data = bpy.data.cameras.new("cam")
cam_data.lens = 85 if close else 50
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
if close:
    cam.location = (0.35, -7.3, 1.5)
    cam.rotation_euler = (math.radians(88), 0, math.radians(3))
else:
    cam.location = (0.7, -7.2, 1.3)
    cam.rotation_euler = (math.radians(86), 0, math.radians(5.5))
scene.camera = cam
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print("portrait written", out)
