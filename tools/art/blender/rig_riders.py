"""The horsemen rigged for live battles and the map: the Figures kit's riders (the plain one and the four cultures'
dress) on a horse skeleton (barrel, neck, and each leg's upper part, lower part and hoof), with three looping actions
made here, in place: Walk, a four-beat walk (WALK_SPEED, about 92 cm/s); Run, a transverse gallop on the right lead
(RUN_SPEED, about 457 cm/s); and Idle, grazing. In both gaits each hoof is planted while it bears weight and held still
on the ground at the gait's speed (each leg solved exactly as two bones, the hoof kept flat), and the barrel rides on
the planted hooves (the carriage): each end of it rides as high as its own planted legs reach, so the body bounces and
rocks with the stride and no planted leg is stretched past REACH of its length. The neck nods, the rider rides the
barrel. Each gait loop starts as the left hind hoof touches down, but the two gaits' other footfalls do not line up, so
the game does not mix them at one phase across the way from one speed to the other: it hands the walk over to the gallop
just past the walk's speed and plays the gallop alone above that, slowed or hurried (SovGait.h kSovHorseHandover). Each
rider is checked (check: skin, planted hooves held, ground, the loop closing, the barrel moving smoothly) before it is
exported as an FBX skeletal mesh with Walk, Run and Idle to <build>/Rigged/SK_Rider*.fbx, alongside rig_figures.py's
people, for tools/art/ue_import_rigged.py; one that fails is not exported (an older export of it is removed) and the
script exits 1. It prints the speeds the gaits were authored at, which the game's SovGait.h must match.

  blender -b --factory-startup --python-exit-code 1 --python tools/art/blender/rig_riders.py -- art/build [Rider ...]

The horse stands along +X (its head), as kit_figures.py builds it; metres, built in centimetres like rig_figures.py.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import kit_figures  # noqa: E402
import kitlib  # noqa: E402

RIDERS = {"Rider": kit_figures.rider, "Rider_MiddleEast": kit_figures.rider_middleeast, "Rider_Asian": kit_figures.rider_asian,
          "Rider_African": kit_figures.rider_african, "Rider_American": kit_figures.rider_american}
FPS = 24
CM = 100.0
# Keys a frame, in the actions and the FBX: a galloping leg turns about 12 degrees a frame, and the game blends in a
# straight line from one key to the next; at one key a frame a planted galloping hoof would slip about 1.5 cm along the
# ground and dip about 1 cm between them (the check fails it), at two it stays within a few millimetres. The FBX keeps
# every key (no simplifying: Blender's default drops keys from a curve that barely changes, and a planted hoof slid).
KEYS = 2
LEGS = {"fl": (0.6, -0.15), "fr": (0.6, 0.15), "hl": (-0.6, -0.15), "hr": (-0.6, 0.15)}  # front/hind, left/right: (x, y)
HIP = 1.0     # m: where each leg meets the barrel
KNEE = 0.5
KNEE_FWD = 0.03  # m: the knee sits this far ahead of the hip and the hoof (kit_figures._horse)
ANKLE = 0.06  # the hoof's height with the leg planted: where the hoof meets the lower leg
LEG = math.hypot(KNEE_FWD, HIP - KNEE) + math.hypot(KNEE_FWD, KNEE - ANKLE)  # m: hip to ankle, at full stretch
REACH = 0.985  # the most of its length a planted leg stretches: the barrel rides as high as that lets it
# The walk: a stride (one hoof's whole cycle) in WALK_FRAMES, each hoof planted for STANCE of it while the horse moves on.
WALK_FRAMES = 24
STANCE = 0.65
STEP = 0.3    # m: a planted hoof runs from STEP ahead of its hip to STEP behind
LIFT = 0.12
FOLD = 40     # degrees: how far a hoof turns up and back at the height of its swing
WALK_SPEED = 2 * STEP / (STANCE * WALK_FRAMES / FPS) * CM  # cm/s, about 92 (SovGait.h kSovHorseWalkSpeed)
PHASE = {"hl": 0.0, "fl": 0.25, "hr": 0.5, "fr": 0.75}  # where each hoof touches down in the stride, the four-beat walk:
# left hind, left fore, right hind, right fore
# The gallop, a transverse gallop on the right lead: the hooves land in turn (left hind, right hind, left fore, right
# fore, at RUN_PHASE of the stride), each planted for RUN_STANCE of it, then a moment with none down, the legs gathered
# under the body, before the left hind lands again. A swing lifts the hoof higher and folds the leg tight.
RUN_FRAMES = 12
RUN_STANCE = 0.35
RUN_STEP = 0.4
RUN_LIFT = 0.3
RUN_FOLD = 80
RUN_RISE = 0.065  # m: how far an end of the barrel rises, above the line between its landings, while it has no hoof down
# (the most that keeps it within the check's JOLT a frame)
RUN_SPEED = 2 * RUN_STEP / (RUN_STANCE * RUN_FRAMES / FPS) * CM  # cm/s, about 457 (SovGait.h kSovHorseRunSpeed)
# Each touchdown falls on a key (24 a stride, at KEYS), so no key's blend runs from a hoof in the air to the planted one.
RUN_PHASE = {"hl": 0 / 24, "hr": 3 / 24, "fl": 8 / 24, "fr": 11 / 24}
# Each gait as gait() reads it. peak: how far through its swing a hoof is highest; rise and sway: how the barrel rides
# (_carriage); nod: the neck's swing up and down (degrees), how many times a stride, and where in it the head is highest.
GAITS = {"Walk": dict(frames=WALK_FRAMES, stance=STANCE, step=STEP, lift=LIFT, fold=FOLD, phase=PHASE, peak=0.5, rise=0.0, sway=4.0,
                      nod=(4.0, 2, 0.1)),
         "Run": dict(frames=RUN_FRAMES, stance=RUN_STANCE, step=RUN_STEP, lift=RUN_LIFT, fold=RUN_FOLD, phase=RUN_PHASE, peak=0.4,
                     rise=RUN_RISE, sway=40.0, nod=(8.0, 1, 0.1))}
# The speed (cm/s) each looping action was authored at: the game plays it at its own speed / this.
SPEEDS = {"Walk": WALK_SPEED, "Run": RUN_SPEED, "Idle": 0.0}
# The check (check): a planted hoof holds within HOLD cm of where it landed, on the ground; no hoof goes more than SINK
# cm into it; the barrel moves no more than JOLT cm (or degrees) a frame; sampled SUBFRAMES times a frame.
HOLD = 0.5
SINK = 0.5
JOLT = 3.0
SUBFRAMES = 4


def _leg_of(co):
    """The leg a solid (its points, in metres) belongs to: every point of it within a hand's breadth of that leg's line
    (which leans forward to the knee) and below the top of the hip's ball; else None."""
    for leg, (lx, ly) in LEGS.items():
        if all(z < HIP + 0.1 and math.hypot(x - lx - KNEE_FWD * max(0.0, min(z / KNEE, (HIP - z) / (HIP - KNEE))), y - ly) < 0.1
               for x, y, z in co):
            return leg
    return None


def bones_for(mesh):
    """Which bone carries each vertex ({vertex: bone}), solid by solid for the legs (every solid is its own island of the
    mesh, as in rig_figures.py), so none is torn between two bones: a leg's solid (_leg_of) rides its upper part (the
    thigh, and the balls of the hip and knee, each turning about its own centre), its hoof (the solid standing on the
    ground) or else its lower part (the cannon and the ankle's ball). The rest goes vertex by vertex: the neck and head,
    else the barrel (the rider and the tack with it, and the barrel's own underside where the legs pass into it)."""
    parent = list(range(len(mesh.vertices)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for e in mesh.edges:
        a, b = find(e.vertices[0]), find(e.vertices[1])
        if a != b:
            parent[a] = b
    islands = {}
    for v in mesh.vertices:
        islands.setdefault(find(v.index), []).append(v.index)
    out = {}
    for members in islands.values():
        co = [mesh.vertices[i].co / CM for i in members]
        leg = _leg_of(co)
        if leg:
            low, high = min(c.z for c in co), max(c.z for c in co)
            part = "_upper" if (low + high) / 2 > KNEE - 0.05 else "_hoof" if low < 0.005 else "_lower"
            out.update((i, leg + part) for i in members)
        else:
            out.update((i, "neck" if c.x > 0.72 and c.z > 1.22 and abs(c.y) < 0.26 else "barrel") for i, c in zip(members, co))
    return out


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
        up.head, up.tail = (x * CM, y * CM, HIP * CM), ((x + KNEE_FWD) * CM, y * CM, KNEE * CM)
        up.parent = barrel
        lo = eb.new(leg + "_lower")
        lo.head, lo.tail = up.tail, (x * CM, y * CM, ANKLE * CM)
        lo.parent = up
        hoof = eb.new(leg + "_hoof")
        hoof.head, hoof.tail = lo.tail, (x * CM, y * CM, 0)
        hoof.parent = lo
    for b in eb:
        b.roll = 0.0
    bpy.ops.object.mode_set(mode="OBJECT")
    for b in arm_data.bones:
        body.vertex_groups.new(name=b.name)
    for i, bone in bones_for(body.data).items():
        body.vertex_groups[bone].add([i], 1.0, "REPLACE")
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    arm.animation_data_create()
    for b in arm.pose.bones:
        b.rotation_mode = "QUATERNION"
    gait(arm, "Walk")
    gait(arm, "Run")
    idle(arm)
    return arm, body


def _hoof(g, q):
    """A hoof's place at phase q of its own stride (0 as it touches down), against its hip at rest: (forward, height, how
    far it is folded, 0 to 1). Planted from 0 to the stance, running back at the gait's speed; then lifted (highest at
    `peak` of the swing) and swung forward, leaving the ground and meeting it again at that same speed, so it neither
    kicks off nor stamps. In the gallop the swing reaches up to 1% past the leg's length for the last frame before
    touchdown: the leg goes straight there (_key reaches for it), so half a frame before it lands the hoof is still about
    1.4 cm up and 3 cm short of its spot, coming forward over the ground at about 150 cm/s."""
    step, stance = g["step"], g["stance"]
    if q < stance:
        return step - 2 * step * q / stance, ANKLE, 0.0
    w = (q - stance) / (1 - stance)
    pace = -2 * step * (1 - stance) / stance  # the planted hoof's speed against the hip, per swing
    # A cubic from STEP behind to STEP ahead, leaving and arriving at that pace.
    fwd = step * (6 * w * w - 4 * w ** 3 - 1) + pace * (2 * w ** 3 - 3 * w * w + w)
    peak = g["peak"]
    rise = (w / peak) ** (4 * peak) * ((1 - w) / (1 - peak)) ** (4 * (1 - peak))  # 1 at the peak, flat at either end
    return fwd, ANKLE + g["lift"] * rise, math.sin(math.pi * w) ** 2


def _kernel(sigma):
    """A Gaussian of sigma samples: its offsets (samples) and weights (summing to 1)."""
    k = np.arange(-4 * int(sigma + 1), 4 * int(sigma + 1) + 1)
    kernel = np.exp(-0.5 * (k / sigma) ** 2)
    return k, kernel / kernel.sum()


def _smooth(c, sigma):
    """A loop's values smoothed round it (a Gaussian of sigma samples)."""
    k, kernel = _kernel(sigma)
    pad = len(k) // 2
    return np.convolve(np.concatenate([c[-pad:], c, c[:pad]]), kernel, mode="valid")


def _hang(c, dt, curve):
    """A loop's values under every parabola of curvature `curve` hung from one of them (dt: the samples' distances
    apart, squared, round the loop): as high as they can be while turning upward no more sharply than that."""
    return np.array([float(np.min(c + 0.5 * curve * np.roll(dt, i))) for i in range(len(c))])


def _carriage(g, sub=16):
    """The heights (m) of the front and hind hips through a gait's stride, `sub` samples a frame: each end rides as
    high as its planted hooves let a leg reach at REACH of its length, eased over a stretch where it has none down
    (rising `rise` in between), and kept under that bound while turning neither upward nor downward more sharply than
    `sway` (m/s/s) allows, smoothly round the loop: no corner where a hoof lands, so every planted hoof stays in reach
    and the barrel never jolts."""
    n, stance = g["frames"], g["stance"]
    m = n * sub
    cell = 1.0 / m
    ends = []
    for end in "fh":
        bound = np.full(m, np.inf)
        for leg in (k for k in LEGS if k[0] == end):
            for i in range(m):
                # The farthest the hoof is from under its hip while planted within this sample's cell.
                q = (i / m - g["phase"][leg]) % 1.0
                for lo, hi in ((q - cell / 2, q + cell / 2), (q - 1 - cell / 2, q - 1 + cell / 2)):
                    lo, hi = max(lo, 0.0), min(hi, stance)
                    if lo <= hi:
                        fwd = max(abs(_hoof(g, lo)[0]), abs(_hoof(g, min(hi, stance - 1e-9))[0]))
                        bound[i] = min(bound[i], ANKLE + math.sqrt((REACH * LEG) ** 2 - fwd * fwd))
        # Where no hoof of this end is down, from the height it left at to the one it lands at (turned so that a sample
        # with a hoof down comes first, and no stretch runs over the loop's end).
        shift = int(np.argmax(np.isfinite(bound)))
        c = np.roll(bound, -shift)
        free = np.flatnonzero(~np.isfinite(c))
        if len(free):
            for gap in np.split(free, np.flatnonzero(np.diff(free) > 1) + 1):
                a, b = c[gap[0] - 1], c[(gap[-1] + 1) % m]
                t = np.arange(1, len(gap) + 1) / (len(gap) + 1)
                c[gap] = a + (b - a) * t * t * (3 - 2 * t) + g["rise"] * np.sin(np.pi * t) ** 2
        c = np.roll(c, shift)
        # Rounded underneath: first the peaks where the end passes from one planted hoof's reach to the next's (under the
        # highest parabolas opening downward, half as sharp as `sway`, that fit beneath it), then the dips where a hoof
        # lands (under every parabola of curvature `sway` hung from a point of it), which leaves it turning neither way
        # more sharply than `sway`.
        dt = (np.minimum(np.arange(m), m - np.arange(m)) / (m / (n / FPS))) ** 2  # seconds apart, squared, round the loop
        sway = g["sway"]
        c = -_hang(-_hang(c, dt, sway / 2), dt, sway / 2)
        c = _hang(c, dt, sway)
        # Then smoothed round the loop, lowered first by as much as smoothing can raise a curve that turns upward no more
        # sharply than `sway` (half of it times the kernel's spread, in seconds, squared), so it stays under the bound
        # without being clipped back onto it, which would put a corner at every touchdown.
        k, kernel = _kernel(sub / 2)
        c = _smooth(c - 0.5 * sway * float((kernel * k * k).sum()) / (sub * FPS) ** 2, sub / 2)
        ends.append(c)
    return ends


def _turn(angle):
    """A turn in the horse's side plane (about Y): positive swings what hangs down toward the head, and the head up."""
    return Matrix.Rotation(-angle, 4, "Y")


def _down(v):
    """A vector's angle from straight down in the side plane, positive toward the head."""
    return math.atan2(v.x, -v.z)


def _key(arm, f, lift, pitch, nod, hooves, fold, last):
    """Keys every bone at frame f, set through armature-space matrices: the barrel raised by `lift` (m) and pitched nose
    up by `pitch` (radians) about the middle between its hips, the neck nodded up by `nod`, and each leg solved exactly
    from its hip where the barrel put it to its ankle at hooves[leg] = (forward of the hip at rest, height, folded 0..1),
    the knee bent forward on a foreleg and the hock back on a hind leg; the hoof flat, or folded up and back by up to
    `fold` (radians) past the lower leg. `last`: the quaternions keyed before, so each curve takes the short way."""
    B = arm.data.bones
    rot = {b.name: b.matrix_local.to_3x3().to_4x4() for b in B}
    pivot = Vector((0.0, 0.0, HIP * CM))
    moved = Matrix.Translation((0.0, 0.0, lift * CM)) @ Matrix.Translation(pivot) @ _turn(pitch) @ Matrix.Translation(-pivot)
    pose = {"root": B["root"].matrix_local.copy(), "barrel": moved @ B["barrel"].matrix_local}
    neck = B["neck"].head_local
    pose["neck"] = moved @ Matrix.Translation(neck) @ _turn(nod) @ Matrix.Translation(-neck) @ B["neck"].matrix_local
    for leg, (fwd, z, folded) in hooves.items():
        up, lo = B[leg + "_upper"], B[leg + "_lower"]
        l1, l2 = (up.tail_local - up.head_local).length, (lo.tail_local - lo.head_local).length
        hip = moved @ up.head_local
        ankle = Vector(((LEGS[leg][0] + fwd) * CM, up.head_local.y, z * CM))
        reach = ankle - hip
        dist = min(reach.length, (l1 + l2) * (1 - 1e-6))  # out of reach (only a swinging hoof can be): reached for
        off = math.acos(max(-1.0, min(1.0, (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist))))  # the thigh off that line
        thigh = _down(reach) + (off if leg[0] == "f" else -off)
        knee = hip + Vector((l1 * math.sin(thigh), 0.0, -l1 * math.cos(thigh)))
        shin = _down(ankle - knee)
        ankle = knee + Vector((l2 * math.sin(shin), 0.0, -l2 * math.cos(shin)))
        pose[up.name] = Matrix.Translation(hip) @ _turn(thigh - _down(up.tail_local - up.head_local)) @ rot[up.name]
        pose[lo.name] = Matrix.Translation(knee) @ _turn(shin - _down(lo.tail_local - lo.head_local)) @ rot[lo.name]
        pose[leg + "_hoof"] = Matrix.Translation(ankle) @ _turn(folded * (shin - fold)) @ rot[leg + "_hoof"]
    P = arm.pose.bones
    for b in B:
        if b.parent is None:
            continue
        local = (b.parent.matrix_local.inverted() @ b.matrix_local).inverted() @ pose[b.parent.name].inverted() @ pose[b.name]
        q = local.to_quaternion()
        if b.name in last and last[b.name].dot(q) < 0.0:
            q.negate()
        last[b.name] = q
        P[b.name].location = local.to_translation()
        P[b.name].rotation_quaternion = q
        P[b.name].keyframe_insert("location", frame=f)
        P[b.name].keyframe_insert("rotation_quaternion", frame=f)


def gait(arm, name):
    """Keys the gait `name` (GAITS): each hoof on its path, the barrel on the carriage its planted hooves allow, the neck
    nodding; KEYS a frame, the last key the first again."""
    g = GAITS[name]
    action = bpy.data.actions.new(name)
    arm.animation_data.action = action
    n = g["frames"]
    front, hind = _carriage(g)
    half = LEGS["fl"][0] - LEGS["hl"][0]
    amp, times, top = g["nod"]
    last = {}
    keys = n * KEYS
    for i in range(keys + 1):
        u = (i % keys) / keys
        j = (i % keys) * len(front) // keys
        hooves = {leg: _hoof(g, (u - g["phase"][leg]) % 1.0) for leg in LEGS}
        nod = math.radians(amp) * math.cos(2 * math.pi * times * (u - top))
        _key(arm, i / KEYS, (front[j] + hind[j]) / 2 - HIP, math.asin((front[j] - hind[j]) / half), nod, hooves, math.radians(g["fold"]), last)
    finish(action, n)


def idle(arm):
    """Standing square on straight legs, as the skin was made, grazing: the head down and up again over three seconds.
    (Lowered toward the walk's carriage the knees buckle, and the cross-fade into the walk dips a hoof no less.)"""
    action = bpy.data.actions.new("Idle")
    arm.animation_data.action = action
    n = FPS * 3
    last = {}
    for f in range(0, n + 1):
        ph = 2 * math.pi * f / n
        _key(arm, f, 0.0, 0.0, math.radians(-6 + 3 * math.sin(ph)), {leg: (0.0, ANKLE, 0.0) for leg in LEGS}, 0.0, last)
    finish(action, n)


def finish(action, n):
    """Keys straight from one frame to the next (as the game plays the baked frames), looping."""
    action.use_fake_user = True
    action.frame_range = (0, n)
    for fc in _fcurves(action):
        for k in fc.keyframe_points:
            k.interpolation = "LINEAR"
        fc.modifiers.new("CYCLES")


def _fcurves(action):
    """An action's curves: action.fcurves before Blender 5, the layers' strips' channel bags since."""
    if hasattr(action, "fcurves"):
        return list(action.fcurves)
    return [fc for layer in action.layers for strip in layer.strips for bag in getattr(strip, "channelbags", []) for fc in bag.fcurves]


def _play(arm, action):
    arm.animation_data.action = action
    if hasattr(arm.animation_data, "action_slot") and getattr(action, "slots", None) and len(action.slots):
        arm.animation_data.action_slot = action.slots[0]


def _runs(mask):
    """The stretches (first, past the last) where mask holds, but for any cut by its ends."""
    j = 1
    while j < len(mask):
        if mask[j] and not mask[j - 1]:
            k = j
            while k < len(mask) and mask[k]:
                k += 1
            if k < len(mask):
                yield j, k
            j = k
        else:
            j += 1


def check(arm, body, name):
    """Checks one rigged horseman; returns (problems, a report line for each gait). Fails when a leg bone, the barrel
    or the neck carries no vertices, or in Walk or Run (sampled SUBFRAMES times a frame): a hoof in its planted window
    (its ankle, the lower leg's tail, and its sole's vertices) moves more than HOLD cm along the ground (which runs back
    at the gait's speed) or off it; a hoof vertex goes more than SINK cm into the ground; the last frame is not the
    first (every bone within 0.01 cm and 0.01 degrees); the left hind hoof does not touch down at frame 0; or the barrel
    moves more than JOLT cm or degrees in a frame. Reports too how far the planted legs stretch (at most about REACH)."""
    problems = []
    groups = {g.index: g.name for g in body.vertex_groups}
    owner = [groups[max(v.groups, key=lambda g: g.weight).group] if len(v.groups) else "" for v in body.data.vertices]
    bare = [b.name for b in arm.data.bones if b.name != "root" and b.name not in owner]
    if bare:
        problems.append("no vertices on " + ", ".join(bare))
    rest = np.empty(len(owner) * 3)
    body.data.vertices.foreach_get("co", rest)
    rest = rest.reshape(-1, 3)
    hooves = {leg: np.array([i for i, o in enumerate(owner) if o == leg + "_hoof"], dtype=np.int64) for leg in LEGS}
    soles = {leg: i[rest[i, 2] < rest[i, 2].min() + 0.5] if len(i) else i for leg, i in hooves.items()}  # what stands on the ground
    every_hoof = np.concatenate(list(hooves.values()))
    scene = bpy.context.scene

    def posed(t):
        scene.frame_set(int(t), subframe=t - int(t))
        return arm.evaluated_get(bpy.context.evaluated_depsgraph_get()).pose.bones

    lines = []
    for act_name in ("Walk", "Run"):
        g = GAITS[act_name]
        act = bpy.data.actions.get(act_name)
        if act is None:
            problems.append("no " + act_name)
            continue
        _play(arm, act)
        n, speed, found = g["frames"], SPEEDS[act_name], []
        samples = n * SUBFRAMES
        ankle = {leg: np.empty((samples, 3)) for leg in LEGS}
        hip = {leg: np.empty((samples, 3)) for leg in LEGS}
        sole = {leg: np.empty((samples, len(soles[leg]), 3)) for leg in LEGS}
        sink = 1e9
        for j in range(samples):
            P = posed(j / SUBFRAMES)
            ev = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
            me = ev.to_mesh()
            co = np.empty(len(me.vertices) * 3)
            me.vertices.foreach_get("co", co)
            ev.to_mesh_clear()
            co = co.reshape(-1, 3)
            if len(every_hoof):
                sink = min(sink, float(co[every_hoof, 2].min()))
            for leg in LEGS:
                ankle[leg][j] = P[leg + "_lower"].tail
                hip[leg][j] = P[leg + "_upper"].head
                sole[leg][j] = co[soles[leg]]
        # Each planted window on the ground, which runs back at the gait's speed: over two laps, so a window across the
        # loop's seam counts whole.
        back = speed / FPS * np.arange(2 * samples) / SUBFRAMES
        slide = lift = stretch = 0.0
        for leg in LEGS:
            planted = [((j / SUBFRAMES / n - g["phase"][leg]) % 1.0) <= g["stance"] + 1e-9 for j in range(samples)] * 2
            a = np.tile(ankle[leg], (2, 1))
            a[:, 0] += back
            s = np.tile(sole[leg], (2, 1, 1))
            s[:, :, 0] += back[:, None]
            reach = np.tile(np.linalg.norm(ankle[leg] - hip[leg], axis=1), 2) / (LEG * CM)
            for j, k in _runs(planted):
                slide = max(slide, float(np.linalg.norm(a[j:k, :2] - a[j, :2], axis=1).max()))
                lift = max(lift, float(np.abs(a[j:k, 2] - ANKLE * CM).max()))
                if s.shape[1]:
                    slide = max(slide, float(np.linalg.norm(s[j:k, :, :2] - s[j, :, :2], axis=2).max()))
                    lift = max(lift, float(np.abs(s[j:k, :, 2] - rest[soles[leg], 2]).max()))
                stretch = max(stretch, float(reach[j:k].max()))
        if slide > HOLD or lift > HOLD:
            found.append("a planted hoof moves %.2f cm along the ground, %.2f cm off it" % (slide, lift))
        if sink < -SINK:
            found.append("a hoof goes %.2f cm into the ground" % -sink)
        # The loop: its last frame is its first.
        first = {b.name: b.matrix.copy() for b in posed(0)}
        end = {b.name: b.matrix.copy() for b in posed(n)}
        gap = max((first[b].to_translation() - end[b].to_translation()).length for b in first)
        turn = max(math.degrees(first[b].to_quaternion().rotation_difference(end[b].to_quaternion()).angle) for b in first)
        if gap > 0.01 or turn > 0.01:
            found.append("the loop does not close (%.3f cm, %.3f degrees)" % (gap, turn))
        # The left hind touches down at frame 0: on the ground then, and a frame before not yet still there.
        now, before = ankle["hl"][0], ankle["hl"][(n - 1) * SUBFRAMES]
        if abs(now[2] - ANKLE * CM) > HOLD or (abs(before[2] - ANKLE * CM) <= HOLD and abs(before[0] - speed / FPS - now[0]) <= HOLD):
            found.append("the left hind hoof does not touch down at frame 0")
        # The barrel, from one frame to the next.
        jolt = jolt_turn = 0.0
        was = None
        for f in range(n + 1):
            b = posed(f)["barrel"]
            at = (b.head.copy(), b.tail.copy(), b.matrix.to_quaternion())
            if was:
                jolt = max(jolt, (at[0] - was[0]).length, (at[1] - was[1]).length)
                jolt_turn = max(jolt_turn, math.degrees(at[2].rotation_difference(was[2]).angle))
            was = at
        if jolt > JOLT or jolt_turn > JOLT:
            found.append("the barrel jumps %.1f cm, %.1f degrees in a frame" % (jolt, jolt_turn))
        problems += ["%s: %s" % (act_name, p) for p in found]
        found = (["no vertices on " + ", ".join(bare)] if bare else []) + found
        lines.append("check %s %s: %s | %d frames %.1f cm/s slide %.2f cm lift %.2f cm sink %.2f cm barrel %.1f cm %.1f deg legs %.1f%%" % (
            name, act_name, "ok" if not found else "FAIL (" + "; ".join(found) + ")", n, speed, slide, lift, max(0.0, -sink), jolt,
            jolt_turn, 100 * stretch))
    return problems, lines


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    build = os.path.abspath(args[0] if args else os.path.join("art", "build"))
    wanted = args[1:] or list(RIDERS)
    out = os.path.join(build, "Rigged")
    os.makedirs(out, exist_ok=True)
    failed = []
    for name in wanted:
        arm, body = rig(name, RIDERS[name])
        problems, lines = check(arm, body, name)
        for line in lines:
            print(line)
        path = os.path.join(out, "SK_%s.fbx" % name)
        if problems:
            # Not exported, and an older export goes too, so ue_import_rigged.py cannot bring back a stale rig.
            failed.append(name)
            if os.path.exists(path):
                os.remove(path)
            continue
        bpy.ops.object.select_all(action="DESELECT")
        arm.select_set(True)
        body.select_set(True)
        bpy.context.view_layer.objects.active = arm
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, mesh_smooth_type="FACE",
                                 colors_type="SRGB", apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", add_leaf_bones=False, bake_anim=True,
                                 bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                                 bake_anim_step=1.0 / KEYS, bake_anim_simplify_factor=0.0, primary_bone_axis="Y", secondary_bone_axis="X",
                                 armature_nodetype="NULL")
        print("rigged %s: %d vertices -> %s" % (name, len(body.data.vertices), path))
    # The speed each loop was authored at (the same for every rider: they share one horse), for the game (SovGait.h).
    print("rig_riders: authored speeds " + ", ".join("%s %.1f cm/s" % (a, v) for a, v in SPEEDS.items()))
    if failed:
        print("rig_riders: not exported (any older SK_<Name>.fbx removed), failed the check: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
