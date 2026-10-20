"""The Figures kit's people rigged and animated for the street scene: each figure (kit_figures.py) gets a simple
skeleton, its vertices bound rigidly to the nearest limb, and two looping actions, Walk (a 90 cm stride in 0.83 s, legs by inverse kinematics so planted feet hold still)
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
import kit_leaders  # noqa: E402
import kitlib  # noqa: E402


F = kit_figures


def townsman(seed=71):
    """The street's townsman: the map's citizen, his tunic dyed (the team slot, tinted per person in the street)."""
    p = F._figure("SM_Townsman", seed)
    F._body(p, F.SKIN[0])
    F._sandals(p)
    F._tunic(p, F.LINEN, hem=0.62, team=True)
    F._belt(p, F.DARK_LEATHER, 1.0)
    p.loft([(1.12, 0.21, 0.135), (1.3, 0.215, 0.13), (1.44, 0.15, 0.1)], "#7a3b22", segments=14, arc=(math.pi * 0.25, math.pi * 1.35),
           folds=6, fold_depth=0.06, two_sided=True)
    p.segment((0.16, -0.11, 1.4), (-0.15, -0.12, 0.98), 0.012, 0.012, F.DARK_LEATHER, segments=6)
    p.box((-0.19, -0.1, 0.94), (0.16, 0.06, 0.15), "#7d6448", taper=0.9)
    F._head(p, F.SKIN[0], F.HAIR[1])
    return p


def traveller(seed=72):
    """A darker-skinned man in a short dyed tunic and a hooded cloak, bearded."""
    p = F._figure("SM_Traveller", seed)
    F._body(p, F.SKIN[2])
    F._sandals(p, boots=True, color=F.DARK_LEATHER)
    F._tunic(p, F.LINEN, hem=0.66, team=True)
    F._belt(p, F.LEATHER, 1.0, buckle=F.GOLD)
    F._cloak(p, F.WOOL, hem=0.6, clasp=F.BRONZE, team=False)
    z = F._head(p, F.SKIN[2], F.HAIR[2], beard=F.HAIR[2])
    p.loft([(z + 0.05, 0.11, 0.12, 0, 0.03), (z + 0.18, 0.115, 0.125, 0, 0.02), (z + 0.27, 0.07, 0.08)], F.WOOL, segments=14,
           arc=(-0.3, math.pi + 0.3), two_sided=True)  # the hood, back off the face
    return p


def townswoman(seed=73):
    """A woman in a long dyed dress and a pale headscarf, a basket on her hip."""
    p = F._figure("SM_Townswoman", seed)
    F._body(p, F.SKIN[1], s=0.95)
    F._sandals(p, s=0.95)
    F._tunic(p, F.LINEN, s=0.95, hem=0.12, team=True)  # the dress, to the ankle
    F._belt(p, "#e8dcc0", 0.98, s=0.95)
    z = F._head(p, F.SKIN[1], F.HAIR[0], s=0.95)
    p.loft([(z + 0.04, 0.1, 0.105, 0, 0.03), (z + 0.15, 0.1, 0.11, 0, 0.01), (z + 0.24, 0.08, 0.09), (z + 0.27, 0.03, 0.03)], "#efe6d2",
           segments=16, arc=(-0.35, math.pi + 0.35), two_sided=True)  # the headscarf
    p.cylinder((-0.26, -0.02, 0.8), 0.12, 0.14, "#b8935a", segments=12)  # the basket
    p.blob((-0.26, -0.02, 0.96), 0.07, "#c03a2a", subdiv=1)
    return p


GREY = "#cfc8bb"  # an elder's hair
JAR = "#9c6a48"   # fired clay
STRAW_HAT = "#c9a96a"
kitlib.patterns({"hair": [GREY], "rooftile": [JAR], "thatch": [STRAW_HAT]})


def elder(seed=74):
    """An old man, white-haired and bearded, in a long dyed robe and a plain cloak, leaning on a staff."""
    p = F._figure("SM_Elder", seed)
    F._body(p, F.SKIN[3])
    F._sandals(p)
    F._tunic(p, F.LINEN, hem=0.1, team=True)
    F._belt(p, F.LEATHER, 1.0)
    F._cloak(p, F.WOOL, hem=0.3, clasp=F.BRONZE, team=False)
    F._head(p, F.SKIN[3], GREY, beard=GREY)
    p.segment((0.31, -0.08, 0.02), (0.29, -0.06, 1.55), 0.022, 0.018, F.WOOD, segments=6)  # the staff, in the right hand
    return p


def porter(seed=75):
    """A labourer in a short dyed tunic, bare-legged, a full sack over his shoulder."""
    p = F._figure("SM_Porter", seed)
    F._body(p, F.SKIN[2])
    F._sandals(p)
    F._tunic(p, F.LINEN, hem=0.78, team=True)
    F._belt(p, "#7d6448", 1.0)
    F._head(p, F.SKIN[2], F.HAIR[2])
    p.blob((-0.13, 0.1, 1.55), 0.2, F.WOOL, squash=0.75, subdiv=2, wobble=0.08)  # the sack, on the head bone with the shoulders
    return p


def merchant(seed=76):
    """A well-fed merchant in a long dyed coat, a purse at his belt and a broad straw hat."""
    p = F._figure("SM_Merchant", seed)
    F._body(p, F.SKIN[0], legs=F.WOOL)
    F._sandals(p, boots=True, color=F.LEATHER)
    F._tunic(p, F.LINEN, hem=0.35, team=True)
    p.team_color(True)
    p.loft([(1.0, 0.19, 0.15), (1.12, 0.2, 0.16), (1.24, 0.19, 0.14)], F.LINEN, segments=16)  # the belly, in the coat
    p.team_color(False)
    F._belt(p, F.DARK_LEATHER, 1.0, buckle=F.GOLD)
    p.blob((0.12, -0.13, 0.94), 0.05, F.LEATHER, squash=1.3, subdiv=1)  # the purse
    z = F._head(p, F.SKIN[0], F.HAIR[3], moustache=F.HAIR[3])
    p.cylinder((0, 0, z + 0.2), 0.24, 0.02, STRAW_HAT, segments=16)  # the brim
    p.cylinder((0, 0, z + 0.2), 0.1, 0.1, STRAW_HAT, radius_top=0.08, segments=14)
    return p


def water_carrier(seed=77):
    """A woman in a long dyed dress and headcloth, a water jar balanced on her head."""
    p = F._figure("SM_WaterCarrier", seed)
    F._body(p, F.SKIN[3], s=0.95)
    F._sandals(p, s=0.95)
    F._tunic(p, F.LINEN, s=0.95, hem=0.12, team=True)
    F._belt(p, "#8b2a22", 0.98, s=0.95)
    z = F._head(p, F.SKIN[3], F.HAIR[1], s=0.95)
    p.loft([(z + 0.04, 0.1, 0.105, 0, 0.03), (z + 0.15, 0.1, 0.11, 0, 0.01), (z + 0.24, 0.08, 0.09)], F.WOOL,
           segments=16, arc=(-0.35, math.pi + 0.35), two_sided=True)  # the headcloth
    p.cylinder((0, 0.01, z + 0.25), 0.06, 0.03, F.WOOL, segments=10)  # the pad under the jar
    p.loft([(z + 0.27, 0.06, 0.06, 0, 0.01), (z + 0.36, 0.13, 0.13, 0, 0.01), (z + 0.46, 0.1, 0.1, 0, 0.01), (z + 0.52, 0.045, 0.045, 0, 0.01),
            (z + 0.56, 0.055, 0.055, 0, 0.01)], JAR, segments=14)
    return p


PEOPLE = {"Citizen": kit_figures.citizen, "Herald": kit_figures.herald, "Captain": kit_figures.captain, "Soldier": kit_figures.soldier,
          "Leader": kit_figures.leader, "Townsman": townsman, "Traveller": traveller, "Townswoman": townswoman, "Elder": elder,
          "Porter": porter, "Merchant": merchant, "WaterCarrier": water_carrier,
          # The other foot soldiers, for live battles (their cultures' dress stays still: rigging each would weigh too much).
          "Archer": kit_figures.archer, "Musketeer": kit_figures.musketeer, "Rifleman": kit_figures.rifleman}
# Each civ's ruler as themselves (kit_leaders.py), walking the street and leading in battle.
PEOPLE.update({"Leader_" + civ: make for civ, make in kit_leaders.LEADERS.items()})
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


# The walk: a stride in WALK_FRAMES at FPS, each foot on the ground for STANCE of it and moving back at a steady
# WALK_SPEED while it is (so planted feet do not slide), then lifted and swung forward. Unreal plays it at
# speed / WALK_SPEED (SovStreetScene.cpp kWalkSpeed).
WALK_FRAMES = 20
STANCE = 0.6
STEP = 0.27     # m: a planted foot runs from STEP ahead of the hip to STEP behind it
LIFT = 0.1      # m: how high the swinging foot rises
ANKLE = 0.08    # m: the ankle's height with the foot flat
WALK_SPEED = 2 * STEP / (STANCE * WALK_FRAMES / FPS) * CM  # cm/s, about 108


def _foot(u):
    """Where a foot is (forward of the hip, height of the ankle) at phase u in 0..1: planted from 0 to STANCE."""
    if u < STANCE:
        return STEP - 2 * STEP * u / STANCE, ANKLE
    w = (u - STANCE) / (1 - STANCE)
    return -STEP + 2 * STEP * (0.5 - 0.5 * math.cos(math.pi * w)), ANKLE + LIFT * math.sin(math.pi * w)


def _solve_leg(arm, side, target, start):
    """Thigh and shin angles (degrees about X) that put the shin's tail (the ankle) at `target` (armature space),
    searched from `start` (the last frame's answer, so the knee keeps bending the same way)."""
    P = arm.pose.bones
    th, sh = P["thigh_" + side], P["shin_" + side]
    vl = bpy.context.view_layer
    a = list(start)

    def err(x):
        th.rotation_euler = (math.radians(x[0]), 0, 0)
        sh.rotation_euler = (math.radians(x[1]), 0, 0)
        vl.update()
        t = sh.tail
        return (t.y - target[0], t.z - target[1])

    for _ in range(8):  # Gauss-Newton on the two angles, from the last answer
        e = err(a)
        if abs(e[0]) + abs(e[1]) < 0.02:
            break
        J = []
        for k in range(2):
            b = list(a)
            b[k] += 0.5
            ek = err(b)
            J.append(((ek[0] - e[0]) / 0.5, (ek[1] - e[1]) / 0.5))
        (j00, j10), (j01, j11) = J  # columns: d/da0, d/da1
        det = j00 * j11 - j01 * j10
        if abs(det) < 1e-9:
            break
        d0 = (j11 * e[0] - j01 * e[1]) / det
        d1 = (-j10 * e[0] + j00 * e[1]) / det
        a = [a[0] - d0, a[1] - d1]
    return [(x + 180.0) % 360.0 - 180.0 for x in a]  # one turn at most, or the keys between frames spin


def walk(arm, name):
    """The legs by inverse kinematics along _foot (each planted foot holds still on the ground), the hips lowered
    as far as the planted legs need and bobbing, the arms swinging against the legs."""
    action = bpy.data.actions.new("Walk")
    arm.animation_data.action = action
    P = arm.pose.bones
    for b in P:
        b.rotation_mode = "XYZ"
    stiff = 0.5 if name in ("Captain", "Soldier") else 1.0  # armed men keep their weapon arm steadier
    L = 0.84 * 0.985  # thigh and shin, kept a little short of straight
    hip0 = 0.92
    n = WALK_FRAMES
    last = {"r": (-10.0, 10.0), "l": (-10.0, 10.0)}
    feet_at = [{"r": _foot((f / n) % 1.0), "l": _foot((f / n + 0.5) % 1.0)} for f in range(n + 1)]
    # The hips as high as the legs allow, never above their rest; then eased (a lower envelope of parabolas, so it
    # only ever goes lower) so they do not drop in a single frame as a foot lands.
    reach = [min([hip0 - 0.02] + [z + math.sqrt(max(L * L - fwd * fwd, 0.0)) for fwd, z in feet.values()]) for feet in feet_at]
    ease = 0.003  # m per frame squared
    hips = [min(reach[g] + ease * min(abs(f - g), n - abs(f - g)) ** 2 for g in range(n)) for f in range(n + 1)]
    for f in range(0, n + 1):
        u = f / n
        feet = feet_at[f]
        hz = hips[f]
        sw = math.sin(2 * math.pi * u)
        # The hips sway a little with the stride; the legs are solved with it applied, so the feet stay put.
        key(P["pelvis"], f, rz=2 * sw, loc=(0, (hz - hip0) * CM, 0))  # bone space: Y runs along the bone, up
        bpy.context.view_layer.update()
        for side, (fwd, z) in feet.items():
            a = last[side] = _solve_leg(arm, side, (-fwd * CM, z * CM), last[side])  # forward is -Y
            P["thigh_" + side].rotation_euler = (math.radians(a[0]), 0, 0)
            P["shin_" + side].rotation_euler = (math.radians(a[1]), 0, 0)
            P["thigh_" + side].keyframe_insert("rotation_euler", frame=f)
            P["shin_" + side].keyframe_insert("rotation_euler", frame=f)
        # Arms against the legs: the right arm forward as the left leg is.
        key(P["upperarm_r"], f, rx=-16 * sw * stiff)
        key(P["upperarm_l"], f, rx=16 * sw)
        key(P["forearm_r"], f, rx=8 + 6 * sw * stiff)
        key(P["forearm_l"], f, rx=8 - 6 * sw)
        key(P["spine"], f, rz=-3 * sw, rx=3)
        key(P["head"], f, rz=1.5 * sw)
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
