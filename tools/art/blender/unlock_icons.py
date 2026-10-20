"""Icons of what a tech or civic unlocks, for the tech and civic trees (as Civ VI shows them on each node): every
district, improvement, wonder and city-centre landmark piece of the kits, three-quarter on, on a transparent ground,
96 px. Units use the unit portraits (unit_portraits.py) instead.

  blender -b --factory-startup --python tools/art/blender/unlock_icons.py -- unreal/Content/Slate/Unlocks

Writes <out>/<Kit>_<Piece>.png (Districts_Campus, Fields_Farm, Landmarks_Colosseum, Wonders_Pyramid, Classical_Granary).
"""
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit_classical  # noqa: E402
import kit_districts  # noqa: E402
import kit_fields  # noqa: E402
import kit_landmarks  # noqa: E402
import kit_wonders  # noqa: E402
import kitlib  # noqa: E402
import look  # noqa: E402
import textures  # noqa: E402

SIZE = 96
TEAM = kitlib.srgb("#5a6f9a")
CLASSICAL = [kit_classical.palace, kit_classical.monument, kit_classical.granary, kit_classical.temple, kit_classical.landmark_generic,
             kit_classical.wall_segment]
KITS = {"Districts": kit_districts.PIECES, "Fields": kit_fields.PIECES, "Landmarks": kit_landmarks.PIECES,
        "Wonders": kit_wonders.PIECES, "Classical": CLASSICAL}


def light(scene, name, energy, rot, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, "SUN")
    d.energy = energy
    d.color = color
    d.angle = math.radians(10)
    o = bpy.data.objects.new(name, d)
    o.rotation_euler = tuple(math.radians(a) for a in rot)
    scene.collection.objects.link(o)


def render(kit, make, out, sheets):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = SIZE * 2
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.12, 0.13, 0.16, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
    scene.world = world
    light(scene, "key", 2.6, (50, 0, -40), (1.0, 0.93, 0.82))
    light(scene, "fill", 0.6, (70, 0, 140), (0.6, 0.7, 1.0))
    piece = make()
    obj = piece.build()
    obj.data.materials[0] = look.material("Base", (1, 1, 1), sheets)
    if len(obj.data.materials) > 1:
        obj.data.materials[1] = look.material("Team", TEAM, sheets)
    vs = [v.co for v in obj.data.vertices]
    centre = sum(vs, Vector()) / len(vs)
    radius = max((v - centre).length for v in vs)
    cam_data = bpy.data.cameras.new("cam")
    cam_data.lens = 50
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    half = math.atan(36.0 / 2.0 / cam_data.lens)
    view = Vector((0.55, -1.0, 0.75)).normalized()
    cam.location = centre + view * (radius / math.sin(half) * 0.82)
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    path = os.path.join(out, "%s_%s.png" % (kit, piece.name[3:]))  # SM_Campus -> Districts_Campus
    scene.render.filepath = path
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path)
    img.scale(SIZE, SIZE)
    img.save()
    print("unlock icon", path)


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(args[0] if args else os.path.join("unreal", "Content", "Slate", "Unlocks"))
    os.makedirs(out, exist_ok=True)
    kitlib.reset_scene()
    sheets = textures.save(os.path.join(tempfile.gettempdir(), "T_SovDetail_unlocks.png"))
    for kit, makers in KITS.items():
        for make in makers:
            render(kit, make, out, sheets)


if __name__ == "__main__":
    main()
