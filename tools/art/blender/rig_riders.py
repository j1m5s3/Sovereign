"""The horsemen rigged for live battles: the Figures kit's riders (the plain one and the four cultures' dress) on a horse
skeleton, the horse walking a four-beat gait with each hoof planted while it bears weight, the neck nodding, the rider
riding the barrel. Exported as FBX skeletal meshes with Walk and Idle to <build>/Rigged/SK_Rider*.fbx, alongside
rig_figures.py's people, for tools/art/ue_import_rigged.py.

  blender -b --factory-startup --python tools/art/blender/rig_riders.py -- art/build

The horse stands along +X (its head), as kit_figures.py builds it; metres, built in centimetres like rig_figures.py.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import kit_figures  # noqa: E402
import kitlib  # noqa: E402

RIDERS = {"Rider": kit_figures.rider, "Rider_MiddleEast": kit_figures.rider_middleeast, "Rider_Asian": kit_figures.rider_asian,
          "Rider_African": kit_figures.rider_african, "Rider_American": kit_figures.rider_american}
FPS = 24
CM = 100.0
LEGS = {"fl": (0.6, -0.15), "fr": (0.6, 0.15), "hl": (-0.6, -0.15), "hr": (-0.6, 0.15)}  # front/hind, left/right: (x, y)
HIP = 1.0     # m: where each leg meets the barrel
KNEE = 0.5
ANKLE = 0.06  # the hoof's height with the leg planted
# The walk: a stride (one hoof's whole cycle) in WALK_FRAMES, each hoof planted for STANCE of it while the horse moves on.
WALK_FRAMES = 24
STANCE = 0.65
STEP = 0.3    # m: a planted hoof runs from STEP ahead of its hip to STEP behind
LIFT = 0.12
WALK_SPEED = 2 * STEP / (STANCE * WALK_FRAMES / FPS) * CM  # cm/s, about 154 (SovBattleScene.cpp kRideSpeed)
PHASE = {"hl": 0.0, "fl": 0.25, "hr": 0.5, "fr": 0.75}  # the four-beat walk: left hind, left fore, right hind, right fore


def bone_for(x, y, z):
    """Which bone carries a vertex: a leg's upper or lower part, the neck and head, else the barrel (rider and tack with it)."""
    if z < HIP and abs(y) < 0.26:
        best = min(LEGS, key=lambda k: (LEGS[k][0] - x) ** 2 + (LEGS[k][1] - y) ** 2)
        if abs(LEGS[best][0] - x) < 0.25:
            return best + ("_upper" if z > KNEE else "_lower")
    if x > 0.72 and z > 1.22 and abs(y) < 0.26:
        return "neck"
    return "barrel"


def rig(name, make):
    kitlib.reset_scene()
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.unit_settings.scale_length = 0.01
    body = make().build()
    body.data.transform(Matrix.Scale(CM, 4))
    body.name = "SK_" + name
    arm_data = bpy.data.armatures.new("Skeleton")
    arm = bpy.data.objects.new("Armature", arm_data)
    scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm_data.edit_bones
    root = eb.new("root")
    root.head, root.tail = (0, 0, 0), (0, 0, 15)
    barrel = eb.new("barrel")
    barrel.head, barrel.tail = (-60, 0, 120), (60, 0, 125)
    barrel.parent = root
    neck = eb.new("neck")
    neck.head, neck.tail = (75, 0, 130), (105, 0, 175)
    neck.parent = barrel
    for leg, (x, y) in LEGS.items():
        up = eb.new(leg + "_upper")
        up.head, up.tail = (x * CM, y * CM, HIP * CM), (x * CM, y * CM, KNEE * CM)
        up.parent = barrel
        lo = eb.new(leg + "_lower")
        lo.head, lo.tail = (x * CM, y * CM, KNEE * CM), (x * CM, y * CM, ANKLE * CM)
        lo.parent = up
    for b in eb:
        b.roll = 0.0
    bpy.ops.object.mode_set(mode="OBJECT")
    for b in arm_data.bones:
        body.vertex_groups.new(name=b.name)
    for v in body.data.vertices:
        co = v.co / CM
        body.vertex_groups[bone_for(co.x, co.y, co.z)].add([v.index], 1.0, "REPLACE")
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    arm.animation_data_create()
    walk(arm)
    idle(arm)
    return arm, body


def _hoof(u):
    """A hoof's place (forward of its hip, height) at phase u: planted from 0 to STANCE, then lifted and swung forward."""
    if u < STANCE:
        return STEP - 2 * STEP * u / STANCE, ANKLE
    w = (u - STANCE) / (1 - STANCE)
    return -STEP + 2 * STEP * (0.5 - 0.5 * math.cos(math.pi * w)), ANKLE + LIFT * math.sin(math.pi * w)


def _solve(leg, fwd, z):
    """Upper and lower leg angles (radians from straight down, + toward the head) that put the hoof `fwd` ahead of its hip at
    height `z`: an exact two-bone solution, the joint bent forward on a foreleg (the knee) and back on a hind leg (the hock)."""
    l1, l2 = HIP - KNEE, KNEE - ANKLE
    dx, dz = fwd, z - HIP
    dist = min(math.hypot(dx, dz), l1 + l2 - 1e-4)
    base = math.atan2(dx, -dz)
    alpha = math.acos(max(-1.0, min(1.0, (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist))))
    beta = math.pi - math.acos(max(-1.0, min(1.0, (l1 * l1 + l2 * l2 - dist * dist) / (2 * l1 * l2))))
    sign = 1.0 if leg.startswith("f") else -1.0
    return base + sign * alpha, -sign * beta  # the lower leg's angle relative to the upper


def _leg_axis(arm):
    """Which local rotation axis swings a downward leg bone fore and aft (along X): found by trying each."""
    P = arm.pose.bones
    b = P["fl_upper"]
    vl = bpy.context.view_layer
    best, best_dx = 0, 0.0
    for axis in range(3):
        r = [0.0, 0.0, 0.0]
        r[axis] = math.radians(10)
        b.rotation_euler = r
        vl.update()
        dx = abs(b.tail.x - LEGS["fl"][0] * CM)
        if dx > best_dx:
            best, best_dx = axis, dx
    b.rotation_euler = (0, 0, 0)
    vl.update()
    return best


def _leg_sign(arm, axis):
    """+1 when a positive turn about `axis` swings a leg toward the head (+X), else -1."""
    b = arm.pose.bones["fl_upper"]
    r = [0.0, 0.0, 0.0]
    r[axis] = math.radians(10)
    b.rotation_euler = r
    bpy.context.view_layer.update()
    sign = 1.0 if b.tail.x > LEGS["fl"][0] * CM else -1.0
    b.rotation_euler = (0, 0, 0)
    bpy.context.view_layer.update()
    return sign


def walk(arm):
    action = bpy.data.actions.new("Walk")
    arm.animation_data.action = action
    P = arm.pose.bones
    for b in P:
        b.rotation_mode = "XYZ"
    axis = _leg_axis(arm)
    turn = _leg_sign(arm, axis)
    n = WALK_FRAMES
    for f in range(0, n + 1):
        u = f / n
        for leg in LEGS:
            fwd, z = _hoof((u + PHASE[leg]) % 1.0)
            for part, ang in zip(("_upper", "_lower"), _solve(leg, fwd, z)):
                r = [0.0, 0.0, 0.0]
                r[axis] = turn * ang
                P[leg + part].rotation_euler = r
                P[leg + part].keyframe_insert("rotation_euler", frame=f)
        P["neck"].rotation_euler = (0, 0, 0)
        P["neck"].rotation_euler[axis] = math.radians(4 * math.sin(4 * math.pi * u))  # the head nods with the stride
        P["neck"].keyframe_insert("rotation_euler", frame=f)
    finish(action, n)


def idle(arm):
    action = bpy.data.actions.new("Idle")
    arm.animation_data.action = action
    P = arm.pose.bones
    axis = _leg_axis(arm)
    n = FPS * 3
    for f in range(0, n + 1, 6):
        ph = 2 * math.pi * f / n
        for b in P:
            b.rotation_euler = (0, 0, 0)
        P["neck"].rotation_euler[axis] = math.radians(-6 + 3 * math.sin(ph))  # grazing, then up
        for b in P:
            b.keyframe_insert("rotation_euler", frame=f)
    finish(action, n)


def finish(action, n):
    action.use_fake_user = True
    action.frame_range = (0, n)
    for fc in getattr(action, "fcurves", []):
        fc.modifiers.new("CYCLES")


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    build = os.path.abspath(args[0] if args else os.path.join("art", "build"))
    wanted = args[1:] or list(RIDERS)
    out = os.path.join(build, "Rigged")
    os.makedirs(out, exist_ok=True)
    for name in wanted:
        arm, body = rig(name, RIDERS[name])
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
