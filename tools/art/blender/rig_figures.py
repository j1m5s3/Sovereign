"""The Figures kit's people rigged and animated for the street scene: each figure (kit_figures.py) gets a simple
skeleton, its vertices bound rigidly to the nearest limb, and two looping actions, Walk (one stride a second)
and Idle (breathing and a slow sway). Exported as FBX skeletal meshes with their animations to
<build>/Rigged/SK_<Name>.fbx for tools/art/ue_import_rigged.py.

  blender -b --factory-startup --python tools/art/blender/rig_figures.py -- art/build [Citizen ...]

Figures stand at the origin facing -Y, metres, as the kits are built.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Matrix  # noqa: E402

import kit_figures  # noqa: E402
import kitlib  # noqa: E402

PEOPLE = {"Citizen": kit_figures.citizen, "Herald": kit_figures.herald, "Captain": kit_figures.captain, "Soldier": kit_figures.soldier,
          "Leader": kit_figures.leader}
FPS = 24
CM = 100.0  # the rig is built in centimetres (see rig)

# name: (head, tail, parent); sides are r (+x) and l (-x).
BONES = {"root": ((0, 0, 0), (0, 0, 0.15), None), "pelvis": ((0, 0, 0.92), (0, 0, 1.05), "root"), "spine": ((0, 0, 1.05), (0, 0, 1.4), "pelvis"),
         "head": ((0, 0, 1.42), (0, 0, 1.75), "spine")}
for s, n in ((1, "r"), (-1, "l")):
    BONES["thigh_" + n] = ((s * 0.09, 0, 0.92), (s * 0.1, -0.01, 0.5), "pelvis")
    BONES["shin_" + n] = ((s * 0.1, -0.01, 0.5), (s * 0.1, 0.01, 0.08), "thigh_" + n)
    BONES["upperarm_" + n] = ((s * 0.215, 0, 1.37), (s * 0.255, 0.01, 1.11), "spine")
    BONES["forearm_" + n] = ((s * 0.255, 0.01, 1.11), (s * 0.265, -0.03, 0.8), "upperarm_" + n)


def bone_for(x, y, z):
    """Which limb carries a vertex: weapons with the hands that hold them, limbs by where they are, cloth with the body."""
    side = "r" if x > 0 else "l"
    if x > 0.29:
        return "forearm_r"  # the spear, staff or musket in the right hand
    if x < -0.3:
        return "forearm_l"  # the shield or bow in the left
    if abs(x) > 0.18 and 0.72 < z < 1.42 and abs(y) < 0.09:
        return ("upperarm_" if z > 1.12 else "forearm_") + side
    if z < 0.92:
        # A leg only within its own limb's radius; a skirt or robe around it stays with the hips.
        if math.hypot(abs(x) - 0.095, y) < 0.09 or (z < 0.16 and abs(x) < 0.2):  # the foot reaches forward of the leg
            return ("thigh_" if z > 0.5 else "shin_") + side
        return "pelvis"
    if z > 1.43:
        return "head"
    return "pelvis" if z < 1.05 else "spine"


def rig(name, make):
    kitlib.reset_scene()
    scene = bpy.context.scene
    scene.render.fps = FPS
    # Built in centimetres with Blender's unit scale at 0.01, so the FBX needs no scaled root node: Unreal then
    # reads the skeleton and the skin at the same size (a metre-scale rig imports with a 100x root and vanishes).
    scene.unit_settings.scale_length = 0.01
    body = make().build()
    body.data.transform(Matrix.Scale(CM, 4))
    body.name = "SK_" + name
    arm_data = bpy.data.armatures.new("Skeleton")
    arm = bpy.data.objects.new("Armature", arm_data)
    scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    edit = {}
    for bname, (head, tail, parent) in BONES.items():
        b = arm_data.edit_bones.new(bname)
        b.head, b.tail = [c * CM for c in head], [c * CM for c in tail]
        b.roll = 0.0
        if parent:
            b.parent = edit[parent]
        edit[bname] = b
    bpy.ops.object.mode_set(mode="OBJECT")
    for bname in BONES:
        body.vertex_groups.new(name=bname)
    for v in body.data.vertices:
        body.vertex_groups[bone_for(*v.co)].add([v.index], 1.0, "REPLACE")
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    arm.animation_data_create()
    walk(arm, name)
    idle(arm, name)
    return arm, body


def key(pb, frame, rx=0.0, ry=0.0, rz=0.0, loc=None):
    pb.rotation_mode = "XYZ"
    pb.rotation_euler = (math.radians(rx), math.radians(ry), math.radians(rz))
    pb.keyframe_insert("rotation_euler", frame=frame)
    if loc is not None:
        pb.location = loc
        pb.keyframe_insert("location", frame=frame)


def walk(arm, name):
    """One stride a second: legs swing 24 degrees, the trailing shin bends, arms swing against the legs, the hips bob."""
    action = bpy.data.actions.new("Walk")
    arm.animation_data.action = action
    P = arm.pose.bones
    stiff = 0.5 if name in ("Captain", "Soldier") else 1.0  # armed men keep their weapon arm steadier
    n = FPS
    for f in range(0, n + 1, 3):
        ph = 2 * math.pi * f / n
        s = math.sin(ph)
        key(P["thigh_r"], f, rx=24 * s)
        key(P["thigh_l"], f, rx=-24 * s)
        key(P["shin_r"], f, rx=-max(0.0, 32 * math.sin(ph - 1.2)))
        key(P["shin_l"], f, rx=-max(0.0, 32 * math.sin(ph + math.pi - 1.2)))
        key(P["upperarm_r"], f, rx=-16 * s * stiff)
        key(P["upperarm_l"], f, rx=16 * s)
        key(P["forearm_r"], f, rx=8 + 6 * s * stiff)
        key(P["forearm_l"], f, rx=8 - 6 * s)
        key(P["pelvis"], f, rz=4 * s, loc=(0, 0.018 * CM * abs(math.cos(ph)), 0))  # bone space: Y runs along the bone, up
        key(P["spine"], f, rz=-3 * s, rx=3)
        key(P["head"], f, rz=1.5 * s)
    finish(action, n)


def idle(arm, name):
    """Standing: slow breathing in the chest and a little sway, three seconds round."""
    action = bpy.data.actions.new("Idle")
    arm.animation_data.action = action
    P = arm.pose.bones
    n = FPS * 3
    for f in range(0, n + 1, 6):
        ph = 2 * math.pi * f / n
        key(P["spine"], f, rx=1.5 * math.sin(ph), rz=1.0 * math.sin(ph * 0.5))
        key(P["head"], f, rz=3.0 * math.sin(ph * 0.5 + 0.7), rx=-1.0 * math.sin(ph))
        key(P["upperarm_r"], f, rx=2.0 * math.sin(ph))
        key(P["upperarm_l"], f, rx=-2.0 * math.sin(ph))
        for b in ("pelvis", "thigh_r", "thigh_l", "shin_r", "shin_l", "forearm_r", "forearm_l"):
            key(P[b], f)
    finish(action, n)


def finish(action, n):
    action.use_fake_user = True
    action.frame_range = (0, n)
    for fc in getattr(action, "fcurves", []):
        fc.modifiers.new("CYCLES")


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    build = os.path.abspath(args[0] if args else os.path.join("art", "build"))
    wanted = args[1:] or list(PEOPLE)
    out = os.path.join(build, "Rigged")
    os.makedirs(out, exist_ok=True)
    for name in wanted:
        arm, body = rig(name, PEOPLE[name])
        bpy.ops.object.select_all(action="DESELECT")
        arm.select_set(True)
        body.select_set(True)
        bpy.context.view_layer.objects.active = arm
        path = os.path.join(out, "SK_%s.fbx" % name)
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, mesh_smooth_type="FACE",
                                 colors_type="SRGB", apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", add_leaf_bones=False, bake_anim=True,
                                 bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                                 primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL")
        print("rigged %s: %d vertices -> %s" % (name, len(body.data.vertices), path))


if __name__ == "__main__":
    main()
