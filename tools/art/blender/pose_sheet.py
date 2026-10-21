"""Renders contact sheets of the rigged people's one-shot actions (rig_actions.py) for review, headless (Workbench,
vertex colours, as preview.py): per figure, one row per take (Strike, Hit, Death, DeathBack), each of its key frames
(FRAMES) from the side (from the figure's +X side and a little above, so it faces left) and then three-quarter (from
its front and +X side), on a grey ground just below z 0 (at -0.2 cm, so a figure lying at 0 shows) with a ruler every
50 cm along it. With --every, each take's every frame instead, three-quarter only, EVERY to a row. The checks
(check_rigs.py) catch numbers; the sheet catches a spear held like a broom.

  blender -b --factory-startup --python tools/art/blender/pose_sheet.py -- [--every] art/build/poses.png Soldier Archer ...
  python -I tools/art/blender/pose_sheet.py [--every] art/build/poses.png Soldier Archer ...      (pip's bpy)

One figure writes the file named; several write one each, <name>_<Figure>.png beside it.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

import rig_actions  # noqa: E402
import rig_figures  # noqa: E402

# take: the frames shown (wind-up, a spear levelled or a bow drawn, blow, follow-through, the way back (17, 20) and the
# end of a Strike; a Hit's peak; a fall's keys: the recoil at 6, what the hands hold tipping over at 11, the crouch or
# stagger at 14, the knees coming down at 19 (and 17 and 22 either side) and the landing at 24 (Death), or the seat at
# 21 (and 17, 19 and 23 about it) and the landing at 26 (DeathBack), and the rest).
FRAMES = {"Strike": (0, 6, 8, 10, 13, 17, 20, 23), "Hit": (0, 2, 5, 8), "Death": (0, 6, 11, 14, 17, 19, 22, 24, 35),
          "DeathBack": (0, 6, 11, 14, 17, 19, 21, 23, 26, 35)}
EVERY = 13      # frames to a row with --every
CELL = 300      # px, each view
SPAN = 360.0    # cm the camera shows across
# cm: what the camera looks at in each take (a strike reaches ahead of the figure, -Y; a fall lies ahead or behind).
CENTRE = {"Strike": (0.0, -70.0, 100.0), "Hit": (0.0, -40.0, 100.0), "Death": (0.0, -60.0, 70.0), "DeathBack": (0.0, 30.0, 70.0)}
VIEWS = {"side": (math.radians(82), 0.0, math.radians(90)), "three-quarter": (math.radians(75), 0.0, math.radians(35))}


def _camera(scene):
    data = bpy.data.cameras.new("sheet")
    data.type = "ORTHO"
    data.ortho_scale = SPAN
    data.clip_start, data.clip_end = 1.0, 5000.0
    cam = bpy.data.objects.new("sheet", data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam


def _ground(scene):
    """A grey floor just below z 0 (at -0.2 cm), and dark marks across it every 50 cm along Y (forward is -Y) to read
    distances by."""
    import bmesh

    mesh = bpy.data.meshes.new("ground")
    bm = bmesh.new()
    colour = bm.loops.layers.color.new("Col")

    def quad(x0, x1, y0, y1, z, rgb):
        vs = [bm.verts.new((x, y, z)) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
        face = bm.faces.new(vs)
        for loop in face.loops:
            loop[colour] = (*rgb, 1.0)

    quad(-300, 300, -400, 300, -0.2, (0.62, 0.62, 0.6))
    for y in range(-350, 300, 50):
        quad(-300, 300, y - 0.8, y + 0.8, -0.1, (0.25, 0.25, 0.25) if y else (0.7, 0.15, 0.1))
    bm.to_mesh(mesh)
    bm.free()
    mesh.color_attributes.active_color = mesh.color_attributes["Col"]
    mesh.color_attributes.render_color_index = mesh.color_attributes.active_color_index
    obj = bpy.data.objects.new("ground", mesh)
    scene.collection.objects.link(obj)
    return obj


def _render(scene, path):
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    image = bpy.data.images.load(path)
    w, h = image.size
    px = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)
    bpy.data.images.remove(image)
    os.remove(path)
    return px


def sheet(name, out, every=False):
    arm, body = rig_figures.rig(name, rig_figures.PEOPLE[name])
    takes = [t for t in FRAMES if bpy.data.actions.get(t)]
    if not takes:
        why = rig_actions.ERRORS.get(name)
        print("pose_sheet: %s has no one-shot actions (%s)" % (name, "they could not be keyed: " + why if why else "it never fights"))
        return
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "VERTEX"
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.render.resolution_x = scene.render.resolution_y = CELL
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    scene.world = bpy.data.worlds.new("sheet")
    scene.world.color = (0.82, 0.86, 0.9)
    cam = _camera(scene)
    _ground(scene)
    # The sheet's rows, each a take and its cells (frame, view): the take's key frames from each view, or (every) its
    # every frame three-quarter, EVERY to a row.
    rows = []
    for take in takes:
        if every:
            cells = [(f, "three-quarter") for f in range(rig_actions.ONCE[take]["frames"] + 1)]
            rows += [(take, cells[i:i + EVERY]) for i in range(0, len(cells), EVERY)]
        else:
            rows.append((take, [(f, view) for f in FRAMES[take] for view in VIEWS]))
    cols = max(len(cells) for _, cells in rows)
    canvas = np.ones((len(rows) * CELL, cols * CELL, 4), dtype=np.float32)
    tmp = out + ".cell.png"
    for row, (take, cells) in enumerate(rows):
        rig_actions.play(arm, bpy.data.actions[take])
        y0 = (len(rows) - 1 - row) * CELL  # (images come bottom row first; the sheet's first row is its top)
        for k, (frame, view) in enumerate(cells):
            scene.frame_set(frame)
            # Ortho, from far enough off along the view's own back axis that nothing is clipped.
            cam.rotation_euler = VIEWS[view]
            back = cam.rotation_euler.to_matrix() @ Vector((0.0, 0.0, 1.0))
            cam.location = tuple(c + 1000.0 * b for c, b in zip(CENTRE[take], back))
            px = _render(scene, tmp)
            x0 = k * CELL
            canvas[y0:y0 + CELL, x0:x0 + CELL] = px
            canvas[y0:y0 + CELL, x0 + CELL - 1] = (0.3, 0.3, 0.3, 1.0)  # a hairline between cells
        canvas[y0, :] = (0.3, 0.3, 0.3, 1.0)
    image = bpy.data.images.new("sheet", cols * CELL, len(rows) * CELL, alpha=True)
    image.pixels = canvas.ravel()
    image.filepath_raw = out
    image.file_format = "PNG"
    image.save()
    shown = ", ".join(takes) + " (every frame)" if every else ", ".join("%s %s" % (t, list(FRAMES[t])) for t in takes)
    print("pose_sheet: %s -> %s (%s)" % (name, out, shown))


def main():
    under_blender = any(a in sys.argv for a in ("--python", "-P"))
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ([] if under_blender else sys.argv[1:])
    every = "--every" in args
    args = [a for a in args if a != "--every"]
    out = os.path.abspath(args[0] if args else os.path.join("art", "build", "poses.png"))
    names = args[1:] or ["Soldier"]
    os.makedirs(os.path.dirname(out), exist_ok=True)
    stem, ext = os.path.splitext(out)
    for name in names:
        sheet(name, out if len(names) == 1 else "%s_%s%s" % (stem, name, ext), every)


if __name__ == "__main__":
    main()
