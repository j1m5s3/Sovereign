"""Views of each architectural style's city centre for the new-game screen (what a civ's cities will look like, as
Civ VI shows its leaders): the style's palace with three houses before it on a patch of grass, lit as portrait.py
lights the kits.

  blender -b --factory-startup --python tools/art/blender/style_views.py -- unreal/Content/Slate/Styles

Writes <out>/<Style>.png (Mediterranean, European, MiddleEast, Asian, African, American: SovArt::CivStyle), 640 x 360.
"""
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit_classical  # noqa: E402
import kit_styles  # noqa: E402
import kit_towns  # noqa: E402
import kitlib  # noqa: E402
import look  # noqa: E402
import textures  # noqa: E402

TEAM = kitlib.srgb("#9a2f2f")


def pieces(style):
    """The style's palace and three houses (the kit functions)."""
    if style == "Mediterranean":
        return kit_classical.palace, [kit_classical.house_a, kit_classical.house_b, kit_classical.house_c]
    if style == "European":
        return kit_towns.medieval_palace, [kit_towns.medieval_house_a, kit_towns.medieval_house_b, kit_towns.medieval_house_c]
    key = style.lower()
    return getattr(kit_styles, key + "_palace"), [getattr(kit_styles, "%s_house_%s" % (key, k)) for k in "abc"]


STYLES = ["Mediterranean", "European", "MiddleEast", "Asian", "African", "American"]
# Where the houses stand before the palace (x, y metres; the camera looks from -y), and how each is turned.
SPOTS = [(-12.0, -6.0, 0.35), (11.5, -5.0, -0.4), (-2.0, -12.5, 0.1)]


def light(scene, name, energy, rot, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, "SUN")
    d.energy = energy
    d.color = color
    d.angle = math.radians(8)
    o = bpy.data.objects.new(name, d)
    o.rotation_euler = tuple(math.radians(a) for a in rot)
    scene.collection.objects.link(o)


def render(style, out, sheets):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.68, 0.85, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.7
    scene.world = world
    light(scene, "sun", 3.0, (48, 0, -35), (1.0, 0.93, 0.82))
    light(scene, "fill", 0.5, (70, 0, 140), (0.6, 0.7, 1.0))
    base, team = look.material("Base", (1, 1, 1), sheets), look.material("Team", TEAM, sheets)

    def place(make, x, y, rot):
        obj = make().build()
        obj.data.materials[0] = base
        if len(obj.data.materials) > 1:
            obj.data.materials[1] = team
        obj.location = (x, y, 0)
        obj.rotation_euler = (0, 0, rot)
        return obj

    palace, houses = pieces(style)
    objs = [place(palace, 0.0, 4.0, 0.0)]
    for make, (x, y, r) in zip(houses, SPOTS):
        objs.append(place(make, x, y, r))
    # Grass under them, a little rough at the edges of the view.
    bpy.ops.mesh.primitive_circle_add(vertices=48, radius=400.0, fill_type="NGON", location=(0, 0, -0.02))
    ground = bpy.context.active_object
    g = bpy.data.materials.new("Grass")
    g.use_nodes = True
    g.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.13, 0.22, 0.06, 1)
    g.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
    ground.data.materials.append(g)

    vs = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    centre = sum(vs, Vector()) / len(vs)
    target = Vector((centre.x, centre.y, max(v.z for v in vs) * 0.35))
    radius = max((v - target).length for v in vs)
    cam_data = bpy.data.cameras.new("cam")
    cam_data.lens = 45
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    # Far enough that the whole group fits the frame's height (16:9 on a 36 mm sensor), from the front-right, above.
    half = math.atan(36.0 * 9.0 / 16.0 / 2.0 / cam_data.lens)
    dist = radius / math.sin(half) * 0.7  # the sphere is generous: the group sits low and wide in it
    view = Vector((0.3, -1.0, 0.62)).normalized()
    cam.location = target + view * dist
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    path = os.path.join(out, style + ".png")
    scene.render.filepath = path
    scene.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path)
    img.scale(640, 360)
    img.save()
    print("style view %s -> %s" % (style, path))


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(args[0] if args else os.path.join("unreal", "Content", "Slate", "Styles"))
    os.makedirs(out, exist_ok=True)
    kitlib.reset_scene()
    sheets = textures.save(os.path.join(tempfile.gettempdir(), "T_SovDetail_styles.png"))
    for style in STYLES:
        render(style, out, sheets)


if __name__ == "__main__":
    main()
