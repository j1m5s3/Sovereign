"""The horsemen rigged for live battles and the map: the Figures kit's riders (the plain one and the four cultures'
dress) on a horse skeleton (barrel, neck, and each leg's upper part, lower part and hoof), with four looping actions
made here, in place: Walk, a four-beat walk (WALK_SPEED, about 92 cm/s); Trot, a two-beat trot on diagonal pairs
(TROT_SPEED, about 264 cm/s, a live battle's pace); Run, a transverse gallop on the right lead (RUN_SPEED, about 457
cm/s); and Idle, grazing. In every gait each hoof is planted while it bears weight and held still on the ground at the
gait's speed (each leg solved exactly as two bones, the hoof kept flat), and the barrel rides on the planted hooves
(the carriage): each end of it rides as high as its own planted legs reach, so the body bounces and rocks with the
stride and no planted leg is stretched past REACH of its length. The neck nods, the rider rides the barrel. Each gait
loop starts as the left hind hoof touches down, but their other footfalls do not line up (the walk and the trot land the
right hind together too, but not the fores, and a hoof stays down longer in the walk), so the game does not mix two at
one phase across the way from one speed to the other: it plays each alone, slowed or hurried to the speed asked, and
hands one to the next only in a narrow band of speed between theirs (SovGait.h kSovHorseHandoverBand), the walk to the
trot around 156 cm/s and the trot to the gallop around 347.

The rider has two bones of his own on the barrel: `rider` (his body, arms and head, leaning on his seat) and, on it,
`rider_arm` (what his hand holds: the lance with its point, pennant and tassels, or Rider_Asian's bow and string,
turning about its grip). In the loops they ride the barrel at rest, so the gaits are as they were before he had them.
They make the one-shots (ONCE), which the game plays in battles (USovLocomotion): Strike (24 frames, the blow at frame
10: the lance lowered from upright to couched and thrust, or the bow raised and loosed; only `rider` and what is under
it move, as the game plays a strike over the upper body alone), Hit (8 frames, a jolt back and a toss of the head, from
the rest pose back to it, no hoof moving) and Death and DeathBack (48 frames: the horse folds its forelegs and then its
hind legs and rolls onto its right or its left side on the ground, its legs lying on the ground and on one another,
the rider slumping and rolling with it, his lance let fall; still from frame 34). Each rider is checked (check: skin,
planted hooves held, ground, the loop closing, the barrel moving smoothly; check_once: the one-shots) before it is
exported as an FBX skeletal mesh with Walk, Trot, Run and Idle and the one-shots to <build>/Rigged/SK_Rider*.fbx,
alongside rig_figures.py's people, for tools/art/ue_import_rigged.py; one whose loops fail is not exported (an older
export of it is removed) and the script exits 1, and one that fails only check_once (or whose check_once raises) is
exported without the one-shots. It prints the speeds the gaits were authored at and the one-shots' frames and blow,
which the game's SovGait.h must match.

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
# The trot, a two-beat trot on diagonal pairs: the left hind lands with the right fore, and the right hind with the left
# fore half a stride later (TROT_PHASE), each pair planted for TROT_STANCE of the stride (lift-off on a key: 6 of its 16
# frames), then a brief moment with none down (2 frames) before the other pair lands. Its speed is near a live battle's
# (the battle sim's kSpeed, 260 cm/s), so a horseman there trots at about its own pace rather than galloping at half of
# it. Both ends of the barrel ride their planted legs alike, so it bounces twice a stride, highest as each pair passes
# under it, and hardly pitches (a real trot sinks at mid-stance on its fetlocks; here a planted leg could only shorten by
# bending its knee or hock deeply); the head nods a little against it, highest in the moment off the ground, so it rides
# steadier than the barrel.
TROT_FRAMES = 16
TROT_STANCE = 0.375
TROT_STEP = 0.33
TROT_LIFT = 0.2
TROT_FOLD = 60
TROT_SPEED = 2 * TROT_STEP / (TROT_STANCE * TROT_FRAMES / FPS) * CM  # cm/s, about 264 (SovGait.h kSovHorseTrotSpeed)
TROT_PHASE = {"hl": 0.0, "fr": 0.0, "hr": 0.5, "fl": 0.5}
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
         "Trot": dict(frames=TROT_FRAMES, stance=TROT_STANCE, step=TROT_STEP, lift=TROT_LIFT, fold=TROT_FOLD, phase=TROT_PHASE, peak=0.45,
                      rise=0.0, sway=10.0, nod=(2.0, 2, 0.4375)),
         "Run": dict(frames=RUN_FRAMES, stance=RUN_STANCE, step=RUN_STEP, lift=RUN_LIFT, fold=RUN_FOLD, phase=RUN_PHASE, peak=0.4,
                     rise=RUN_RISE, sway=40.0, nod=(8.0, 1, 0.1))}
# The speed (cm/s) each looping action was authored at: the game plays it at its own speed / this.
SPEEDS = {"Walk": WALK_SPEED, "Trot": TROT_SPEED, "Run": RUN_SPEED, "Idle": 0.0}
# The check (check): a planted hoof holds within HOLD cm of where it landed, on the ground; no hoof goes more than SINK
# cm into it; the barrel moves no more than JOLT cm (or degrees) a frame; sampled SUBFRAMES times a frame.
HOLD = 0.5
SINK = 0.5
JOLT = 3.0
SUBFRAMES = 4

# The rider's own bones (m): `rider` from the seat (where kit_figures._seated's torso starts) to the nape, on the
# barrel; `rider_arm` on it, from the grip (_held: the point of the lance's line nearest the hand, or the bow's grip)
# along what the hand holds. What rides them (bones_for): the seated torso, the neck, head and headgear and the arms (each arm's
# solids, and what is centred within ON_ARM of an arm's line: the hand's ball, Rider_African's shield on the forearm)
# ride `rider`; what lies wholly in the band HELD of y on the lance side (-Y) and rises above the seat (the lance with its
# point, pennant and tassels, Rider_Asian's bow and string) rides `rider_arm`; the legs down the flanks, the saddle and
# the cloth stay on the barrel, as before.
SEAT = Vector((0.0, 0.0, 1.55))
NAPE = Vector((0.0, 0.0, 2.05))
ARMS = [((0.0, s * 0.215, 1.37 + kit_figures.RIDE), (0.15, s * 0.26, 1.15 + kit_figures.RIDE)) for s in (-1, 1)]  # shoulder to hand
HAND = Vector(ARMS[0][1])  # the lance hand (on -Y)
ON_ARM = 0.15
HELD = (-0.40, -0.22)
TORSO = 0.3  # m: a solid above the seat centred within this of the rider's axis (in x and y) is his
SEAT_EDGE = (0.16, 0.10)  # m: how far ahead of and behind the seat, and to either side, his body's bottom reaches (he leans
# about its edge: _seat)
LANCE = 1.2  # m: what the hand holds is a lance if its longest solid is at least this long (else a bow)
# The one-shots: take: frames (the action's range, its last frame included), the frame the Strike's blow lands, and for
# a fall the frames to which the fore and the hind hooves stay planted and from which it lies still, and the side it
# goes down on (-1: its right, -Y, the lance side; 1: its left). Two keys a frame, as the loops.
ONCE = {"Strike": dict(frames=24, impact=10), "Hit": dict(frames=8),
        "Death": dict(frames=48, fore=2, hind=12, still=34, side=-1), "DeathBack": dict(frames=48, fore=2, hind=12, still=34, side=1)}
# Riders whose one-shots could not be keyed (once raised an error, under another Blender say): {name: why}, for
# check_once to report, so the rider is exported without them.
ERRORS = {}
# What each rider holds (_held): {name: dict(kind ("lance" or "bow"), grip and axis (cm, armature space at rest), and
# the vertices of its tip and butt and all of it)}.
ITEMS = {}


def _leg_of(co):
    """The leg a solid (its points, in metres) belongs to: every point of it within a hand's breadth of that leg's line
    (which leans forward to the knee) and below the top of the hip's ball; else None."""
    for leg, (lx, ly) in LEGS.items():
        if all(z < HIP + 0.1 and math.hypot(x - lx - KNEE_FWD * max(0.0, min(z / KNEE, (HIP - z) / (HIP - KNEE))), y - ly) < 0.1
               for x, y, z in co):
            return leg
    return None


def _islands(mesh):
    """The mesh's solids (each its own island of vertices joined by edges): lists of vertex indices."""
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
    return list(islands.values())


def bones_for(mesh):
    """Which bone carries each vertex ({vertex: bone}), solid by solid for the legs (every solid is its own island of the
    mesh, as in rig_figures.py), so none is torn between two bones: a leg's solid (_leg_of) rides its upper part (the
    thigh, and the balls of the hip and knee, each turning about its own centre), its hoof (the solid standing on the
    ground) or else its lower part (the cannon and the ankle's ball). The rider's solids go whole too: on an arm (_on_arm)
    or about his axis above the seat (the torso, neck, head and headgear) to `rider`, what his hand holds (in the band
    HELD, above the seat) to `rider_arm`. The rest goes vertex by vertex: the neck and head, else the barrel (the
    rider's legs, the saddle and the tack with it, and the barrel's own underside where the legs pass into it)."""
    out = {}
    for members in _islands(mesh):
        co = [mesh.vertices[i].co / CM for i in members]
        leg = _leg_of(co)
        if leg:
            low, high = min(c.z for c in co), max(c.z for c in co)
            part = "_upper" if (low + high) / 2 > KNEE - 0.05 else "_hoof" if low < 0.005 else "_lower"
            out.update((i, leg + part) for i in members)
            continue
        centre = sum(co, Vector()) / len(co)
        low, high = min(c.z for c in co), max(c.z for c in co)
        if high > SEAT.z and (_on_arm(centre) or (low >= SEAT.z - 0.01 and abs(centre.x) < TORSO and abs(centre.y) < TORSO)):
            out.update((i, "rider") for i in members)
        elif high > SEAT.z and all(HELD[0] <= c.y <= HELD[1] for c in co):
            out.update((i, "rider_arm") for i in members)
        else:
            out.update((i, "neck" if c.x > 0.72 and c.z > 1.22 and abs(c.y) < 0.26 else "barrel") for i, c in zip(members, co))
    return out


def _on_arm(p):
    """Whether a point (m) is within ON_ARM of one of the rider's arms (shoulder to hand)."""
    for a, b in ARMS:
        a, b = Vector(a), Vector(b)
        t = max(0.0, min(1.0, (p - a).dot(b - a) / (b - a).length_squared))
        if (p - a.lerp(b, t)).length < ON_ARM:
            return True
    return False


def _held(co, solids):
    """What the rider's hand holds (its solids, each a list of vertex indices of the rest coordinates `co`, cm, as rows):
    a lance if its longest solid (the shaft) is at least LANCE long, else a bow; its axis (unit, armature space) by
    principal components of the shaft, or of all of a bow, toward its end that is higher at rest (the lance's point, the
    bow's top); its grip, the point on a lance's axis nearest the hand (HAND), or the middle of a bow's front; and its tip
    and butt, the vertices furthest along its axis either way. None if he holds nothing (the one-shots then cannot be
    keyed, and check_once says so)."""
    if not solids:
        return None
    idx = np.array(sorted(i for solid in solids for i in solid), dtype=np.int64)
    pts = co[idx]

    def axis_of(p):
        _, _, vt = np.linalg.svd(p - p.mean(axis=0), full_matrices=False)
        return Vector(vt[0]) if vt[0][2] > 0.0 else -Vector(vt[0])

    size = [float(np.linalg.norm(np.ptp(co[solid], axis=0))) for solid in solids]  # each solid's bounding diagonal
    shaft = co[solids[int(np.argmax(size))]]
    axis = axis_of(shaft)
    kind = "lance" if max(size) / CM >= LANCE else "bow"
    if kind == "lance":
        centre = Vector(shaft.mean(axis=0))
        grip = centre + axis * (HAND * CM - centre).dot(axis)
    else:
        axis = axis_of(pts)
        grip = Vector(pts[pts[:, 0] > pts[:, 0].max() - 3.0].mean(axis=0))
    along = pts @ np.array(axis)
    return dict(kind=kind, grip=grip, axis=axis, tip=int(idx[along.argmax()]), butt=int(idx[along.argmin()]), held=idx)


def rig(name, make):
    kitlib.reset_scene()
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.unit_settings.scale_length = 0.01
    body = make().build()
    body.data.transform(Matrix.Scale(CM, 4))
    body.name = "SK_" + name
    owner = bones_for(body.data)
    co = np.empty(len(body.data.vertices) * 3)
    body.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    item = ITEMS[name] = _held(co, [s for s in _islands(body.data) if owner[s[0]] == "rider_arm"])
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
    # The rider's (after the horse's, which keep their order): he leans on his seat (_seat), and what he holds turns about
    # its grip (so a lance couched and thrust, or blended in and out by the game, swings about the hand, not the shoulder).
    rider = eb.new("rider")
    rider.head, rider.tail = SEAT * CM, NAPE * CM
    rider.parent = barrel
    held = eb.new("rider_arm")
    if item:
        held.head, held.tail = item["grip"], item["grip"] + item["axis"] * 20.0
    else:  # holding nothing: the bone at his hand, carrying nothing
        held.head, held.tail = HAND * CM, HAND * CM + Vector((0.0, 0.0, 20.0))
    held.parent = rider
    for b in eb:
        b.roll = 0.0
    bpy.ops.object.mode_set(mode="OBJECT")
    for b in arm_data.bones:
        body.vertex_groups.new(name=b.name)
    for i, bone in owner.items():
        body.vertex_groups[bone].add([i], 1.0, "REPLACE")
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    arm.animation_data_create()
    for b in arm.pose.bones:
        b.rotation_mode = "QUATERNION"
    gait(arm, "Walk")
    gait(arm, "Trot")
    gait(arm, "Run")
    idle(arm)
    ERRORS.pop(name, None)
    try:
        once(arm, body, name)
    except Exception as e:
        # (under another Blender, say): its loops stand, the one-shots keyed so far go, and check_once reports why, so
        # the rider is exported without them and the riders after it are still rigged.
        for action in [a for a in bpy.data.actions if a.name.split(".")[0] in ONCE]:
            bpy.data.actions.remove(action)
        ERRORS[name] = "%s: %s" % (type(e).__name__, e)
    _play(arm, bpy.data.actions["Idle"])
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
        hip = moved @ up.head_local
        thigh, knee, shin, ankle = _reach(up, lo, hip, Vector(((LEGS[leg][0] + fwd) * CM, up.head_local.y, z * CM)), leg[0] == "f")
        pose[up.name] = Matrix.Translation(hip) @ _turn(thigh - _down(up.tail_local - up.head_local)) @ rot[up.name]
        pose[lo.name] = Matrix.Translation(knee) @ _turn(shin - _down(lo.tail_local - lo.head_local)) @ rot[lo.name]
        pose[leg + "_hoof"] = Matrix.Translation(ankle) @ _turn(folded * (shin - fold)) @ rot[leg + "_hoof"]
    # The rider and what he holds ride the barrel, at rest.
    _set_keys(arm, f, pose, last, ("rider", "rider_arm"))


def _reach(up, lo, hip, ankle, front):
    """A leg (its upper and lower bones) solved exactly as two bones from its hip to its ankle at `ankle` (armature space,
    cm, in its side plane), the knee bent forward on a foreleg (`front`) and the hock back on a hind leg: the thigh's and
    the shin's angles from straight down (_down), the knee and the ankle reached."""
    l1, l2 = (up.tail_local - up.head_local).length, (lo.tail_local - lo.head_local).length
    reach = ankle - hip
    dist = min(reach.length, (l1 + l2) * (1 - 1e-6))  # out of reach (only a swinging hoof can be): reached for
    off = math.acos(max(-1.0, min(1.0, (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist))))  # the thigh off that line
    thigh = _down(reach) + (off if front else -off)
    knee = hip + Vector((l1 * math.sin(thigh), 0.0, -l1 * math.cos(thigh)))
    shin = _down(ankle - knee)
    return thigh, knee, shin, knee + Vector((l2 * math.sin(shin), 0.0, -l2 * math.cos(shin)))


def _set_keys(arm, f, pose, last, rest=()):
    """Keys every bone but the root at frame f from its armature-space matrix (pose: {bone: matrix}), as its parent's
    leaves it; a bone named in `rest` exactly at rest. `last`: the quaternions keyed before, so each curve takes the
    short way."""
    B, P = arm.data.bones, arm.pose.bones
    for b in B:
        if b.parent is None:
            continue
        if b.name in rest:
            local = Matrix.Identity(4)
        else:
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


def finish_once(action, n):
    """A one-shot: kept (fake user) over frames 0..n, its keys straight from one to the next (as the game plays the baked
    frames), not looping (no CYCLES modifier, which finish gives every loop's curves)."""
    action.use_fake_user = True
    action.frame_range = (0, n)
    for fc in _fcurves(action):
        for k in fc.keyframe_points:
            k.interpolation = "LINEAR"


# ------------------------------------------------------------------------------------------------------- the one-shots
# The Strike with a lance, turned about its grip (where `rider_arm` sits): lowered from upright to couched by frame 8,
# easing into the couch, its point LANCE_DOWN degrees below level and its grip at COUCH, under the hand, drawn back DRAW
# through the hand as it comes level; after a moment's pause (to POISED) thrust from there THRUST along itself at the
# blow (10), fastest as it lands (the fastest the point moves in the take), and FOLLOW on by 12, the rider leaning in
# LEAN; raised again from 13 and at rest at 24. From frame 4 to 18 its line runs through the hand (the grip at COUCH),
# so it turns and slides in his fist rather than leaving it. Its butt must pass over the saddle cloth (which reaches
# about 35 cm ahead of the seat) and the rider's knee, never through them: while it is steeper than about 17 degrees
# its butt would come down through the cloth unless the hand is near enough to it, so the lance is first slid up
# through the hand (hoisted, while it is still steep) as it comes to the hand, and slides home only as it swings down
# past about 20 degrees; raised again, it is slid up through the hand as it swings up past that, and once upright (21)
# leaves the hand for where it rests and slides home (by 24). The raise is slower than the swing down, and both are
# slower than the thrust as it lands. After the blow FOLLOW carries the point a few centimetres on, as a man's thrust
# follows through (and as it is raised again, slid up through the hand, it goes out once more, as far to within a few
# millimetres, 15-16.5); before the blow it is short of it (high in the swing down, and well short once couched). The
# hand takes it only at 2.5-4 and lets go of it at 19-21: at rest the kit's lance stands 39.5 cm off the fist with its
# butt under the saddle cloth's overhang, beside the knee, and its line can come to the fist only once the butt is up
# past the cloth's front edge. Keyed as (frame, value) through a monotone cubic (_spline): `turn` the share of the way
# from upright (0) to couched (1); `slide` how far up through the hand (m, toward the point; the thrust adds to it from
# 8 to 13); `grip` where the grip is, from the couch (0) to where it is at rest (1); `rise` how far the grip is raised
# (m).
LANCE_DOWN = 6.0
DRAW = 0.2
COUCH = Vector((HAND.x, 0.0, HAND.z - 0.06))  # m: x and z (the lance keeps its own y)
LANCE_KEYS = {
    "turn": ([(0, 0.0), (1.5, 0.06), (2.5, 0.2), (4, 0.45), (5.5, 0.7), (7, 0.93), (8, 1.0)],
             [(13, 1.0), (15.5, 0.85), (18.5, 0.45), (21, 0.1), (22.5, 0.0)]),
    "slide": ([(0, 0.0), (2.5, 0.62), (4, 0.55), (5, 0.41), (5.5, 0.33), (7, -0.1), (8, -DRAW)],
              [(13, 0.33), (15, 0.3), (17, 0.45), (19, 0.6), (21.5, 0.55), (24, 0.0)]),
    "grip": ([(0, 1.0), (2, 1.0), (4, 0.0), (8, 0.0)], [(13, 0.0), (18, 0.0), (21.5, 1.0)]),
    "rise": ([(0, 0.0), (2, 0.12), (4, 0.0), (8, 0.0)], [(13, 0.0), (18, 0.0), (20, 0.15), (22, 0.1), (24, 0.0)]),
}
POISED = 8.5
THRUST = 0.30
FOLLOW = 0.03
LEAN = 10.0
# Without a lance (Rider_Asian's bow, already drawn as the kit holds it): the bow brought up BOW_RAISE degrees about the
# shoulder by 8, the rider leaning in BOW_LEAN; loosed at 10, the bow's top snapping forward BOW_SNAP about its grip in
# a frame and the bow pushed BOW_PUSH cm forward, rising on a little, as he rocks back; at rest by 24. (His arm does
# not move with it, as the kit holds the bow 47 cm ahead of his hand, so the bow's swing is what shows the loose: a
# large one, about 38 degrees in that frame, his rocking back taking some of BOW_SNAP off it.)
BOW_RAISE = 20.0
BOW_LEAN = 6.0
BOW_SNAP = 50.0
BOW_PUSH = 12.0
# The falls (_death), degrees: the forelegs' joints kneeling (by frame 12, the barrel pitched KNEEL nose-down) and
# folded under (22), the hind legs' folded (22), and every leg's lying out (SETTLED); the roll onto the side. The roll
# starts at 20 and lands at IMPACT, LANDED of the way over and falling faster as it goes; it settles the rest of the way
# by SETTLED, slowing to a stop, and the neck, the rider and the legs land and settle with it. The legs stay folded
# until UNFOLD and then lie out as it lands; turned only in their side planes they would stand off the ground (the
# horse on its side, its upper legs held over the lower), so as it lands (IMPACT to SETTLED) each also drops about the
# barrel's line through its hip (by up to SPLAY): the pair on the ground's side until it lies on the ground, then the
# upper pair until it rests on those or the ground. While it rolls, a leg on the ground gives way (its thigh turned
# back, or a foreleg's forward, by up to GIVE) rather than holding the horse up, so the barrel only comes down.
KNEEL = -15.0
FORE_KNEEL = (6.6, -82.7, -38.9)
FORE_FOLDED = (70.0, -150.0, -30.0)
HIND_FOLDED = (-49.4, 147.0, 10.0)
FORE_OUT = (25.0, -20.0, -10.0)
HIND_OUT = (-25.0, 20.0, 10.0)
ROLL = 80.0
IMPACT, SETTLED = 31, 33
LANDED = 0.95
UNFOLD = 24
GIVE = 90.0
SPLAY = 45.0
SLACK = 2.0  # frames: a leg gives way, and comes back, over at least this long
FLOOR = 0.5   # cm: where a fallen horse's lowest point lies
# What the rider holds, let go as he is hit: falling from frame DROP[0] to DROP[1], to lie DROP_OUT degrees out from
# straight ahead, its grip DROP_AHEAD cm ahead of where it was, and DROP_GAP cm clear of horse and rider.
DROP = (4, 20)
DROP_OUT = 10.0
DROP_GAP = 15.0
DROP_AHEAD = 20.0  # cm: its grip this far ahead of where it was at rest
AXIS = Vector((0.0, 0.0, 122.0))  # cm: on the barrel's line, which it rolls about


def _ease(kind, u):
    """How far along a stretch between two keys a value is at u (0..1): "in" starting slowly, "out" ending slowly,
    "inout" both."""
    return u * u if kind == "in" else 1.0 - (1.0 - u) ** 2 if kind == "out" else u * u * (3.0 - 2.0 * u)


def _curve(keys, t):
    """The value at frame t of keys [(frame, value) or (frame, value, ease)]: held before the first and after the last,
    eased between two by the later one's ease ("inout" unless given). Values are numbers or numpy arrays."""
    if t <= keys[0][0]:
        return keys[0][1]
    for (f0, v0, *_), (f1, v1, *ease) in zip(keys, keys[1:]):
        if t <= f1:
            return v0 + (v1 - v0) * _ease(ease[0] if ease else "inout", (t - f0) / (f1 - f0))
    return keys[-1][1]


def _land(was, value):
    """A fall's last keys for a value going from `was` to `value`: LANDED of the way at IMPACT, falling faster as it goes,
    and the rest by SETTLED, slowing to a stop."""
    return [(IMPACT, was + (value - was) * LANDED, "in"), (SETTLED, value, "out")]


def _about(point, rot):
    """A turn `rot` (a 4x4 matrix) about `point`."""
    return Matrix.Translation(point) @ rot @ Matrix.Translation(-point)


def _carry(lift, pitch, roll=0.0):
    """The barrel's move from rest: raised `lift` cm and pitched nose up by `pitch` (radians) about the middle between its
    hips, as _key moves it, after rolling `roll` about its own line (positive: onto its right side, -Y)."""
    return Matrix.Translation((0.0, 0.0, lift)) @ _about(Vector((0.0, 0.0, HIP * CM)), _turn(pitch)) @ _about(AXIS, Matrix.Rotation(roll, 4, "X"))


def _posed(B, moved, neck, legs, rider, held):
    """Every bone's armature-space matrix: the barrel moved by `moved` from rest; the neck turned by `neck` (a rotation in
    the barrel's frame, about the neck's root); each leg's joints turned by legs[leg] = (upper, lower, hoof) (radians,
    each against the bone above it, in the barrel's side plane, as _turn turns), and, given a fourth, the whole leg first
    turned out of that plane about the barrel's line through its hip by so much (positive: its hoof toward +Y); the
    rider moved by `rider` (in the barrel's frame) and what he holds by `held` (in his)."""
    pose = {"root": B["root"].matrix_local.copy(), "barrel": moved @ B["barrel"].matrix_local}
    pose["neck"] = moved @ _about(B["neck"].head_local, neck) @ B["neck"].matrix_local
    for leg, turns in legs.items():
        m = moved
        if len(turns) > 3:
            m = m @ _about(B[leg + "_upper"].head_local, Matrix.Rotation(turns[3], 4, "X"))
        for part, a in zip(("_upper", "_lower", "_hoof"), turns):
            b = B[leg + part]
            m = m @ _about(b.head_local, _turn(a))
            pose[b.name] = m @ b.matrix_local
    pose["rider"] = moved @ rider @ B["rider"].matrix_local
    pose["rider_arm"] = moved @ rider @ held @ B["rider_arm"].matrix_local
    return pose


def _planted(B, leg, moved, pitch):
    """The joint turns (as _posed's legs) that keep a leg's hoof standing flat where it stands at rest while the barrel is
    moved by `moved` (pitched by `pitch`, not rolled): the leg solved exactly as two bones (_reach)."""
    up, lo = B[leg + "_upper"], B[leg + "_lower"]
    thigh, knee, shin, ankle = _reach(up, lo, moved @ up.head_local, lo.tail_local.copy(), leg[0] == "f")
    turn_up, turn_lo = thigh - _down(up.tail_local - up.head_local), shin - _down(lo.tail_local - lo.head_local)
    return (turn_up - pitch, turn_lo - turn_up, -turn_lo)


class _Skin:
    """The body as the armature deforms it (cm, a row per vertex), each vertex riding the one bone it is bound to, for a
    pose (_posed's matrices): what the floor pass and the falls' fitting measure."""

    def __init__(self, arm, body):
        groups = {g.index: g.name for g in body.vertex_groups}
        owner = [groups[v.groups[0].group] for v in body.data.vertices]
        co = np.empty(len(owner) * 3)
        body.data.vertices.foreach_get("co", co)
        self.co = np.hstack([co.reshape(-1, 3), np.ones((len(owner), 1))])
        self.parts = {b.name: np.array([i for i, o in enumerate(owner) if o == b.name], dtype=np.int64) for b in arm.data.bones}
        self.inv = {b.name: b.matrix_local.inverted() for b in arm.data.bones}

    def __call__(self, pose, bones=None):
        out = []
        for b in bones or self.parts:
            idx = self.parts[b]
            if len(idx):
                out.append(self.co[idx] @ np.array(pose[b] @ self.inv[b])[:3].T)
        return np.concatenate(out) if out else np.zeros((0, 3))

    def low(self, pose, bones=None):
        return float(self(pose, bones)[:, 2].min())


class _Legs:
    """The horse's legs as solids, for a fallen horse's legs lying on the ground or on one another: each leg's solids'
    hulls at rest on their bones (_hull), and points on its surface (its vertices, and along its edges at most ALONG cm
    apart), each riding its bone. Built from the body as rig binds it (each vertex on one bone)."""

    def __init__(self, arm, body):
        groups = {g.index: g.name for g in body.vertex_groups}
        owner = [groups[v.groups[0].group] for v in body.data.vertices]
        rest = np.empty(len(owner) * 3)
        body.data.vertices.foreach_get("co", rest)
        rest = rest.reshape(-1, 3)
        self.inv = {b.name: b.matrix_local.inverted() for b in arm.data.bones}
        self.solids = {leg: [] for leg in LEGS}
        self.points = {leg: [] for leg in LEGS}
        for members in _islands(body.data):
            bone = owner[members[0]]
            if bone[:2] not in LEGS or len(members) < 4:
                continue
            self.solids[bone[:2]].append((bone,) + _hull(rest[members]))
            a, b, t = _along(body, members, rest)
            self.points[bone[:2]].append((bone, rest[a] + (rest[b] - rest[a]) * t))

    def _carried(self, pose, bone):
        """The rigid move (rotation, translation: numpy) that carries bone's rest frame to where `pose` puts it."""
        D = np.array(pose[bone] @ self.inv[bone])
        return D[:3, :3], D[:3, 3]

    def gap(self, pose, leg, under=None):
        """How far leg's lowest point is above the ground, or, where less, above the solids of the legs `under` (all
        the others unless given) straight below it: the least drop that would bring it down onto one (negative where it
        is in one)."""
        pts = []
        for bone, p in self.points[leg]:
            R, T = self._carried(pose, bone)
            pts.append(p @ R.T + T)
        pts = np.concatenate(pts)
        best = float(pts[:, 2].min())
        for other in (under if under is not None else [k for k in LEGS if k != leg]):
            for bone, normals, offsets in self.solids[other]:
                R, T = self._carried(pose, bone)
                local = (pts - T) @ R  # into the bone's rest frame
                d = -R[2]  # straight down, in that frame
                above = local @ normals.T - offsets  # > 0 outside a face's plane
                along = normals @ d
                with np.errstate(divide="ignore", invalid="ignore"):
                    t = -above / along
                enter = np.where(along < -1e-9, t, -np.inf).max(axis=1)
                leave = np.where(along > 1e-9, t, np.inf).min(axis=1)
                miss = ((np.abs(along) <= 1e-9) & (above > 0.0)).any(axis=1)
                hit = (enter <= leave) & (leave >= 0.0) & ~miss
                if hit.any():
                    best = min(best, float(enter[hit].min()))
        return best


def _bisect(f, lo, hi, want, steps=30):
    """x in lo..hi where f(x), rising with x, reaches `want` (lo or hi when it never does)."""
    for _ in range(steps):
        mid = 0.5 * (lo + hi)
        if f(mid) < want:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


HORSE = ("barrel", "fl_upper", "fl_lower", "fl_hoof", "fr_upper", "fr_lower", "fr_hoof", "hl_upper", "hl_lower", "hl_hoof", "hr_upper",
         "hr_lower", "hr_hoof")
STILL_LEGS = {leg: (0.0, 0.0, 0.0) for leg in LEGS}


def _spline(keys, t):
    """The value at frame t of keys [(frame, value)] through a monotone cubic (Fritsch and Carlson's): smooth through
    each key, never overshooting one, coming to rest at the first and the last, held outside them."""
    xs, ys = [k[0] for k in keys], [k[1] for k in keys]
    if t <= xs[0]:
        return ys[0]
    if t >= xs[-1]:
        return ys[-1]
    d = [(ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]) for i in range(len(xs) - 1)]
    m = [0.0] * len(xs)
    for i in range(1, len(xs) - 1):
        if d[i - 1] * d[i] > 0.0:
            m[i] = 3.0 * (xs[i + 1] - xs[i - 1]) / ((2.0 * xs[i + 1] - xs[i] - xs[i - 1]) / d[i - 1] + (xs[i + 1] + xs[i] - 2.0 * xs[i - 1]) / d[i])
    i = max(j for j in range(len(xs) - 1) if xs[j] <= t)
    h = xs[i + 1] - xs[i]
    u = (t - xs[i]) / h
    return ((2 * u ** 3 - 3 * u * u + 1) * ys[i] + (u ** 3 - 2 * u * u + u) * h * m[i] + (3 * u * u - 2 * u ** 3) * ys[i + 1]
            + (u ** 3 - u * u) * h * m[i + 1])


def _lance(item, rider, t):
    """The lance (as _posed's `held`, in the rider's frame `rider`) at frame t of the Strike (LANCE_KEYS): turned about
    its grip from upright toward couched, slid up through the hand (or thrust), its grip moved between where it is at
    rest and the couch and raised."""
    G0, d0 = item["grip"], item["axis"]
    keys = {k: v[0] if t <= 8 else v[1] for k, v in LANCE_KEYS.items()}
    slide = _spline(keys["slide"], t)
    if 8 < t <= 13:
        drawn = LANCE_KEYS["slide"][0][-1][1]
        slide = _curve([(8, drawn), (POISED, drawn), (10, THRUST, "in"), (12, THRUST + FOLLOW, "out"), (13, THRUST + FOLLOW)], t)
    turn = Matrix.Rotation(_spline(keys["turn"], t) * (math.asin(max(-1.0, min(1.0, d0.z))) + math.radians(LANCE_DOWN)), 4, "Y")
    at = Vector((COUCH.x * CM, G0.y, COUCH.z * CM)).lerp(G0, _spline(keys["grip"], t)) + Vector((0.0, 0.0, _spline(keys["rise"], t) * CM))
    return rider.inverted() @ Matrix.Translation(rider @ at) @ turn @ Matrix.Translation(d0 * slide * CM - G0)


def _seat(lean, over=0.0):
    """The rider's move (in the barrel's frame): leaning `lean` (radians, forward; negative, back) and rolled `over`
    (about the barrel's line, positive onto its right side, -Y), about the edge of his seat he leans or rolls toward
    (SEAT_EDGE ahead of the seat or behind it, and to its side, each eased from one side to the other over the first
    degrees), so the bottom of his body rocks on that edge rather than sinking into the barrel."""
    edge = Vector((SEAT_EDGE[0] * math.tanh(lean / math.radians(2.0)), -SEAT_EDGE[1] * math.tanh(over / math.radians(2.0)), 0.0))
    return _about((SEAT + edge) * CM, Matrix.Rotation(over, 4, "X") @ Matrix.Rotation(lean, 4, "Y"))


def strike(B, item, t):
    """The Strike at frame t: (rider, held) as _posed takes them (the horse stays at rest)."""
    if item["kind"] == "lance":
        lean = _curve([(0, 0.0), (8, 3.0), (10, LEAN, "in"), (13, LEAN - 2.0, "out"), (24, 0.0)], t)
        rider = _seat(math.radians(lean))
        return rider, _lance(item, rider, t)
    lean = _curve([(0, 0.0), (8, BOW_LEAN), (10, BOW_LEAN), (11.5, -BOW_LEAN / 2.0, "out"), (15, -1.0), (24, 0.0)], t)
    up = _curve([(0, 0.0), (8, BOW_RAISE), (10, BOW_RAISE), (11.5, BOW_RAISE + 4.0, "out"), (24, 0.0)], t)
    snap = _curve([(0, 0.0), (10, 0.0), (11, BOW_SNAP, "out"), (14, BOW_SNAP / 3.0), (24, 0.0)], t)
    push = _curve([(0, 0.0), (10, 0.0), (11, BOW_PUSH, "out"), (16, 0.0)], t)
    rider = _seat(math.radians(lean))
    raised = _about(Vector(ARMS[0][0]) * CM, Matrix.Rotation(-math.radians(up), 4, "Y"))
    return rider, Matrix.Translation((push, 0.0, 0.0)) @ raised @ _about(item["grip"], Matrix.Rotation(math.radians(snap), 4, "Y"))


def hit(t):
    """The Hit at frame t: (the neck's nod, the rider's move); the rest stays at rest."""
    lean = _curve([(0, 0.0), (2, -8.0, "out"), (8, 0.0)], t)
    nod = _curve([(0, 0.0), (2, 10.0, "out"), (8, 0.0)], t)
    return math.radians(nod), _seat(math.radians(lean))


def _rotation_onto(rest, want):
    """The turn (4x4) taking the frame `rest` (two orthogonal unit vectors: along and across) onto `want`."""
    def basis(a, b):
        return Matrix((a, b, a.cross(b))).transposed()
    return (basis(*want) @ basis(*rest).transposed()).to_4x4()


def _death(B, skin, legs, item, take):
    """A fall (Death or DeathBack, ONCE[take]): a function of the frame giving every bone's matrix. The horse jolts as it
    is hit (0-3), the rider jolted back; the forelegs fold to a kneel (from frame `fore` to 12), the body dropping onto
    the knees as they fold, nose-down KNEEL, while the hind hooves stay planted (to `hind`); the hind legs fold under and
    the barrel comes down onto them (22), the rider slumping forward; it rolls ROLL onto its side (from 20, falling
    faster as it goes, landing at IMPACT and settling by SETTLED: _land), the legs lying out from UNFOLD, the neck
    bending down to the ground and the rider rolling on with it onto the ground beside it; still from `still`. The rider
    lets go of what he holds as he is hit: it tips over and falls (DROP) to lie flat on the ground on its own side, clear
    of where horse and rider end up. From the moment the forelegs fold it is a floor pass that sets how high the horse
    is: never into the ground, the planted hind hooves apart, and from `hind` on resting on it (its lowest point FLOOR
    up), so it rolls over on the ground rather than through it or above it. Once it is down on its folded legs (22) the
    barrel only comes down as it rolls, from there to where it lies, and no lower than the rest of it lets it lie; a leg
    that would hold it up instead gives way (its thigh turned back, or a foreleg's forward), its hoof sliding out along
    the ground. As it lands each leg drops about the barrel's line to lie on the ground or on the leg under it. How far
    the knees take it down, the neck bends, the rider rolls on, a leg gives way and a leg drops are found by measuring
    the posed body (_Skin; `legs`, a _Legs, for a leg resting on another)."""
    spec = ONCE[take]
    fall = -spec["side"]  # the roll's sign (_carry): positive onto its right side (-Y)
    fore, hind = spec["fore"], spec["hind"]
    rad = math.radians
    parts = {leg: [leg + part for part in ("_upper", "_lower", "_hoof")] for leg in LEGS}
    hinds = [b for leg in LEGS if leg[0] == "h" for b in parts[leg]]
    fores = [b for leg in LEGS if leg[0] == "f" for b in parts[leg]]
    # What the floor pass measures: all but what the rider holds (let go of, it falls on its own), and while the hind
    # hooves are planted, all but the hind legs; and while it rolls, all but the legs (core), which give way instead.
    body = [b.name for b in B if b.name not in ("root", "rider_arm")]
    rest = [b for b in body if b not in hinds]
    core = [b for b in rest if b not in fores]
    I = Matrix.Identity(4)

    def over_by(t):
        """How far over it has rolled at frame t, as a share of the roll."""
        return _curve([(0, 0.0), (20, 0.0)] + _land(0.0, 1.0), t)

    def shape(t, s, lift, give=None):
        """The pose at frame t with the barrel raised `lift` cm, what the rider holds still in his hand, and each leg in
        `give` given way by so much (radians)."""
        pitch = rad(_curve([(0, 0.0), (3, 0.0), (12, KNEEL), (22, -4.0)] + _land(-4.0, 0.0), t))
        moved = _carry(lift, pitch, rad(fall * s["roll"] * over_by(t)))
        legs = {}
        for leg in LEGS:
            front = leg[0] == "f"
            until = fore if front else hind
            if t <= until:
                legs[leg] = _planted(B, leg, moved, pitch)
                continue
            # Folded (the hind legs' thighs tucked forward by `tuck`, their shins kept as they were), then lying out.
            folded = np.radians(FORE_FOLDED) if front else np.radians(HIND_FOLDED) + s["tuck"] * np.array((1.0, -1.0, 0.0))
            keys = [(until, np.array(s["start"][leg]))]
            if front:
                keys.append((12, np.radians(FORE_KNEEL), "out"))
            keys += [(22, folded), (UNFOLD, folded)] + _land(folded, np.radians(FORE_OUT if front else HIND_OUT))
            turns = _curve(keys, t)
            if give and give.get(leg):
                # Given way: the thigh turned back (a hind leg) or forward (a foreleg), the shin kept as it was, so the
                # knee and all below it rise.
                g = give[leg] if front else -give[leg]
                turns = turns + np.array((g, -g, 0.0))
            # And as it lands, dropped about the barrel's line toward the ground (out of its side plane).
            legs[leg] = tuple(turns) + (s["splay"][leg] * _curve([(IMPACT, 0.0), (SETTLED, 1.0)], t),)
        nod = rad(_curve([(0, 0.0), (3, 12.0, "out"), (12, -20.0), (22, -30.0)] + _land(-30.0, -45.0), t))
        bend = rad(_curve([(0, 0.0), (20, 0.0)] + _land(0.0, s["bend"]), t))
        neck = Matrix.Rotation(-fall * bend, 4, "Z") @ _turn(nod)
        lean = rad(_curve([(0, 0.0), (3, -8.0, "out"), (12, 20.0), (22, 25.0)] + _land(25.0, 10.0), t))
        over = rad(_curve([(0, 0.0), (12, 0.0), (22, 8.0)] + _land(8.0, s["over"]), t))
        rider = _seat(lean, fall * over)
        return _posed(B, moved, neck, legs, rider, I)

    def lift_at(t, s):
        """The floor pass: how high the barrel is at frame t."""
        if t <= fore:
            return 0.0
        if t <= hind:
            # The knees come down as they fold (the drop lagging the fold), never into the ground; the planted hind
            # legs reach for their hooves wherever the barrel is.
            low = _curve([(fore, 0.0), (12, s["L12"], "in")], t)
            return low + max(0.0, -skin.low(shape(t, s, low), rest))
        if t <= 22:
            # Its lowest point (a hind sole, on the ground until now) eased up to FLOOR as the hind legs fold.
            return _curve([(hind, 0.0), (22, FLOOR)], t) - skin.low(shape(t, s, 0.0), body)
        # Rolling: never higher than before, so from where its folded legs held it (22) it only comes down, as far as it
        # has rolled over, to where it lies; and no lower than its barrel, neck and rider let it, now or later.
        return max([floor(t, s)] + [h for u, h in s["floors"] if u > t])

    def floor(t, s):
        """How low the barrel may be at frame t > 22 for its barrel, neck and rider (all but the legs) to stay out of the
        ground, or for it to come down from where it was at 22 no faster than it rolls over."""
        down = (over_by(t) - over_by(22)) / (1.0 - over_by(22))
        return max(FLOOR - skin.low(shape(t, s, 0.0), core), s["L22"] + (s["L_end"] - s["L22"]) * down)

    def needs(t, s):
        """How far each leg must give way at frame t (radians) to stay out of the ground: none, or just enough."""
        out = {}
        lift = lift_at(t, s)
        for leg in LEGS:
            def low(g):
                return skin.low(shape(t, s, lift, {leg: g}), parts[leg])
            out[leg] = _bisect(low, 0.0, rad(GIVE), FLOOR) if low(0.0) < FLOOR else 0.0
        return out

    def at(t, s):
        lift = lift_at(t, s)
        give = {}
        if t > 22:
            # As far as it must at any key, or at one within SLACK frames, less the further off (so it gives way and
            # comes back no faster than that allows).
            near = [(1.0 - abs(u - t) / SLACK, g) for u, g in s["needs"] if abs(u - t) < SLACK]
            give = {leg: max([0.0] + [w * g[leg] for w, g in near]) for leg in LEGS}
        pose = shape(t, s, lift, give)
        if t > DROP[0]:
            # Let go: from where it was in his hand, tipping over and falling (faster as it goes) to where it lies, and
            # from the first moving out to its side (sooner than it falls), so it tips over clear of the saddle and his
            # leg rather than past them.
            v = min(1.0, (t - DROP[0]) / (DROP[1] - DROP[0]))
            u = _ease("in", v)
            start, end = s["let go"], s["lies"]
            turn = start.to_quaternion().slerp(end.to_quaternion(), u).to_matrix().to_4x4()
            was, lies = start @ item["grip"], end @ item["grip"]
            grip = was.lerp(lies, u)
            grip.y = was.y + (lies.y - was.y) * _ease("inout", v)
            world = Matrix.Translation(grip) @ turn @ Matrix.Translation(-item["grip"])
            pose["rider_arm"] = world @ B["rider_arm"].matrix_local
        return pose

    # Where each leg's fold starts: as it stood planted (the forelegs at rest; the hind legs under the kneeling horse).
    s = dict(L12=0.0, roll=ROLL, bend=0.0, over=0.0, tuck=0.0, start={leg: (0.0, 0.0, 0.0) for leg in LEGS}, floors=[], needs=[],
             splay={leg: 0.0 for leg in LEGS})
    # Kneeling at 12: the fore knees on the ground, the hind hooves planted.
    s["L12"] = _bisect(lambda L: skin.low(shape(12, s, L), fores), -100.0, 0.0, FLOOR)
    for leg in ("hl", "hr"):
        s["start"][leg] = _planted(B, leg, _carry(lift_at(hind, s), rad(KNEEL)), rad(KNEEL))
    # Rolled (SETTLED): the neck bent and the rider rolled on, each down to where the horse lies; and how high it lies.
    horse = skin.low(shape(SETTLED, s, 0.0), HORSE)
    s["bend"] = _bisect(lambda g: -skin.low(shape(SETTLED, dict(s, bend=g), 0.0), ("neck",)), 0.0, 90.0, -horse)
    s["over"] = _bisect(lambda c: -skin.low(shape(SETTLED, dict(s, over=c), 0.0), ("rider",)), -20.0, 70.0, -horse)
    s["L_end"] = FLOOR - skin.low(shape(SETTLED, s, 0.0), body)
    # Down on its folded legs (22) no lower than the rest of it will need as it rolls over (the rider's leg under it, as
    # it starts over onto that side, can hold it higher than its folded legs do): the hind legs' thighs tucked forward
    # until they hold it there.
    times = [22 + i / KEYS for i in range(1, (SETTLED - 22) * KEYS + 1)]
    s["L22"] = lift_at(22, s)
    need = max(FLOOR - skin.low(shape(t, s, 0.0), core) for t in times)
    if s["L22"] < need:
        s["tuck"] = _bisect(lambda k: lift_at(22, dict(s, tuck=k)), 0.0, rad(GIVE), need)
        s["L22"] = lift_at(22, s)
    # Lying (SETTLED), each leg dropped about the barrel's line toward the ground (-fall: its hoof toward the side it lies
    # on): the pair on that side until it lies on the ground (a hair above FLOOR, so no leg need give way there), then
    # the upper pair until it rests on them, or on the ground, FLOOR above it.
    lift = lift_at(SETTLED, s)

    def dropped(leg, a):
        return shape(SETTLED, dict(s, splay=dict(s["splay"], **{leg: -fall * a})), lift)

    lower = [leg for leg in LEGS if LEGS[leg][1] * spec["side"] > 0.0]
    for leg in lower:
        s["splay"][leg] = -fall * _bisect(lambda a: -skin.low(dropped(leg, a), parts[leg]), 0.0, rad(SPLAY), -FLOOR - 0.01)
    for leg in LEGS:
        if leg not in lower:
            s["splay"][leg] = -fall * _bisect(lambda a: -legs.gap(dropped(leg, a), leg), 0.0, rad(SPLAY), -FLOOR)
    s["floors"] = [(t, floor(t, s)) for t in times]
    s["needs"] = [(t, needs(t, s)) for t in times]
    # What he holds: where he lets go of it, and where it lies, along the ground on its own side (forward, a little
    # out), DROP_GAP clear of all else as horse and rider end up, flat (what stood across it at rest, a pennant or a bow's
    # string, lying down).
    pose = shape(DROP[0], s, lift_at(DROP[0], s))
    s["let go"] = pose["rider_arm"] @ B["rider_arm"].matrix_local.inverted()
    out = math.radians(DROP_OUT)
    lying = _rotation_onto((item["axis"], Vector((0.0, 1.0, 0.0))), (Vector((math.cos(out), -math.sin(out), 0.0)), Vector((0.0, 0.0, 1.0))))
    held = skin({"rider_arm": lying @ B["rider_arm"].matrix_local}, ("rider_arm",))
    others = skin(shape(SETTLED, s, lift_at(SETTLED, s)), body)
    ahead = item["grip"].x + DROP_AHEAD - (lying @ item["grip"]).x
    side = float(others[:, 1].min()) - DROP_GAP - float(held[:, 1].max())
    s["lies"] = Matrix.Translation((ahead, side, FLOOR - float(held[:, 2].min()))) @ lying
    return lambda t: at(t, s)


def _record(arm, take, posed, rest=()):
    """Keys the one-shot `take` (ONCE) from posed(frame) (every bone's matrix), KEYS a frame, the bones in `rest` at rest."""
    n = ONCE[take]["frames"]
    action = bpy.data.actions.new(take)
    _play(arm, action)
    last = {}
    for i in range(n * KEYS + 1):
        _set_keys(arm, i / KEYS, posed(i / KEYS), last, rest)
    finish_once(action, n)
    return action


def once(arm, body, name):
    """Keys the rider's one-shots (ONCE): Strike (strike) and Hit (hit), the horse at rest under them, and the falls
    (_death). Returns the actions by take."""
    B = arm.data.bones
    item = ITEMS[name]
    if not item:
        raise ValueError("he holds nothing (no solid above the seat in the band HELD)")
    still = Matrix.Identity(4)
    out = {}
    out["Strike"] = _record(arm, "Strike", lambda t: _posed(B, still, still, STILL_LEGS, *strike(B, item, t)),
                            [b.name for b in B if b.name not in ("rider", "rider_arm")])
    out["Hit"] = _record(arm, "Hit", lambda t: _posed(B, still, _turn(hit(t)[0]), STILL_LEGS, hit(t)[1], still),
                         [b.name for b in B if b.name not in ("rider", "neck")])
    skin, legs = _Skin(arm, body), _Legs(arm, body)
    for take in ("Death", "DeathBack"):
        out[take] = _record(arm, take, _death(B, skin, legs, item, take))
    return out


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
    """Checks one rigged horseman, its loops (check_loops) and its one-shots (check_once); returns (problems, report
    lines), as check_rigs.py takes them."""
    problems, lines = check_loops(arm, body, name)
    once, line = check_once_safely(arm, body, name)
    return problems + once, lines + [line]


def check_once_safely(arm, body, name):
    """check_once, an error in it (under another Blender, say) reported as its failure, so that it costs the rider only
    its one-shots (as rig_figures.py and check_rigs.py treat the people's)."""
    try:
        return check_once(arm, body, name)
    except Exception as error:
        problem = "its one-shots could not be checked (%s: %s)" % (type(error).__name__, error)
        return [problem], "check %s once (%s): FAIL (%s)" % (name, (ITEMS.get(name) or {}).get("kind", "nothing held"), problem)


def check_loops(arm, body, name):
    """Checks one rigged horseman's loops; returns (problems, a report line for each gait). Fails when a bone (`rider`
    too, but not `rider_arm`) carries no vertices, or in Walk, Trot or Run (sampled SUBFRAMES times a frame): a hoof in
    its planted window (its ankle, the lower leg's tail, and its sole's vertices) moves more than HOLD cm along the
    ground (which runs back at the gait's speed) or off it; a hoof vertex goes more than SINK cm into the ground; the
    last frame is not the first (every bone within 0.01 cm and 0.01 degrees); the left hind hoof does not touch down at
    frame 0; or the barrel moves more than JOLT cm or degrees in a frame. Reports too how far the planted legs stretch
    (at most about REACH)."""
    problems = []
    groups = {g.index: g.name for g in body.vertex_groups}
    owner = [groups[max(v.groups, key=lambda g: g.weight).group] if len(v.groups) else "" for v in body.data.vertices]
    # (`rider_arm` carries nothing on a rider who holds nothing: check_once fails that one, so only his one-shots go.)
    bare = [b.name for b in arm.data.bones if b.name not in ("root", "rider_arm") and b.name not in owner]
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
    for act_name in ("Walk", "Trot", "Run"):
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


# The one-shots' check (check_once): cm, degrees.
STEADY = 0.1      # how far a vertex of the horse may move in a Strike (or a Hit, its neck aside)
ONCE_SINK = 1.0   # how far anything may go into the ground, in any one-shot
LIE = 0.2         # how far apart a fall's still frames may be
LYING = 3.0       # how high a fall's lowest point may end (and no lower than -ONCE_SINK), and each leg's above the ground or the
# leg under it
DOWN = 70.0       # how high the barrel may end, fallen
ROLLED = 60.0     # how far over it must end, onto the take's side
RISE = 3.0        # how far its barrel may rise once the hind legs fold (it only comes down: no heaving up onto a leg)
JUMP = 60.0       # how far anything may move in a frame
AIM = 20.0        # the lance at the blow: within this of level, and of straight ahead in plan
AHEAD = 40.0      # and its point at least this far ahead of the horse's nose
BOW_UPRIGHT, BOW_AHEAD, BOW_RAISED = 25.0, 35.0, 5.0  # the bow at the loose: within this of upright, its grip this far
# ahead of the seat, and raised (or moved) this far since frame 0
INSIDE = 0.3      # how far inside the horse's neck or head, the rider's legs, the saddle or a shield on his arm what he
# holds may go, tested at points ALONG cm apart along its edges (a lance's shaft has vertices only at its ends)
ALONG = 2.0


def _hull(points):
    """The convex hull of points (cm, rows) as its faces' planes: (outward normals, offsets), so p is inside where every
    normals @ p - offsets < 0 (every kit solid is convex: a tube, a ball, a box, a disc)."""
    import bmesh

    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts)
    centre = np.mean(points, axis=0)
    normals, offsets = [], []
    for f in bm.faces:
        if f.calc_area() < 1e-6:
            continue
        n = np.array(f.normal)
        c = np.array(f.calc_center_median())
        if n @ (c - centre) < 0.0:
            n = -n
        normals.append(n)
        offsets.append(float(n @ c))
    bm.free()
    return np.array(normals), np.array(offsets)


def _obstacles(body, owner, rest):
    """What what the rider holds must keep out of in a Strike or a Hit, each a solid as its bone carries it: (name,
    bone, its hull at rest). The horse's neck and head (the solids on `neck`); the rider's legs down the flanks (the
    solids on the barrel within the box they hang in beside the saddle); the saddle (a flat solid on the barrel across
    its back at the seat); and a shield on his arm (a flat plate on `rider` facing across)."""
    out = []
    for members in _islands(body.data):
        bone = owner[members[0]]
        co = rest[members] / CM
        lo, hi = co.min(axis=0), co.max(axis=0)
        span = hi - lo
        centre = co.mean(axis=0)
        if bone == "neck":
            name = "neck"
        elif bone == "barrel" and lo[0] > -0.1 and hi[0] < 0.25 and np.abs(co[:, 1]).min() > 0.04 and hi[2] < 1.65 and lo[2] > 0.7 \
                and np.abs(co[:, 1]).max() < 0.45:
            name = "leg"
        elif bone == "barrel" and span[2] < 0.08 and 1.4 < centre[2] < 1.6 and span[1] > 0.5:
            name = "saddle"
        elif bone == "rider" and span[1] < 0.06 and span[0] > 0.3 and span[2] > 0.3:
            name = "shield"
        else:
            continue
        if len(members) >= 4:
            out.append((name, bone) + _hull(rest[members]))
    return out


def _along(body, indices, rest):
    """Points along every edge of what the rider holds (its vertices `indices`), at most ALONG cm apart, as (a, b, t):
    each point is vertex a + (vertex b - vertex a) * t."""
    keep = set(int(i) for i in indices)
    a, b, t = list(keep), list(keep), [0.0] * len(keep)
    for e in body.data.edges:
        i, j = e.vertices
        if i in keep and j in keep:
            m = int(np.linalg.norm(rest[i] - rest[j]) // ALONG)
            for k in range(1, m + 1):
                a.append(i)
                b.append(j)
                t.append(k / (m + 1.0))
    return np.array(a, dtype=np.int64), np.array(b, dtype=np.int64), np.array(t)[:, None]


def check_once(arm, body, name):
    """Checks a rider's one-shots (ONCE), each over its every key (KEYS a frame), its last frame included; returns
    (problems, report line). Fails when the one-shots could not be keyed (ERRORS) or one is missing; a bone has no keys
    in one; anything goes more than ONCE_SINK into the ground, or anything moves more than JUMP over a frame (from any
    key). In Strike and Hit: the horse moves (a vertex not the rider's, nor in a Hit the neck's, more than STEADY),
    frame 0 is not the rest pose or the last frame not the first (every bone within 0.01 cm and 0.01 degrees), or what
    the rider holds goes more than INSIDE into the horse's neck or head, the rider's legs, the saddle or a shield on his
    arm (_obstacles); at the Strike's blow, a lance more than AIM off level or off straight ahead, or its point less
    than AHEAD ahead of the horse's nose; a bow more than BOW_UPRIGHT off upright, its grip less than BOW_AHEAD ahead of
    the seat, or raised less than BOW_RAISED. In Death and DeathBack: a hoof's sole moves more than HOLD before its leg
    folds (the fores after `fore`, the hinds after `hind`); the barrel rises more than RISE once the hind legs fold; it
    does not lie still from `still` (within LIE); or it ends with its lowest point outside -ONCE_SINK..LYING, a leg's
    lowest point more than LYING above both the ground and the leg straight under it (_Legs.gap: a leg held up off
    them, stiff in the air), the barrel higher than DOWN, or rolled less than ROLLED onto the take's side."""
    item = ITEMS.get(name) or {}
    head = "check %s once (%s): " % (name, item.get("kind", "nothing held"))
    if ERRORS.get(name):
        problem = "its one-shots could not be keyed (%s)" % ERRORS[name]
        return [problem], head + "FAIL (%s)" % problem
    missing = [t for t in ONCE if bpy.data.actions.get(t) is None]
    if missing:
        problem = "no " + ", ".join(missing)
        return [problem], head + "FAIL (%s)" % problem
    groups = {g.index: g.name for g in body.vertex_groups}
    owner = np.array([groups[v.groups[0].group] for v in body.data.vertices])
    rest = np.empty(len(owner) * 3)
    body.data.vertices.foreach_get("co", rest)
    rest = rest.reshape(-1, 3)
    held = item["held"]
    a, b, t_ = _along(body, held, rest)
    obstacles = _obstacles(body, owner, rest)
    neck = owner == "neck"
    legs = _Legs(arm, body)
    soles = {}
    for leg in LEGS:
        idx = np.flatnonzero(owner == leg + "_hoof")
        soles[leg] = idx[rest[idx, 2] < rest[idx, 2].min() + 0.5]
    scene = bpy.context.scene
    B = arm.data.bones
    problems, parts = [], []
    for take, spec in ONCE.items():
        n = spec["frames"]
        act = bpy.data.actions[take]
        found = []
        keyed = {fc.data_path.split('"')[1] for fc in _fcurves(act) if "rotation_quaternion" in fc.data_path}
        loose = [x.name for x in B if x.parent is not None and x.name not in keyed]
        if loose:
            found.append("no keys for " + ", ".join(loose))
        _play(arm, act)
        track, mats = [], []
        clear = (1e9, "")
        for i in range(n * KEYS + 1):
            f = i / KEYS
            scene.frame_set(int(f), subframe=f - int(f))
            dg = bpy.context.evaluated_depsgraph_get()
            P = arm.evaluated_get(dg).pose.bones
            mats.append({x.name: x.matrix.copy() for x in P})
            ev = body.evaluated_get(dg)
            me = ev.to_mesh()
            co = np.empty(len(me.vertices) * 3)
            me.vertices.foreach_get("co", co)
            ev.to_mesh_clear()
            co = co.reshape(-1, 3)
            track.append(co)
            pts = co[a] + (co[b] - co[a]) * t_
            for what, bone, normals, offsets in obstacles:
                D = np.array(mats[-1][bone] @ B[bone].matrix_local.inverted())
                local = (pts - D[:3, 3]) @ D[:3, :3]  # into the bone's rest frame (D is rigid)
                depth = float((-(local @ normals.T - offsets).max(axis=1)).max())
                if -depth < clear[0]:
                    clear = (-depth, what, f)
        track = np.array(track)
        low = float(track[:, :, 2].min())
        if low < -ONCE_SINK:
            found.append("something goes %.1f cm into the ground at frame %g" % (-low, track[:, :, 2].min(axis=1).argmin() / KEYS))
        step = np.linalg.norm(track[KEYS:] - track[:-KEYS], axis=2).max(axis=1)  # over a frame, from every key
        jump = (float(step.max()), int(step.argmax()) / KEYS)
        if jump[0] > JUMP:
            found.append("something moves %.0f cm from frame %g to %g" % (jump[0], jump[1], jump[1] + 1))
        note = "low %.1f jump %.0f" % (low, jump[0])
        if take in ("Strike", "Hit"):
            horse = ~np.isin(owner, ("rider", "rider_arm") + (("neck",) if take == "Hit" else ()))
            steady = float(np.linalg.norm(track[:, horse] - track[0, horse], axis=2).max())
            if steady > STEADY:
                found.append("the horse moves %.2f cm" % steady)
            gap = max((mats[0][x].to_translation() - mats[-1][x].to_translation()).length for x in mats[0])
            turn = max(math.degrees(mats[0][x].to_quaternion().rotation_difference(mats[-1][x].to_quaternion()).angle) for x in mats[0])
            if gap > 0.01 or turn > 0.01:
                found.append("its last frame is not its first (%.3f cm, %.3f degrees)" % (gap, turn))
            gap = max((mats[0][x.name] - x.matrix_local).to_translation().length for x in B)
            turn = max(math.degrees(mats[0][x.name].to_quaternion().rotation_difference(x.matrix_local.to_quaternion()).angle) for x in B)
            if gap > 0.01 or turn > 0.01:
                found.append("frame 0 is not the rest pose (%.3f cm, %.3f degrees)" % (gap, turn))
        if clear[0] < -INSIDE:
            found.append("what the rider holds goes %.1f cm into the %s at frame %g" % (-clear[0], clear[1], clear[2]))
        note += " clear %.1f (%s)" % (clear[0], clear[1])
        if take == "Strike":
            k = spec["impact"] * KEYS
            co = track[k]
            D = mats[k]["rider_arm"] @ B["rider_arm"].matrix_local.inverted()
            axis = D.to_3x3() @ item["axis"]
            if item["kind"] == "lance":
                level = math.degrees(math.asin(max(-1.0, min(1.0, axis.z))))
                plan = math.degrees(math.atan2(abs(axis.y), axis.x))
                ahead = float(co[item["tip"], 0] - co[neck, 0].max())
                if abs(level) > AIM or plan > AIM or ahead < AHEAD:
                    found.append("at the blow the lance is %.0f degrees off level and %.0f off ahead, its point %.0f cm ahead of the nose" % (
                        abs(level), plan, ahead))
                note = "blow: point %.0f cm ahead of the nose, %.0f up, %.0f deg %s, %.0f deg out; " % (
                    ahead, co[item["tip"], 2], abs(level), "down" if level < 0 else "up", plan) + note
            else:
                upright = math.degrees(math.acos(max(-1.0, min(1.0, abs(axis.z)))))
                grip = D @ item["grip"]
                raised = (grip - item["grip"]).length
                if upright > BOW_UPRIGHT or grip.x - SEAT.x * CM < BOW_AHEAD or raised < BOW_RAISED:
                    found.append("at the loose the bow is %.0f degrees off upright, its grip %.0f cm ahead, raised %.0f cm" % (
                        upright, grip.x - SEAT.x * CM, raised))
                note = "loose: bow %.0f deg off upright, grip %.0f cm ahead %.0f up (raised %.0f); " % (
                    upright, grip.x - SEAT.x * CM, grip.z, raised) + note
        elif take not in ("Hit",):
            slide = 0.0
            for leg in LEGS:
                until = (spec["fore"] if leg[0] == "f" else spec["hind"]) * KEYS
                if len(soles[leg]):
                    slide = max(slide, float(np.linalg.norm(track[:until + 1, soles[leg]] - track[0, soles[leg]], axis=2).max()))
            if slide > HOLD:
                found.append("a planted hoof moves %.2f cm before its leg folds" % slide)
            k = spec["still"] * KEYS
            still = float(np.linalg.norm(track[k:] - track[-1], axis=2).max())
            if still > LIE:
                found.append("it does not lie still from frame %d (%.2f cm)" % (spec["still"], still))
            end = float(track[-1, :, 2].min())
            if not -ONCE_SINK <= end <= LYING:
                found.append("its lowest point ends %.1f cm from the ground" % end)
            gaps = {leg: legs.gap(mats[-1], leg) for leg in LEGS}
            leg = max(gaps, key=gaps.get)
            if gaps[leg] > LYING:
                found.append("its %s leg ends %.1f cm above the ground and the leg under it" % (leg, gaps[leg]))
            barrel = mats[-1]["barrel"]
            high = max(barrel.to_translation().z, (barrel @ Vector((0.0, (B["barrel"].tail_local - B["barrel"].head_local).length, 0.0))).z)
            up = barrel.to_3x3() @ (B["barrel"].matrix_local.to_3x3().inverted() @ Vector((0.0, 0.0, 1.0)))
            over = math.degrees(math.atan2(spec["side"] * up.y, up.z))
            if high > DOWN:
                found.append("its barrel ends %.0f cm up" % high)
            if over < ROLLED:
                found.append("it ends rolled %.0f degrees onto its %s side" % (over, "right" if spec["side"] < 0 else "left"))
            # Once the hind legs fold, the barrel (the middle of its bone) only comes down: how far it rises above the
            # lowest it has been since.
            length = (B["barrel"].tail_local - B["barrel"].head_local).length
            mid = [(m["barrel"].to_translation().z + (m["barrel"] @ Vector((0.0, length, 0.0))).z) / 2 for m in mats[spec["hind"] * KEYS:]]
            rises = np.array(mid) - np.minimum.accumulate(mid)
            rise = float(rises.max())
            if rise > RISE:
                found.append("its barrel rises %.1f cm at frame %g, after it went down" % (rise, spec["hind"] + int(rises.argmax()) / KEYS))
            note = "slide %.2f rise %.1f end %.1f legs %.1f barrel %.0f rolled %.0f still %.2f " % (slide, rise, end, gaps[leg], high, over,
                                                                                                    still) + note
        problems += ["%s: %s" % (take, p) for p in found]
        parts.append("%s %d fr %s%s" % (take, n, note, "" if not found else " FAIL"))
    return problems, head + ("ok" if not problems else "FAIL (" + "; ".join(problems) + ")") + " | " + " | ".join(parts)


def once_line_of():
    """The line rig_riders prints after it exports (tools/art/README.md)."""
    parts = []
    for take, spec in ONCE.items():
        if "impact" in spec:
            parts.append("%s %d fr (blow at %d, %.3f s)" % (take, spec["frames"], spec["impact"], spec["impact"] / float(FPS)))
        else:
            parts.append("%s %d fr" % (take, spec["frames"]))
    return "rig_riders: one-shot actions " + ", ".join(parts)


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    build = os.path.abspath(args[0] if args else os.path.join("art", "build"))
    wanted = args[1:] or list(RIDERS)
    out = os.path.join(build, "Rigged")
    os.makedirs(out, exist_ok=True)
    failed, without = [], []
    for name in wanted:
        arm, body = rig(name, RIDERS[name])
        problems, lines = check_loops(arm, body, name)
        once, once_line = check_once_safely(arm, body, name)
        for line in lines + [once_line]:
            print(line)
        path = os.path.join(out, "SK_%s.fbx" % name)
        if problems:
            # Not exported, and an older export goes too, so ue_import_rigged.py cannot bring back a stale rig.
            failed.append(name)
            if os.path.exists(path):
                os.remove(path)
            continue
        if once:
            # Its loops are sound: exported without the one-shots (the game falls back to no strike or flinch and the
            # old roll where a take is missing) rather than taking a horseman off the field for a pose.
            without.append(name)
            _play(arm, bpy.data.actions["Idle"])
            for take in ONCE:
                if bpy.data.actions.get(take):
                    bpy.data.actions.remove(bpy.data.actions[take])
            print("rig_riders: %s exported without its one-shot actions: %s" % (name, "; ".join(once)))
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
    # The one-shots' frames and blow (the same for every rider), for the game (SovGait.h's kSovStrikeImpact).
    print(once_line_of())
    if without:
        print("rig_riders: exported without one-shot actions (failed check_once): " + ", ".join(without))
    if failed:
        print("rig_riders: not exported (any older SK_<Name>.fbx removed), failed the check: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
