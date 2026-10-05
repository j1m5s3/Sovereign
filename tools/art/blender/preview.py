"""Renders a contact sheet of a kit for review (workbench, vertex colours):

  blender -b --python tools/art/blender/preview.py -- <out.png> <kit>
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402

import generate  # noqa: E402,F401  (registers kits; its main() exports nothing without args)
import kitlib  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
out, kit = os.path.abspath(args[0]), args[1]
module = generate.KITS[kit]

kitlib.reset_scene()
x = 0.0
objs = []
for make in module.PIECES:
    piece = make()
    obj = piece.build()
    width = max(v.co.x for v in obj.data.vertices) - min(v.co.x for v in obj.data.vertices)
    obj.location.x = x + width / 2
    x += width + 3.0
    objs.append(obj)

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "VERTEX"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.render.resolution_x, scene.render.resolution_y = 2400, 900
scene.render.film_transparent = False
world = bpy.data.worlds.new("w")
scene.world = world
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = x * 1.02
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
cam.location = (x / 2, -x * 0.9, x * 0.45)
cam.rotation_euler = (math.radians(68), 0, 0)
scene.camera = cam
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print("preview written", out)
