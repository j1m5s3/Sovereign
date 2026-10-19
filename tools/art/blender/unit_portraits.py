"""Portraits of the units for the unit panel (Civ VI shows one beside the selected unit): each Figures kit piece the
map draws a unit as (SovUnitFigure, SovCultureFigure), lit as portrait.py lights them, framed on the upper body for
people and whole for machines and ships, on a transparent ground, the team cloth in a neutral steel blue.

  blender -b --factory-startup --python tools/art/blender/unit_portraits.py -- unreal/Content/Slate/Portraits

Writes <out>/<Figure>.png (Soldier, Archer_Asian, Tank...), 160 px square.
"""
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit_figures  # noqa: E402
import kitlib  # noqa: E402
import look  # noqa: E402
import textures  # noqa: E402

# The map's figure names (SovMirror.cpp) and the kit function each is built by.
FIGURES = {"Soldier": "soldier", "Archer": "archer", "Rider": "rider", "Siege": "siege", "Musketeer": "musketeer",
           "Rifleman": "rifleman", "Tank": "tank", "Plane": "plane", "Ship": "ship", "Steamship": "steamship",
           "Citizen": "citizen", "Leader": "leader"}
for arm in ("Soldier", "Archer", "Rider", "Musketeer"):
    for style in ("MiddleEast", "Asian", "African", "American"):
        FIGURES["%s_%s" % (arm, style)] = "%s_%s" % (arm.lower(), style.lower())
PEOPLE = {"Soldier", "Archer", "Musketeer", "Rifleman", "Citizen", "Leader"}
SIZE = 160
TEAM = kitlib.srgb("#5a6f9a")


def light(scene, name, energy, rot, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, "SUN")
    d.energy = energy
    d.color = color
    d.angle = math.radians(10)
    o = bpy.data.objects.new(name, d)
    o.rotation_euler = tuple(math.radians(a) for a in rot)
    scene.collection.objects.link(o)


def render(name, func, out, sheets):
    # A clean stage, keeping the detail sheets (kitlib.reset_scene would reload the factory file and lose them).
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
    light(scene, "key", 2.4, (50, 0, -40), (1.0, 0.93, 0.82))
    light(scene, "fill", 0.6, (70, 0, 140), (0.6, 0.7, 1.0))
    light(scene, "rim", 2.0, (115, 0, 160), (1.0, 0.88, 0.7))
    obj = getattr(kit_figures, func)().build()
    obj.data.materials[0] = look.material("Base", (1, 1, 1), sheets)
    if len(obj.data.materials) > 1:
        obj.data.materials[1] = look.material("Team", TEAM, sheets)
    vs = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    cam_data = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    arm = name.split("_")[0]
    if arm in PEOPLE:
        # Head and shoulders, by the head's height (not the top of the piece: spears and muskets reach far above it).
        cam_data.lens = 85
        target = Vector((0.0, 0.0, 1.52))
        dist = 2.5
    elif arm == "Rider":
        # The rider and the horse's head and shoulders.
        cam_data.lens = 70
        target = Vector(((lo.x + hi.x) * 0.5, 0.0, 1.55))
        dist = 4.2
    else:
        # The whole machine or ship, three-quarter on.
        cam_data.lens = 50
        target = (lo + hi) * 0.5
        dist = max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z) * 1.35
    cam.location = target + Vector((dist * 0.35, -dist, dist * 0.12))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    path = os.path.join(out, name + ".png")
    scene.render.filepath = path
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    bpy.ops.render.render(write_still=True)
    # Down to SIZE (rendered at twice that for clean edges).
    img = bpy.data.images.load(path)
    img.scale(SIZE, SIZE)
    img.save()
    print("portrait %s -> %s" % (name, path))


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(args[0] if args else os.path.join("unreal", "Content", "Slate", "Portraits"))
    os.makedirs(out, exist_ok=True)
    kitlib.reset_scene()
    sheets = textures.save(os.path.join(tempfile.gettempdir(), "T_SovDetail_portraits.png"))
    for name, func in FIGURES.items():
        render(name, func, out, sheets)


if __name__ == "__main__":
    main()
