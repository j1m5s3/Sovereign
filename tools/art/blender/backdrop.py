"""The main menu's backdrop: a walled Classical city on a rise above a river at golden hour, built from the kits
and rendered (Eevee) to unreal/Content/Slate/Backdrop.png, which the menu shows behind its panel.

  blender -b --factory-startup --python tools/art/blender/backdrop.py -- unreal/Content/Slate/Backdrop.png

The city stands right of centre and the river runs out to the left, so the menu panel in the middle covers
open country rather than the town.
"""
import math
import os
import random
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402

import kit_classical  # noqa: E402
import kit_figures  # noqa: E402
import kit_nature  # noqa: E402
import kitlib  # noqa: E402
import look  # noqa: E402
import textures  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
out = os.path.abspath(args[0] if args else "Backdrop.png")
width, height = (int(args[1]), int(args[2])) if len(args) > 2 else (1920, 1080)

kitlib.reset_scene()
scene = bpy.context.scene
sheet = textures.save(os.path.join(tempfile.gettempdir(), "T_SovDetail_backdrop.png"))
rng = random.Random(7)
TEAM = kitlib.srgb("#9a2f2f")
base_mat, team_mat = look.material("Base", (1, 1, 1), sheet), look.material("Team", TEAM, sheet)

CITY = (38.0, 40.0)  # the town's centre (x right, y away from the camera)


def ground_height(x, y):
    """Rolling country, a rise under the town, and the river's valley curving out to the left."""
    h = 0.9 * math.sin(x * 0.07) * math.cos(y * 0.05) + 0.5 * math.sin(x * 0.17 + 1.3) * math.sin(y * 0.11)
    d = math.hypot(x - CITY[0], y - CITY[1])
    h += 3.2 * math.exp(-(d / 15.0) ** 2)
    h += 0.05 * max(0.0, y - 60.0)  # far hills
    h += 6.0 * math.exp(-((y - 120.0) / 25.0) ** 2) * (0.6 + 0.4 * math.sin(x * 0.05))
    river = abs(y - (22.0 + 9.0 * math.sin(x * 0.045) - 0.12 * x))
    h -= 2.2 * math.exp(-(river / 5.0) ** 2)
    return h


def ground_color(x, y, z):
    """Grass from deep green in the hollows to straw on the tops; earth near the river's banks."""
    g = min(1.0, max(0.0, (z + 1.0) / 5.0))
    grass = (0.20 + 0.25 * g, 0.33 + 0.12 * g, 0.10 + 0.05 * g)
    n = 0.06 * math.sin(x * 0.9 + y * 0.4) * math.cos(y * 0.7 - x * 0.3)
    if z < -0.6:
        return (0.42 + n, 0.36 + n, 0.25 + n)
    return (grass[0] + n, grass[1] + n, grass[2] + n * 0.5)


def build_ground():
    n, size_x, size_y = 220, 260.0, 200.0
    verts, faces, cols = [], [], []
    for j in range(n + 1):
        for i in range(n + 1):
            x = -size_x / 2 + size_x * i / n
            y = -20.0 + size_y * j / n
            verts.append((x, y, ground_height(x, y)))
    for j in range(n):
        for i in range(n):
            a = j * (n + 1) + i
            faces.append((a, a + 1, a + n + 2, a + n + 1))
    mesh = bpy.data.meshes.new("Ground")
    mesh.from_pydata(verts, [], faces)
    attr = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            x, y, z = verts[mesh.loops[li].vertex_index]
            r, g, b = ground_color(x, y, z)
            attr.data[li].color_srgb = (r, g, b, 6 / 15.0)  # grass tile of the detail sheet
    uv = mesh.uv_layers.new(name="UVMap")
    for li, loop in enumerate(mesh.loops):
        x, y, _ = verts[loop.vertex_index]
        uv.data[li].uv = (x / 1.7, y / 1.7)
    for p in mesh.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new("Ground", mesh)
    obj.data.materials.append(base_mat)
    scene.collection.objects.link(obj)


def build_water():
    bpy.ops.mesh.primitive_plane_add(size=400, location=(0, 80, -0.9))
    water = bpy.context.active_object
    m = bpy.data.materials.new("Water")
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (0.03, 0.09, 0.13, 1)
    bsdf.inputs["Roughness"].default_value = 0.25
    water.data.materials.append(m)


_made = {}


def place(make, x, y, rot=0.0, scale=1.0, sink=0.05):
    """A kit piece (built once, then linked copies) standing on the ground at (x, y)."""
    if make not in _made:
        obj = make().build()
        obj.data.materials[0] = base_mat
        if len(obj.data.materials) > 1:
            obj.data.materials[1] = team_mat
        scene.collection.objects.unlink(obj) if obj.name in scene.collection.objects else None
        _made[make] = obj.data
    o = bpy.data.objects.new(make.__name__, _made[make])
    o.location = (x, y, ground_height(x, y) - sink)
    o.rotation_euler = (0, 0, rot)
    o.scale = (scale, scale, scale)
    scene.collection.objects.link(o)
    return o


def build_town():
    cx, cy = CITY
    C = kit_classical
    place(C.palace, cx, cy + 2.0, rot=math.radians(-12))
    place(C.temple, cx - 9.0, cy + 7.0, rot=math.radians(-20))
    place(C.landmark_generic, cx + 10.0, cy + 9.0, rot=math.radians(5))
    place(C.granary, cx + 8.5, cy - 3.0, rot=math.radians(30))
    place(C.monument, cx - 2.5, cy - 6.0)
    for k in range(4):
        place(C.market_stall, cx - 6.0 + 2.6 * k, cy - 9.0 + 0.4 * k, rot=math.radians(-8 + 6 * k))
    houses = [C.house_a, C.house_b, C.house_c]
    for k in range(34):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(7.0, 15.5)
        x, y = cx + r * math.cos(a) * 1.15, cy + r * math.sin(a)
        if abs(x - (cx - 2.5)) < 3 and abs(y - (cy - 7)) < 4:
            continue
        place(rng.choice(houses), x, y, rot=rng.uniform(0, 2 * math.pi), scale=rng.uniform(0.9, 1.1))
    # The wall round the town, with banners at its gates.
    n = 40
    for k in range(n):
        a = 2 * math.pi * k / n
        if k in (28, 29, 30):  # the gate toward the river
            continue
        x, y = cx + 20.0 * math.cos(a) * 1.15, cy + 20.0 * math.sin(a)
        place(C.wall_segment, x, y, rot=a + math.pi / 2, scale=0.55)
    for k in (27, 31):
        a = 2 * math.pi * k / n
        place(C.banner, cx + 20.0 * math.cos(a) * 1.15, cy + 20.0 * math.sin(a), rot=a)


def build_country():
    N = kit_nature
    for k in range(260):
        x, y = rng.uniform(-120, 120), rng.uniform(-5, 150)
        if math.hypot(x - CITY[0], y - CITY[1]) < 27 or ground_height(x, y) < -0.7:
            continue
        if rng.random() < 0.45 and not (-25 < x < 20 and y < 30):  # woods, thinner in the middle foreground
            place(rng.choice([N.tree_broadleaf, N.tree_broadleaf, N.tree_conifer]), x, y, rot=rng.uniform(0, 6.3), scale=rng.uniform(1.0, 1.5))
        elif rng.random() < 0.3:
            place(N.bush, x, y, rot=rng.uniform(0, 6.3), scale=rng.uniform(1.0, 1.6))
    for k in range(10):
        x, y = rng.uniform(-60, 0), rng.uniform(40, 90)
        place(N.rocks, x, y, rot=rng.uniform(0, 6.3), scale=rng.uniform(1.5, 3))
    # Reeds along the river's near bank.
    for k in range(40):
        x = rng.uniform(-90, 60)
        y = 22.0 + 9.0 * math.sin(x * 0.045) - 0.12 * x - rng.uniform(3.5, 6.0)
        place(N.reeds, x, y, rot=rng.uniform(0, 6.3), scale=rng.uniform(0.8, 1.3))


def build_people():
    F = kit_figures
    # A column of soldiers marching up the road to the gate, and townsfolk outside it.
    for k in range(9):
        x, y = CITY[0] - 4.0 - 1.1 * (k % 3), CITY[1] - 24.0 - 1.6 * (k // 3)
        place(F.soldier, x, y, rot=math.radians(180 + 10), scale=1.0)
    for k in range(5):
        x, y = CITY[0] + 3 + rng.uniform(-6, 6), CITY[1] - 22 + rng.uniform(-3, 3)
        place(rng.choice([F.citizen, F.herald]), x, y, rot=rng.uniform(0, 6.3))
    place(F.rider, CITY[0] - 14.0, CITY[1] - 22.0, rot=math.radians(60))
    place(F.ship, -40.0, 21.0 - 9.0 * math.sin(-40 * 0.045) + 0.0, rot=math.radians(10), scale=1.0, sink=0.6)


def lights_and_camera():
    try:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = width, height
    scene.view_settings.view_transform = "AgX" if "AgX" in [i.identifier for i in scene.view_settings.bl_rna.properties["view_transform"].enum_items] else "Filmic"
    world = bpy.data.worlds.new("Sky")
    world.use_nodes = True
    nodes, links = world.node_tree.nodes, world.node_tree.links
    bg = nodes["Background"]
    tc = nodes.new("ShaderNodeTexCoord")
    sep = nodes.new("ShaderNodeSeparateXYZ")
    ramp = nodes.new("ShaderNodeValToRGB")
    links.new(tc.outputs["Window"], sep.inputs[0])
    links.new(sep.outputs["Y"], ramp.inputs["Fac"])
    # Graded up the frame (the window's Y): warm at the horizon, blue at the top.
    ramp.color_ramp.elements[0].position = 0.78
    ramp.color_ramp.elements[0].color = (1.0, 0.55, 0.25, 1)
    mid = ramp.color_ramp.elements.new(0.86)
    mid.color = (0.8, 0.6, 0.55, 1)
    ramp.color_ramp.elements[2].position = 1.0
    ramp.color_ramp.elements[2].color = (0.18, 0.32, 0.58, 1)
    links.new(ramp.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 1.0
    scene.world = world
    sun = bpy.data.lights.new("Sun", "SUN")
    sun.energy = 3.2
    sun.color = (1.0, 0.82, 0.6)
    sun.angle = math.radians(3)
    so = bpy.data.objects.new("Sun", sun)
    so.rotation_euler = (math.radians(72), 0, math.radians(-135))
    scene.collection.objects.link(so)
    fill = bpy.data.lights.new("Fill", "SUN")
    fill.energy = 0.5
    fill.color = (0.6, 0.7, 1.0)
    fo = bpy.data.objects.new("Fill", fill)
    fo.rotation_euler = (math.radians(50), 0, math.radians(60))
    scene.collection.objects.link(fo)
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.lens = 28
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (-8.0, -26.0, 26.0)
    target = (14.0, 48.0, 0.0)
    from mathutils import Vector
    cam.rotation_euler = (Vector(target) - Vector(cam.location)).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    try:
        scene.eevee.use_shadows = True
    except AttributeError:
        pass


build_ground()
build_water()
build_town()
build_country()
build_people()
lights_and_camera()
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print("backdrop written", out)
