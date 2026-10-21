"""One-shot actions for the rigged people (rig_figures.py) in live battles: a Strike keyed for each weapon class, a Hit,
and two falls, Death (forward, onto the face) and DeathBack (onto the back). They are made here rather than from motion
capture: CMU has no spear thrust, shield work or hit reaction (its two gun clips, 79_96 and 80_03 "shooting a gun",
are not fitted), its bow takes 2.6 s to draw, and a blow must land on a known frame. Each is keyed every frame,
straight from one to the next, through armature-space poses as mocap.py keys its clips: the hips, chest and head by
angles, each arm by two-bone IK to a wrist target (its elbow toward a pole), each leg likewise to an ankle target, and
what a hand holds aimed by its hand bone (Poser._hand), so each figure's own spear, shield, bow or musket stays on line
whatever its length, and slid along its line through the fist (a spear held nearer its tip to land in a foe at the
battle's spacing, REACH).

What the game will rely on (tools/art/README.md, One-shot actions; SovGait.h's kSovStrikeImpact, which PR B of Plan G
step 5 adds):
- Strike, 24 frames at 24 fps: the blow lands, or the shot goes off, at frame 10 (0.417 s). Only the chest, head, arms
  and hands move; the hips, legs, skirt and root are keyed at rest (the game plays it over the upper body, `spine` and
  the bones that hang from it, the head, arms and hands, so the legs keep walking), and its last frame is its first,
  the rest pose.
- Hit, 8 frames: a flinch, its frames 0 and 8 the rest pose (the game adds its change from frame 0).
- Death and DeathBack, 36 frames: a fall forward onto the face and one back onto the back, the feet planted until frame
  14 (buckle), landing at 24 (Death) or 26 (DeathBack, which sits down first), lying still on the ground from frame 28
  (the game holds the last frame), a robe going into the ground with the legs once the knees give (drape: the ground
  hides it).

The rig's "_r" side (+X) holds the spear, club, musket or sceptre and its "_l" side the shield or bow (the kits' right
and left hands; the figure's anatomical left and right, as it faces -Y). Nothing is mirrored.
"""
import math

import bpy
import numpy as np
from mathutils import Euler, Matrix, Vector

import mocap

# take: frames (the action's range, its last frame included), the frame its blow lands (a Strike), the frame until
# which its feet stay planted, the frame he lands on his seat (DeathBack), the frame it lands on the ground (a death),
# and the frame from which it lies still.
ONCE = {"Strike": dict(frames=24, impact=10), "Hit": dict(frames=8), "Death": dict(frames=36, buckle=14, down=24, still=28),
        "DeathBack": dict(frames=36, buckle=14, seat=21, down=26, still=28)}
FPS = 24
# Figures whose one-shots rig_figures could not make (item_axes or strike_class in its rig, or add in its animate,
# raised an error, under another Blender say): {name: why}, for check_rigs.check_once to report, so the figure is
# exported without them.
ERRORS = {}
# Who fights: everyone else (the townsfolk, Citizen, Herald, Captain) never does, and gets no one-shots.
FIGHTERS = ("Soldier", "Archer", "Musketeer", "Rifleman", "Leader")
SPEAR = 120.0  # cm: a +X held item at least this long thrusts as a spear (a staff or sceptre gripped at its middle too)
END = 5.0      # cm: how much of each end of a held item tells which end is higher at rest
UPPER = ("spine", "head", "upperarm_r", "upperarm_l", "forearm_r", "forearm_l", "hand_r", "hand_l")  # what a Strike or Hit poses
# The bones whose place is keyed as well as their turn: the hips, and the hands, along whose line what they hold slides
# (a spear held nearer its tip to thrust, Poser.pose's "slide_<s>").
PLACED = ("pelvis", "hand_r", "hand_l")
UNSURE = 150.0  # degrees: a roll of an arm's or leg's bend between two keys past which it may go the other way round
GUARD_FOOT = 0.3  # cm: how far a foot may go into the ground before Poser.pose lifts its ankle, in a fall
HEEL = 30.0    # cm: the most it lifts it (a man kneeling up on his toes)
# frames: from a fall's landing (ONCE's "down") to the end of its bounce, from which he is laid on the ground (floor),
# not only kept out of it
BOUNCE = 2
FLOOR = 0.5    # cm: where his lowest point then lies
EASE = 2.0     # cm per frame squared: how fast the floor pass eases a lift in and out
ARMS_OUT = 50.0  # degrees: a fallen man's arms out from his sides, on the ground beside him rather than under him
ROLL = 30.0    # degrees: a man fallen on his back, rolled onto one side (DeathBack)
# cm: a man falling back lands on his seat with his ankles this far ahead of where he stood, his legs out in front and
# his knees still bent (about 28 cm off the ground), and straightens them as he topples back. With them nearly
# straight there (58), each knee dropped 26 cm in the frame he landed, as a leg's last few cm of reach straighten it
# all at once. (A robe goes with the thigh most ahead, rig_figures.key_skirt, and so stood up before his face like a
# sail with his knees up; drape now holds it down, ROBE_UP.)
SIT = 45.0
# Lying on his back, each arm lies limp on the ground beside him (_limp): out from the shoulder LIMP degrees from
# straight out toward his feet, the upper arm dropped to the ground as far as it reaches and the elbow's joint ELBOW_LIES
# cm above it. (Held out to hands on the ground 52 cm out, the arm on the side a roll had raised propped him up from
# its shoulder 28 to 41 cm up, and the other arched its elbow 9 cm above its shoulder.)
LIMP, ELBOW_LIES = 40.0, 7.0
# cm: the most his hips rise from his seat to lying on his back as his body lies (7 to 13: the hip joint lies higher
# on the back than on the seat). More, and what he carries on his back props him up (the Rifleman's pack, 21 cm): he
# lands on it rather than sitting first and then rising onto it.
PROPPED = 15.0
REBOUND = 4.0  # degrees: his chest bounces up off the ground as his back lands (DeathBack), his hips staying down
# The share of the hips' drop to where he lands (ONCE's "seat") that they have sunk by the end of the stagger: an "on"
# over the 8 frames from the recoil then arrives at the pace an "on" over the 7 frames on from there starts at
# (1.3 * SAG / 8 = 0.7 * (1 - SAG) / 7), so he falls in one quickening move however high he lands. A sword or a quiver
# hanging low at the hip lands first and props him up (by Leader's 50 cm: he cannot sit on the ground over a rigid
# scabbard), and with the stagger sunk as far as for a seat on the ground, his hips slowed from 5.4 to 1.9 cm a frame
# in mid-air.
SAG = 0.38


def strike_class(name, items):
    """How a figure strikes: "gun" (Musketeer*, Rifleman), "bow" (Archer*), "spear" (a +X held item at least SPEAR
    long), "club" (a shorter one), "fist" (nothing in the +X hand), or None for one who never fights. `items`: item_axes
    per side."""
    if not name.startswith(FIGHTERS):
        return None
    if name.startswith(("Musketeer", "Rifleman")):
        return "gun"
    if name.startswith("Archer"):
        return "bow"
    if not items.get("r"):
        return "fist"
    return "spear" if items["r"]["length"] >= SPEAR else "club"


def item_axes(co, indices, wrist):
    """What a hand holds (its vertices `indices` of the rest coordinates `co`, cm) as it is at rest: its centre and
    length, its long axis by principal components, toward its end that is higher at rest (its business end: a spear's
    tip, a club's blade, a muzzle, a bow's top), its thinnest axis (a shield's face) away from the body, and the
    vertices at its tip (and where that is), its butt and nearest the hand (`wrist`, a bow's grip), and how wide and
    deep it is across its length (along its middle and thinnest axes)."""
    pts = np.array([co[i] for i in indices])
    centre = pts.mean(axis=0)
    _, _, vt = np.linalg.svd(pts - centre, full_matrices=False)
    along = (pts - centre) @ vt[0]
    if pts[along < along.min() + END][:, 2].mean() > pts[along > along.max() - END][:, 2].mean():
        vt[0], along = -vt[0], -along
    if vt[2] @ np.array([centre[0], centre[1], 0.0]) < 0.0:
        vt[2] = -vt[2]
    grip = int(np.argmin(np.linalg.norm(pts - np.array(wrist), axis=1)))
    return {"centre": Vector(centre), "length": float(along.max() - along.min()), "long": Vector(vt[0]), "mid": Vector(vt[1]),
            "thin": Vector(vt[2]), "up": Vector((0, 0, 1)), "tip": indices[int(np.argmax(along))], "butt": indices[int(np.argmin(along))],
            "tip_at": Vector(pts[int(np.argmax(along))]), "grip": indices[grip], "width": float(np.ptp((pts - centre) @ vt[1])),
            "depth": float(np.ptp((pts - centre) @ vt[2])),
            "behind": float((Vector(wrist) - Vector(centre)).dot(Vector(vt[0])) - along.min())}


def _turn(pitch=0.0, turn=0.0, tilt=0.0):
    """A rotation (armature space) leaning forward (toward -Y) by `pitch`, turning the +X side forward by `turn` and
    tilting the top toward +X by `tilt`, in degrees."""
    return Matrix.Rotation(math.radians(-turn), 3, "Z") @ Matrix.Rotation(math.radians(pitch), 3, "X") @ Matrix.Rotation(math.radians(tilt), 3, "Y")


def _ease(kind, t):
    """How far along a stretch between two keys a pose is at t (0..1): "inout" eased at both ends, "in" starting slowly
    and arriving fast (a blow), "on" the same for a fall already under way (evenly quickening from 0.7 of its mean pace
    to 1.3 times it: after another "on" stretch about as long that covered about half as much, it goes on at the pace
    that one arrived at), "off" the other way round (a jolt that has not stopped when the next stretch takes over),
    "out" starting fast and settling, "linear", and "inout1" and "inout2", the first and second halves of "inout" (two
    stretches eased as one move through the key between them, at most 1.5 times their mean pace where "in" then "out"
    reach twice it). A key eased into by "inout", "out" or "inout2" is a stop: everything stands still there for a
    moment, which reads as a hitch in a fall."""
    if kind == "in":
        return t * t
    if kind == "on":
        return t * (0.7 + 0.3 * t)
    if kind == "off":
        return t * (1.3 - 0.3 * t)
    if kind == "out":
        return 1.0 - (1.0 - t) ** 2
    if kind == "inout1":
        return t * t * (3.0 - t) / 2.0
    if kind == "inout2":
        return 1.0 - (1.0 - t) ** 2 * (2.0 + t) / 2.0
    if kind == "linear":
        return t
    return t * t * (3.0 - 2.0 * t)


class Poser:
    """Poses one rigged figure (armature `arm`, centimetres) by armature-space matrices and keys them, as mocap.Retarget
    does. `items`: item_axes per side ("r", "l"), or None for an empty hand."""

    def __init__(self, arm, items):
        self.arm = arm
        B = arm.data.bones
        self.rest = {b.name: b.matrix_local.copy() for b in B}
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in B}
        self.order = [b.name for b in B]  # parents before children, as the armature was built
        self.items = items
        self.at = {b.name: (b.head_local.copy(), b.tail_local.copy()) for b in B}
        # Each arm and leg as a chain of two bones: its lengths, and its bend at rest (the pole toward its elbow or knee
        # and the bend's axis), so a pose that asks for the rest wrist or ankle gets the rest bones back exactly.
        self.chains = {}
        for s in ("r", "l"):
            for limb, top, low in (("arm", "upperarm_", "forearm_"), ("leg", "thigh_", "shin_")):
                a, j = self.at[top + s][0], self.at[low + s][0]
                e = self.at[low + s][1]
                u = (e - a).normalized()
                pole = (j - a) - u * (j - a).dot(u)
                pole = pole.normalized() if pole.length > 1e-6 else Vector((0, -1, 0))
                lat = pole.cross(u)
                self.chains[limb + s] = dict(top=top + s, low=low + s, l1=(j - a).length, l2=(e - j).length, pole=pole,
                                             frames=(mocap._frame(j - a, lat), mocap._frame(e - j, lat)), lat=lat)
            foot = self.at["foot_" + s]
            self.chains["leg" + s]["foot"] = mocap._frame(foot[1] - foot[0], self.chains["leg" + s]["lat"])
        self.rest_spec = {}
        for s in ("r", "l"):
            self.rest_spec["wrist_" + s] = ("chest", tuple(self.at["forearm_" + s][1]))
            self.rest_spec["elbow_" + s] = ("chest", tuple(self.chains["arm" + s]["pole"]))
            self.rest_spec["ankle_" + s] = ("world", tuple(self.at["shin_" + s][1]))
            self.rest_spec["knee_" + s] = ("pelvis", tuple(self.chains["leg" + s]["pole"]))
            self.rest_spec["toe_" + s] = ("world", tuple((self.at["foot_" + s][1] - self.at["foot_" + s][0]).normalized()))

    def swung(self, s, out=0.0, back=0.0, bend=0.0):
        """An arm's rest pose turned about its shoulder, out from the body by `out` and back by `back` degrees, its
        elbow bent a further `bend` (the wrist drawn up toward the shoulder): its wrist and elbow, in the chest's rest
        frame."""
        sign = 1.0 if s == "r" else -1.0
        R = Matrix.Rotation(math.radians(-sign * out), 3, "Y") @ Matrix.Rotation(math.radians(back), 3, "X")
        shoulder = self.at["upperarm_" + s][0]
        elbow = self.at["forearm_" + s][0]
        wrist = self.at["forearm_" + s][1]
        if bend:
            wrist = elbow + Matrix.Rotation(math.radians(-bend), 3, "X") @ (wrist - elbow)
        return {"wrist_" + s: ("chest", tuple(shoulder + R @ (wrist - shoulder))), "elbow_" + s: ("chest", tuple(R @ self.chains["arm" + s]["pole"]))}

    # Posing --------------------------------------------------------------------------------------------------------

    def _limb(self, chain, root, target, pole):
        """Two-bone IK for one chain from `root` toward `target` (armature space), bending toward `pole`: the two bones'
        armature-space matrices and the bend's axis."""
        c = self.chains[chain]
        joint, end, lat = mocap._two_bone(root, target, pole, c["l1"], c["l2"])
        top = mocap._frame(joint - root, lat) @ c["frames"][0].transposed() @ self.rest[c["top"]].to_3x3()
        low = mocap._frame(end - joint, lat) @ c["frames"][1].transposed() @ self.rest[c["low"]].to_3x3()
        return Matrix.Translation(root) @ top.to_4x4(), Matrix.Translation(joint) @ low.to_4x4(), lat

    def _bent(self, chain, keys, w, line):
        """The way a chain bends a share `w` of the way between two key poses (`keys`, Poser.pose's matrices), its line
        from root to target now `line`. Each key's bend is the direction from its line to its elbow or knee, read off
        the bend's axis (which holds even for a nearly straight arm). The first key's bend is carried onto `line` by
        the least turn from its own line, then rolled about `line` by the share `w` of the roll that, so carried onto
        the second key's line, takes it to that key's bend: the short way round, or, where that is near half a turn
        (UNSURE) and so could go either way, the way that takes an elbow round outside the arm or a knee round in front
        of the leg rather than across the body."""
        c = self.chains[chain]
        ends = []
        for M in keys:
            top = M[c["top"]].to_3x3() @ self.rest[c["top"]].to_3x3().transposed() @ c["frames"][0]
            end = M[c["low"]] @ (self.rest[c["low"]].inverted() @ self.at[c["low"]][1])
            uk = (end - M[c["top"]].translation).normalized()
            ends.append((uk, uk.cross(Vector(top.col[0])).normalized()))
        (ua, pa), (ub, pb) = ends
        carried = ua.rotation_difference(ub) @ pa
        roll = carried.angle(pb, 0.0)
        if carried.cross(pb).dot(ub) < 0.0:
            roll = -roll
        if abs(roll) > math.radians(UNSURE):
            side = Vector((1.0, 0.0, 0.0) if chain == "armr" else (-1.0, 0.0, 0.0) if chain == "arml" else (0.0, -1.0, 0.0))
            mid = (ua + ub).normalized() if (ua + ub).length > 1e-6 else ua
            start = ua.rotation_difference(mid) @ pa
            other = roll - math.copysign(2.0 * math.pi, roll)
            if (Matrix.Rotation(0.5 * other, 3, mid) @ start).dot(side) > (Matrix.Rotation(0.5 * roll, 3, mid) @ start).dot(side):
                roll = other
        u = line.normalized()
        return Matrix.Rotation(roll * w, 3, u) @ (ua.rotation_difference(u) @ pa)

    def _toe(self, s, specs, keys, w):
        """A foot's direction (ankle to toe) against the shin, in the shin's rest frame, a share `w` of the way between
        two keys' `specs` of which one turns it with the shin and the other holds it in the world: each as it lies
        against that key's own shin, the ankle turned between them (a foot flat on the ground turned down to point
        along a shin lying flat, not over the top in a frame as their mix in the world would)."""
        local = []
        for spec, M in zip(specs, keys):
            v = Vector(spec[1]).normalized()
            if spec[0] != "shin":
                shin = M["shin_" + s].to_3x3() @ self.rest["shin_" + s].to_3x3().transposed()
                v = shin.transposed() @ v
            local.append(v)
        return local[0].slerp(local[1], w, local[0] if w < 0.5 else local[1])

    def _hand(self, s, spec, M, frames):
        """hand_<s>'s turn from rest (a quaternion) for an item spec: None keeps the hand as it is on the forearm;
        "rest" holds the item as it stands at rest, whatever the forearm does (so a spear going from upright to level
        turns a quarter turn, not the half turn from where a raised forearm would carry it); else (axis, frame, want[,
        ref_axis, ref_frame, ref_want]) aims the item's rest `axis` (item_axes: "long", "thin"...) along `want` in
        `frame`, turned about it so its `ref_axis` lies as near `ref_want` as it can, or by the least turn from where
        the forearm carries it (or, given a fourth "rest", from where it stands at rest: a gun levelled from upright
        turns only forward; a fifth then rolls it that many degrees about `want`, its own line, which turns it in
        place)."""
        fore = M["forearm_" + s].to_3x3() @ self.rest["forearm_" + s].to_3x3().transposed()
        item = self.items.get(s)
        if spec is None or not item:
            return fore.to_quaternion()
        if spec == "rest":
            return Matrix.Identity(3).to_quaternion()
        axis = item[spec[0]]
        want = (frames[spec[1]].to_3x3() @ Vector(spec[2])).normalized()
        if len(spec) >= 4 and spec[3] == "rest":
            R = axis.rotation_difference(want).to_matrix()
            if len(spec) == 5:
                R = Matrix.Rotation(math.radians(spec[4]), 3, want) @ R
        elif len(spec) > 3:
            ref = (frames[spec[4]].to_3x3() @ Vector(spec[5])).normalized()
            R = mocap._frame(want, ref) @ mocap._frame(axis, item[spec[3]]).transposed()
        else:
            R = (fore @ axis).rotation_difference(want).to_matrix() @ fore
        return R.to_quaternion()

    def pose(self, a, b, w, up=0.0, bends=True, guard=False):
        """Every bone's armature-space matrix (cm) a share `w` of the way from key pose `a` to key pose `b` (dicts of
        the parameters below; one left out is at rest):
        - "pelvis": (x, y, z, pitch, turn, tilt[, roll]): the hips moved (cm) and turned (_turn) about their joint,
          first rolled about their own length by `roll` degrees, the +X side back (down, for a man on his back);
        - "spine", "head": (pitch, turn, tilt), each from the bone it hangs from;
        - "wrist_<s>", "elbow_<s>", "ankle_<s>", "knee_<s>": (frame, point or direction) for the IK, the frame "world",
          "pelvis" or "chest" (a point or direction in the rest pose carried with the hips or the chest); a wrist swings
          round its shoulder from the key before, or comes in a straight line given a third "straight" (a thrust);
        - "toe_<s>": (frame, direction) of the foot from ankle to toe, "world" or "shin" (with the shin);
        - "item_<s>": what the hand holds, aimed (_hand);
        - "slide_<s>": (cm,): what the hand holds slid back through the fist along its own line (its hand bone moved
          along it from the wrist), so a spear is held nearer its tip.
        `up`: cm the hips and the ankles' world targets go up besides (key's lift). Between two keys (`bends`) each
        elbow and knee bends the way each key bends it, carried round onto this frame's line from the shoulder to the
        wrist (or the hip to the ankle) by the least turn, and turned a share `w` of the way from one to the other
        about that line (_bent). Mixing the two keys' poles as given would swing an elbow round in a frame as the line
        passed the mix (a wrist drawn up past its shoulder), and bending toward a mix of where the two keys put the
        elbow rolled a nearly straight arm half over in a frame, that point lying almost on the line. Likewise a foot
        the one key turns with the shin and the other holds in the world turns in the shin's own frame (_toe), not
        through a mix in the world. `guard` (a fall, measured on self.skin): a foot that would go into the ground has
        its ankle lifted (up to HEEL: a foot turning down to point back as the shin goes flat pivots on its toes, the
        heel up), and a wrist is raised until neither the forearm nor what the hand holds goes below FLOOR, each by the
        least that does it, frame by frame (two keys out of the ground may still put a hand or a toe into it between
        them). Turning the foot up instead turned it over the top, or back, in a frame."""
        rest = self.rest
        keys = None
        if bends and a is not b and 0.0 < w < 1.0:
            keys = (self.pose(a, a, 0.0, up, False), self.pose(b, b, 0.0, up, False))

        def mix(key, size=3):
            va, vb = ((tuple(p.get(key, ())) + (0.0,) * size)[:size] for p in (a, b))  # (a value left out is 0)
            return [x + (y - x) * w for x, y in zip(va, vb)]

        hips = mix("pelvis", 7)
        D = _turn(*hips[3:6]) @ Matrix.Rotation(math.radians(hips[6]), 3, "Z")
        Ds = D @ _turn(*mix("spine"))
        Dh = Ds @ _turn(*mix("head"))
        M = {"root": rest["root"].copy()}
        M["pelvis"] = Matrix.Translation(self.at["pelvis"][0] + Vector(hips[:3]) + Vector((0.0, 0.0, up))) @ (D @ rest["pelvis"].to_3x3()).to_4x4()
        P = M["pelvis"] @ rest["pelvis"].inverted()
        M["spine"] = Matrix.Translation(P @ self.at["spine"][0]) @ (Ds @ rest["spine"].to_3x3()).to_4x4()
        C = M["spine"] @ rest["spine"].inverted()
        M["head"] = Matrix.Translation(C @ self.at["head"][0]) @ (Dh @ rest["head"].to_3x3()).to_4x4()
        frames = {"world": Matrix.Identity(4), "pelvis": P, "chest": C}
        lifted = Matrix.Translation((0.0, 0.0, up))  # (the ankles' world targets go up with the hips; the wrists' stay)

        def point(key, about=None):
            pa, pb = a.get(key, self.rest_spec[key]), b.get(key, self.rest_spec[key])
            fa, fb = (lifted if f == "world" and key.startswith("ankle") else frames[f] for f in (pa[0], pb[0]))
            ta, tb = fa @ Vector(pa[1]), fb @ Vector(pb[1])
            if about is None or pb[2:] == ("straight",):
                return ta.lerp(tb, w)
            # A wrist swings round its shoulder (`about`), its reach mixed apart from its way, so a hand drawn up from
            # the hip to beside the ear goes out round the shoulder, not through it.
            va, vb = ta - about, tb - about
            return about + va.normalized().slerp(vb.normalized(), w, va.normalized()) * (va.length + (vb.length - va.length) * w)

        def way(key, extra=None):
            out = []
            for spec in (a.get(key, self.rest_spec[key]), b.get(key, self.rest_spec[key])):
                frame = extra if spec[0] == "shin" else frames[spec[0]]
                out.append((frame.to_3x3() @ Vector(spec[1])).normalized())
            v = out[0].lerp(out[1], w)
            return v.normalized() if v.length > 1e-6 else out[1]

        def toe_way(s, shin):
            key = "toe_" + s
            specs = [p.get(key, self.rest_spec[key]) for p in (a, b)]
            if keys is None or (specs[0][0] == "shin") == (specs[1][0] == "shin"):
                return way(key, shin.to_4x4())
            return shin @ self._toe(s, specs, keys, w)

        for s in ("r", "l"):
            hip = P @ self.at["thigh_" + s][0]
            ankle = point("ankle_" + s)
            knee = self._bent("leg" + s, keys, w, ankle - hip) if keys else way("knee_" + s)

            def stand(dz, s=s, hip=hip, ankle=ankle, knee=knee):
                M["thigh_" + s], M["shin_" + s], lat = self._limb("leg" + s, hip, ankle + Vector((0.0, 0.0, dz)), knee)
                shin = M["shin_" + s].to_3x3() @ rest["shin_" + s].to_3x3().transposed()
                foot = mocap._frame(toe_way(s, shin), lat) @ self.chains["leg" + s]["foot"].transposed() @ rest["foot_" + s].to_3x3()
                M["foot_" + s] = Matrix.Translation(M["shin_" + s] @ (rest["shin_" + s].inverted() @ self.at["foot_" + s][0])) @ foot.to_4x4()
                return self.skin.low(M, "foot_" + s) if guard else 0.0
            if stand(0.0) < -GUARD_FOOT:
                stand(_bisect(stand, 0.0, HEEL, -GUARD_FOOT))
            for skirt in ("skirt_f", "skirt_b"):  # (keyed from the legs by rig_figures.key_skirt)
                M[skirt] = P @ rest[skirt]
        for s in ("r", "l"):
            shoulder = C @ self.at["upperarm_" + s][0]
            target = point("wrist_" + s, shoulder)
            elbow = self._bent("arm" + s, keys, w, target - shoulder) if keys else way("elbow_" + s)
            slide = mix("slide_" + s, 1)[0] if self.items.get(s) else 0.0

            def reach(dz, s=s, shoulder=shoulder, elbow=elbow, target=target, slide=slide):
                M["upperarm_" + s], M["forearm_" + s], _ = self._limb("arm" + s, shoulder, target + Vector((0.0, 0.0, dz)), elbow)
                qa = self._hand(s, a.get("item_" + s), M, frames)
                qb = self._hand(s, b.get("item_" + s), M, frames)
                if qa.dot(qb) < 0.0:
                    qb.negate()
                turn = qa.slerp(qb, w).to_matrix()
                wrist = M["forearm_" + s] @ (rest["forearm_" + s].inverted() @ self.at["hand_" + s][0])
                if slide:
                    wrist = wrist - (turn @ self.items[s]["long"]) * slide
                M["hand_" + s] = Matrix.Translation(wrist) @ (turn @ rest["hand_" + s].to_3x3()).to_4x4()
                return min(self.skin.low(M, "fore_" + s), self.skin.low(M, "item_" + s)) if guard else FLOOR
            if reach(0.0) < FLOOR:
                reach(_bisect(reach, 0.0, 100.0, FLOOR))
        return M

    def key(self, name, n, keys, bones=None, lift=None, guard=False):
        """Keys action `name` over frames 0..n from `keys`: [(frame, ease, pose)], the first at frame 0 and the last at
        n, each pose eased into from the one before (_ease). `bones`: only those (the rest by key_rest), else all but
        the root and the skirt's. `lift`: per frame, cm the hips (and the ankles' world targets) go up besides (floor),
        the wrists' world targets staying put. `guard`: pose's, for a fall. Returns the action."""
        action = bpy.data.actions.new(name)
        arm = self.arm
        arm.animation_data.action = action
        P = arm.pose.bones
        for b in P:
            b.rotation_mode = "XYZ"
        keep = [b for b in self.order if (b in bones if bones else b not in ("root", "skirt_f", "skirt_b"))]
        prev = {}
        for f in range(n + 1):
            k = max(i for i, key in enumerate(keys) if key[0] <= f)
            if k == len(keys) - 1:
                a = b = keys[k][2]
                w = 0.0
            else:
                (fa, _, a), (fb, ease, b) = keys[k], keys[k + 1]
                w = _ease(ease, (f - fa) / float(fb - fa))
            M = self.pose(a, b, w, lift[f] if lift else 0.0, guard=guard)
            for bone in keep:
                par = self.parent[bone]
                basis = self.rest[bone].inverted() @ self.rest[par] @ M[par].inverted() @ M[bone]
                loc, rot, _ = basis.decompose()
                e = rot.to_euler("XYZ", prev[bone]) if bone in prev else rot.to_euler("XYZ")
                prev[bone] = e
                P[bone].rotation_euler = e
                P[bone].keyframe_insert("rotation_euler", frame=f)
                if bone in PLACED:
                    P[bone].location = loc
                    P[bone].keyframe_insert("location", frame=f)
        return action


class Skin:
    """A figure's vertices as Blender's armature deforms them (each vertex's bones mixed by its weights) for bone
    matrices from Poser.pose, so a pose can be measured before it is keyed (settle). Its parts, cloth left out (the
    skirt bones are keyed afterwards: key_skirt, drape): "torso" (on the hips or the chest, a quiver or a pack too),
    "head", "leg_<s>" (thigh and shin), "foot_<s>", "upper_<s>" (the upper arm), "fore_<s>" (the forearm and hand),
    "arm_<s>" (the arm, its hand and what the hand holds) and "item_<s>"; and "neck", the ends of the edges between
    the head and the body (Skin.neck)."""

    def __init__(self, arm, body, held):
        B = arm.data.bones
        self.names = [b.name for b in B]
        col = {name: k for k, name in enumerate(self.names)}
        groups = {g.index: g.name for g in body.vertex_groups}
        n = len(body.data.vertices)
        W = np.zeros((n, len(self.names)))
        for v in body.data.vertices:
            for g in v.groups:
                if g.weight > 0.0 and groups[g.group] in col:
                    W[v.index, col[groups[g.group]]] = g.weight
        W /= np.maximum(W.sum(axis=1, keepdims=True), 1e-9)
        co = np.empty(n * 3)
        body.data.vertices.foreach_get("co", co)
        self.co = np.hstack([co.reshape(-1, 3), np.ones((n, 1))])
        self.inv = {name: np.array(B[name].matrix_local.inverted()) for name in self.names}
        owner = [self.names[k] for k in W.argmax(axis=1)]
        cloth = W[:, col["skirt_f"]] + W[:, col["skirt_b"]] > 0.0
        carried = {s: set(held.get(s) or ()) for s in ("r", "l")}
        loose = [i for i in range(n) if not cloth[i] and i not in carried["r"] and i not in carried["l"]]

        def part(*bones):
            return [i for i in loose if owner[i] in bones]
        parts = {"torso": part("pelvis", "spine"), "head": part("head")}
        for s in ("r", "l"):
            parts["leg_" + s] = part("thigh_" + s, "shin_" + s)
            parts["foot_" + s] = part("foot_" + s)
            parts["upper_" + s] = part("upperarm_" + s)
            parts["fore_" + s] = part("forearm_" + s, "hand_" + s)
            parts["arm_" + s] = sorted(set(part("upperarm_" + s, "forearm_" + s, "hand_" + s)) | carried[s])
            parts["item_" + s] = sorted(carried[s])
        # The edges between the head and the body (its hips and chest), cloth aside, as check_rigs measures its seams:
        # the neck's own, and those of a quiver or a toga's shoulder worn across it, bound partly to the head (as the
        # loops need), which open far wider as the head turns.
        neck = np.array([e.vertices[:] for e in body.data.edges if not cloth[e.vertices[0]] and not cloth[e.vertices[1]]
                         and {owner[e.vertices[0]], owner[e.vertices[1]]} in ({"head", "pelvis"}, {"head", "spine"})], dtype=np.int64).reshape(-1, 2)
        length = np.linalg.norm(self.co[neck[:, 0], :3] - self.co[neck[:, 1], :3], axis=1)
        neck = neck[length > 0.2]
        self.neck_len = length[length > 0.2]
        parts["neck"], ends = np.unique(neck, return_inverse=True)
        self.neck_ends = ends.reshape(-1, 2)
        # Each part's vertices, their weights and the bones that weigh on them.
        self.parts = {}
        for name, idx in parts.items():
            idx = np.array(idx, dtype=np.int64)
            w = W[idx]
            used = [k for k in range(len(self.names)) if len(idx) and w[:, k].any()]
            self.parts[name] = (idx, [(self.names[k], w[:, k]) for k in used])

    def low(self, M, part):
        """The lowest point (cm) of a part as the bone matrices M (armature space) pose it (1e9 for an empty part)."""
        idx, bones = self.parts[part]
        if not len(idx):
            return 1e9
        z = np.zeros(len(idx))
        for name, w in bones:
            z += w * (self.co[idx] @ (np.array(M[name]) @ self.inv[name])[2])
        return float(z.min())

    def neck(self, M):
        """How far (cm) the edges between the head and the body open beyond their length at rest, the most, as the bone
        matrices M pose them (check_rigs' "seam", for these edges)."""
        idx, bones = self.parts["neck"]
        if not len(idx):
            return 0.0
        at = np.zeros((len(idx), 3))
        for name, w in bones:
            at += w[:, None] * (self.co[idx] @ (np.array(M[name]) @ self.inv[name])[:3].T)
        a, b = self.neck_ends[:, 0], self.neck_ends[:, 1]
        return float((np.linalg.norm(at[a] - at[b], axis=1) - self.neck_len).max())


# The takes ---------------------------------------------------------------------------------------------------------

# A spear's line at the blow: forward, its tip about 7 degrees down and 3 in, toward a foe before his chest (the
# hand is out at his side), which also swings the back half of a sceptre gripped at its middle (FRANCE's) out off
# his upper arm: aimed straight ahead it went 3 cm into it.
LEVEL = ("world", (-0.06, -1.0, -0.12))
# cm: the battle's spacing (battle/include/sovereign_battle/sim.h, kReach): a man strikes a foe standing up to this
# far off, his hips to the foe's. A spear's tip lands TIP ahead of the hips at the blow, in that foe between his chest
# (about 128 ahead) and his back: held where it stands at rest, a 2.5 m spear's tip went 80 to 120 cm out through his
# back. A club or a fist leans into the blow and reaches as far as an arm does (REACH_ARM of it): the macuahuitl gets
# there, but the short reaches (a fist or EGYPT's crook) cannot without a step, and fall short of him.
REACH = 140.0
TIP = 145.0
REACH_ARM = 0.95
# How much of that slide is done by the end of the wind-up (frame 6), the rest as the spear is levelled (to 8): slid
# all the way in the wind-up, a long spear's butt swung back faster than the thrust moves its tip.
SLID = 0.6
SHIELD_UP = ("up", "world", (0.0, 0.0, 1.0))  # a shield's own up kept up as it turns


def _from_rest(axis, want, roll=0.0):
    """An item spec (_hand) turning what a hand holds by the least turn from where it stands at rest that lays its
    `axis` along `want` (armature space): a spear or club upright at rest only pitches forward or back, so between
    two such keys its tip stays ahead of the man instead of swinging across his face. `roll`: degrees it then turns
    about `want`, in place."""
    return (axis, "world", want, "rest", roll) if roll else (axis, "world", want, "rest")


def _slide_to(poser, pose, ahead):
    """How far (cm) what the +X hand holds slides back through the fist (Poser.pose's "slide_r") for its tip to lie
    `ahead` cm ahead of the hips (-Y) in key `pose`: negative slides it on, for a sceptre gripped at its middle that
    falls short."""
    item = poser.items["r"]
    M = poser.pose(pose, pose, 0.0)
    hand = M["hand_r"] @ poser.rest["hand_r"].inverted()
    tip = hand @ item["tip_at"]
    line = hand.to_3x3() @ item["long"]
    return (-tip.y - ahead) / max(-line.y, 0.2)


def _arm_out(poser, pose, s, toward, share=REACH_ARM):
    """A wrist target ("world") for arm <s> in key `pose`, reaching from where the pose puts its shoulder toward the
    point `toward`, a share `share` of the arm's length out (nearly straight: as far as an arm reaches)."""
    shoulder = poser.pose(pose, pose, 0.0)["upperarm_" + s].translation
    c = poser.chains["arm" + s]
    return ("world", tuple(shoulder + (Vector(toward) - shoulder).normalized() * share * (c["l1"] + c["l2"])))


FLAT = 2.0  # a club at least this many times as wide as it is deep is flat, with a face and an edge


def _edge(poser, s, want):
    """The roll (degrees) about its own line, laid along `want`, that turns a flat club's face across its swing (its
    thinnest axis, item_axes' "thin", to the side, +-X), so its edge leads: a macuahuitl held upright at rest faces
    forward, and pitched forward it chopped with the flat. None (0) for a round one (a crook: it has no face, and rolled
    between keys by different amounts it swung out to the side)."""
    item = poser.items[s]
    if item["width"] < FLAT * item["depth"]:
        return 0.0
    u = Vector(want).normalized()
    thin = item["long"].rotation_difference(u) @ item["thin"]
    a = (thin - u * thin.dot(u)).normalized()
    side = Vector((1.0, 0.0, 0.0))
    b = (side - u * side.dot(u)).normalized()
    if a.dot(b) < 0.0:
        b = -b
    return math.degrees(math.atan2(a.cross(b).dot(u), a.dot(b)))


def _guard(poser):
    """The off (-X) arm while the +X one strikes: a shield, or the empty hand, brought up in front as a guard."""
    pose = {"wrist_l": ("world", (-14.0, -34.0, 118.0)), "elbow_l": ("world", (-1.0, 0.2, -0.6))}
    if poser.items.get("l"):
        pose["item_l"] = ("thin", "world", (0.0, -1.0, 0.15)) + SHIELD_UP
    return pose


SUPPORT = 6.0  # cm: a gun's other wrist below the gun's centre line, so the hand lies under the barrel
GUN_HAND = 11.0  # cm: the firing wrist at least this far out (+X) from the middle of the chest
KICK = (5.0, 8.0, 6.0)  # a shot's recoil: cm the hands go back, degrees the muzzle climbs and the chest leans back
LOWERED = (34.0, 2.0, 112.0)  # cm: a bow's draw wrist on its way back down after the loose, out at the side
DROP = 21  # frame: where it passes there on its way to rest


def strike(poser, cls):
    """The Strike for a weapon class: keys [(frame, ease, pose)] over 24 frames, the blow at frame 10 (ONCE). What the
    hands hold starts and ends as it stands at rest ("rest", _hand). A spear, club or gun is turned only by the least
    turn from there (_from_rest), so a spear or club pitches forward and back in front of the man and never sweeps
    through him; a bow at the draw is aimed by its own axes, and a shield (_guard) by its face and its up."""
    rest = {"item_r": "rest", "item_l": "rest"}
    if cls == "spear":
        # Overhand: drawn back beside the ear by 6, the chest turned back, the spear levelled as the hand rises (its
        # tip still a little up: levelled over the whole wind-up, it turns no faster than the thrust moves it) and slid
        # back through the fist (Poser.pose's "slide_r", SLID of the way by 6) so the hand holds it nearer its tip, and
        # level by 8; thrust along it by 10 (the wrist in a straight line, from beside the ear down to chest height, a
        # little steeper than the spear), its tip in a foe at the battle's spacing (TIP), the chest turned and leaning
        # into it; a little further through by 13; drawn back beside the ear again by 18 (the wind-up's pose), and
        # back to rest by 24 as it came, the spear raised upright and slid on through the fist to where it is held at
        # rest. (Slid on as it went straight down to rest, the spear's butt went into the ground, and its tip on into
        # the foe, 15 to 20 cm past the blow.)
        guard = _guard(poser)
        blow = dict(guard, spine=(12.0, 25.0, 0.0), head=(-8.0, -20.0, 0.0), wrist_r=("world", (26.0, -56.0, 128.0), "straight"),
                    elbow_r=("world", (0.6, 0.2, -1.0)), item_r=_from_rest("long", LEVEL[1]))
        slide = _slide_to(poser, blow, TIP)
        blow["slide_r"] = (slide,)
        back = dict(guard, spine=(-3.0, -15.0, 0.0), head=(0.0, 12.0, 0.0), wrist_r=("world", (30.0, 14.0, 152.0)),
                    elbow_r=("world", (1.0, 0.3, -0.7)), item_r=_from_rest("long", (0.0, -1.0, 0.36)), slide_r=(SLID * slide,))
        aim = dict(back, wrist_r=("world", (30.0, 10.0, 146.0)), item_r=_from_rest("long", LEVEL[1]), slide_r=(slide,))
        through = dict(blow, spine=(14.0, 28.0, 0.0), wrist_r=("world", (26.0, -60.0, 125.0)))
        return [(0, None, rest), (6, "inout", back), (8, "inout", aim), (10, "in", blow), (13, "out", through), (18, "inout", back),
                (24, "inout", rest)]
    if cls == "club":
        # Overhead chop: raised over the shoulder by 6 with the club's head tipped back; down in front by 10, the
        # chest pitched well forward into it and the arm reaching nearly straight out and a little down (_arm_out),
        # the hand at the chest's height and the club pointing forward and down about 15 degrees; on through and down
        # by 13; brought up again in front by 20; back to rest by 24. A flat club (a macuahuitl) is rolled about its
        # own line so its edge leads (_edge).
        guard = _guard(poser)

        def club(want):
            return _from_rest("long", want, _edge(poser, "r", want))
        up = dict(guard, spine=(-6.0, -15.0, 0.0), head=(-5.0, 10.0, 0.0), wrist_r=("world", (28.0, 10.0, 160.0)),
                  elbow_r=("world", (1.0, 0.0, -0.4)), item_r=club((0.1, 0.6, 0.8)))
        blow = dict(guard, spine=(24.0, 18.0, 0.0), head=(-4.0, -12.0, 0.0), elbow_r=("world", (0.6, 0.6, -0.6)),
                    item_r=club((0.0, -0.96, -0.28)))
        blow["wrist_r"] = _arm_out(poser, blow, "r", (22.0, -REACH, 90.0))
        through = dict(blow, spine=(26.0, 20.0, 0.0), head=(-10.0, -12.0, 0.0), item_r=club((0.0, -0.64, -0.77)))
        through["wrist_r"] = _arm_out(poser, through, "r", (20.0, -REACH, 40.0), 0.9)
        down = dict(guard, spine=(5.0, 6.0, 0.0), wrist_r=("world", (28.0, -32.0, 100.0)), elbow_r=("world", (0.6, 0.4, -1.0)),
                    item_r=club((0.0, -0.3, 0.95)))
        return [(0, None, rest), (6, "inout", up), (10, "in", blow), (13, "out", through), (20, "inout", down), (24, "inout", rest)]
    if cls == "fist":
        # A straight punch: the fist drawn back to the ribs at 6, the forearm about level and the elbow down and back
        # behind him (chambered: with the wrist behind the shoulder the elbow went up behind it, a chicken wing), the
        # chest turned back; thrown out in a straight line (not round the shoulder, which would scoop it) to shoulder
        # height by 10, the arm nearly straight (_arm_out) and the chest turned and leaning well into it.
        guard = {"wrist_l": ("world", (-10.0, -24.0, 132.0)), "elbow_l": ("world", (-0.6, 0.0, -1.0))}
        back = dict(guard, spine=(0.0, -20.0, 0.0), head=(0.0, 15.0, 0.0), wrist_r=("world", (24.0, -5.0, 112.0)),
                    elbow_r=("world", (0.1, 0.8, -0.6)))
        blow = dict(guard, spine=(16.0, 32.0, 0.0), head=(-10.0, -26.0, 0.0), elbow_r=("world", (1.0, 0.0, -0.6)))
        blow["wrist_r"] = _arm_out(poser, blow, "r", (6.0, -REACH, 128.0)) + ("straight",)
        through = dict(blow, spine=(17.0, 34.0, 0.0))
        through["wrist_r"] = _arm_out(poser, through, "r", (6.0, -REACH, 127.0), 0.97)
        return [(0, None, rest), (6, "inout", back), (10, "in", blow), (13, "out", through), (24, "inout", rest)]
    if cls == "bow":
        # Raise, draw and loose: the bow arm out in front at shoulder height by 8, the bow upright and canted a little,
        # the chest turned to bring the bow's shoulder forward and the draw hand at the chin; held to 10, the loose; the
        # draw hand snaps back past the ear and out, about 20 cm in the one frame, and the bow arm drops a little, by 11
        # (the follow through, held to 14, so the shot shows without a string or arrow); back to rest by 24 in one move
        # eased in and out (inout1, inout2), the draw hand passing out at the side (LOWERED) at DROP, its elbow out:
        # swung round the shoulder from beside the ear straight to its rest, it came down across the chest and its
        # forearm through the belly. The loose is the quickest move of the take (the wrist 20 cm a frame, against at
        # most 16 on the way down and on the way up; the hand's end 19, against 15 to 19 about DROP and 18 into frame
        # 3: eased out over two frames it went 15, slower than the way down), and no one frame of the way down stands
        # out; the chest, the head, the bow arm and the bow are where that one move puts them there.
        drawn = {"spine": (2.0, -35.0, 0.0), "head": (0.0, 30.0, 0.0), "wrist_l": ("world", (-12.0, -64.0, 136.0)), "item_r": "rest",
                 "elbow_l": ("world", (-1.0, 0.0, -0.3)), "item_l": ("long", "world", (-0.17, 0.0, 0.98), "thin", "world", (-1.0, 0.0, 0.0)),
                 "wrist_r": ("world", (5.0, -14.0, 146.0)), "elbow_r": ("world", (1.0, 0.8, 0.3))}
        loosed = dict(drawn, wrist_r=("world", (16.0, 2.0, 149.0)), elbow_r=("world", (1.0, 1.0, 0.2)),
                      wrist_l=("world", (-12.0, -62.0, 129.0)))
        share = _ease("inout", (DROP - 14) / 10.0)
        M = poser.pose(loosed, rest, share)
        shoulder, elbow = M["upperarm_l"].translation, M["forearm_l"].translation
        wrist = M["forearm_l"] @ (poser.rest["forearm_l"].inverted() @ poser.at["forearm_l"][1])
        lowered = dict(loosed, spine=tuple((1.0 - share) * x for x in loosed["spine"]), head=tuple((1.0 - share) * x for x in loosed["head"]),
                       wrist_l=("world", tuple(wrist)), elbow_l=("world", tuple((elbow - shoulder).normalized())),
                       wrist_r=("world", LOWERED), elbow_r=("world", (1.0, 0.5, -0.3)))
        if poser.items.get("l"):
            bow = M["hand_l"].to_3x3() @ poser.rest["hand_l"].to_3x3().transposed() @ poser.items["l"]["long"]
            lowered["item_l"] = _from_rest("long", tuple(bow))
        return [(0, None, rest), (8, "inout", drawn), (10, "linear", drawn), (11, "linear", loosed), (14, "linear", loosed),
                (DROP, "inout1", lowered), (24, "inout2", rest)]
    if cls == "gun":
        # Shouldered and aimed by 7 (the butt at the shoulder, the muzzle forward and level, the other hand under the
        # barrel), fired at 10: the recoil kicks the hands back KICK[0] cm and up, the muzzle up KICK[1] degrees (the
        # other hand rising with the barrel) and the chest back KICK[2] degrees by 11, settling back to the aim by 14;
        # lowered to rest by 24.
        item = poser.items.get("r")
        # The butt this far behind the hand, so it comes to the pocket: the Rifleman holds his rifle by its butt (0.6
        # cm), and kept 10 cm off, its butt floated before his chin, 24 cm from his shoulder's joint.
        grip = max(0.0, min(30.0, item["behind"])) if item else 20.0
        pocket = Vector((13.0, -4.0, 140.0))  # the butt: in front of the shoulder, which the chest's turn takes back
        hand = pocket + Vector((0.0, -grip, 0.0))
        aimed = {"spine": (0.0, -25.0, 0.0), "head": (6.0, 18.0, 6.0), "wrist_r": ("world", tuple(hand)), "item_l": "rest",
                 "elbow_r": ("world", (0.8, 0.2, -1.0)), "item_r": ("long", "world", (0.0, -1.0, 0.02), "rest"), "elbow_l": ("world", (-0.3, 0.0, -1.0))}
        line = hand.copy()
        if item:
            # The gun's line through the pocket (the gun sits off the hand as it was modelled): the hand moved by
            # however far its line misses, which leaves the gun's turn (from rest, not from the forearm) as it was.
            M = poser.pose(aimed, aimed, 0.0)
            line = M["hand_r"] @ poser.rest["hand_r"].inverted() @ item["centre"]
            hand += Vector((pocket.x - line.x, 0.0, pocket.z - line.z - 0.02 * (line.y - pocket.y)))
            hand.x = max(hand.x, GUN_HAND)  # (a gun much wider on one side would take the hand across the chest)
            aimed["wrist_r"] = ("world", tuple(hand))
            M = poser.pose(aimed, aimed, 0.0)
            line = M["hand_r"] @ poser.rest["hand_r"].inverted() @ item["centre"]
        # The other hand under the barrel, 30 cm ahead of the firing hand.
        y = hand.y - 30.0
        support = Vector((line.x - 1.0, y, line.z + 0.02 * (line.y - y) - SUPPORT))
        aimed["wrist_l"] = ("world", tuple(support))
        climb = math.radians(KICK[1])
        up = Vector((0.0, KICK[0], 2.0))
        kick = dict(aimed, spine=(-KICK[2], -25.0, 0.0), item_r=("long", "world", (0.0, -1.0, 0.02 + math.tan(climb)), "rest"),
                    wrist_r=("world", tuple(hand + up)), wrist_l=("world", tuple(support + up + Vector((0.0, 0.0, (hand.y - y) * math.sin(climb))))))
        return [(0, None, rest), (7, "inout", aimed), (10, "linear", aimed), (11, "out", kick), (14, "inout", aimed), (24, "inout", rest)]
    raise ValueError("no strike for class %r" % cls)


def _shield(poser, s):
    """Whether hand <s> holds a shield (on the -X side, short, facing forward at rest)."""
    item = poser.items.get(s)
    return bool(item) and s == "l" and item["length"] < SPEAR and item["thin"].y < -0.5


def hit(poser):
    """Hit: the chest snapped back 20 degrees and the head a further 6, so the top of the chest goes about 13 cm back and
    the head about 18 (readable from the game's camera, 8 m and more away: back 10 and 12 degrees, the neck moved 6
    cm), and the upper arms out and back 10 (an arm with a shield out and a little forward instead, or the shield
    would go into the belly), most at frame 2, eased back to rest by 8. What the hands hold stays as it stands at rest
    ("rest", _hand), carried by the hands but not tipped with the arms: a bow or musket upright at the side, tipped as
    the arm swings out, put its top through a hat's brim."""
    rest = {"item_r": "rest", "item_l": "rest"}
    peak = dict(rest, spine=(-20.0, 0.0, 0.0), head=(-6.0, 0.0, 0.0))
    for s in ("r", "l"):
        peak.update(poser.swung(s, out=12.0, back=-4.0) if _shield(poser, s) else poser.swung(s, out=10.0, back=10.0))
    return [(0, None, rest), (2, "out", peak), (8, "inout", rest)]


def _flat(poser, s, toward, roll=0.0, share=1.0):
    """What hand <s> holds as it lies on the ground in a fall, turned there by the least turn from where it stands at
    rest (_from_rest): a spear, staff, club, musket or bow tips over forward or back beside the man, its business end
    toward `toward` (+1 or -1 along Y), and a shield tips over onto its face, the arm on its back. Either way it only
    pitches, out at the side, so on its way down it passes beside him and not over his head or through his legs.
    `roll`: degrees a long one then turns in place about its own line (settle: so it lies under the hand, not on it).
    `share`: how far over it has tipped (1 lying, 0 as it stands at rest)."""
    item = poser.items.get(s)
    if not item:
        return None
    axis, want = ("thin", Vector((0.0, 0.0, -1.0))) if _shield(poser, s) else ("long", Vector((0.0, toward, 0.0)))
    return _from_rest(axis, tuple(item[axis].slerp(want, share)), roll)


SHIELD_OUT = 45.0  # degrees: a shield arm swung out from the side as he falls (the shield tips over clear of the legs)
TIPPED = 0.7  # how far over what the hands hold has tipped as the knees give (frame 14): the rest as he goes down
# The same falling back, by the end of the stagger: as far as his hips have dropped toward his seat (SAG), so it falls
# with him in one quickening move (tipped 0.7 by then, it came over at a run and all but stopped there; tipped 0.3
# with the hips sunk 0.38 of the way, a 2.4 m spear's tip came down the rest of the way 61 cm in the last frame).
TIPPED_BACK = SAG


def _out(poser, s, out):
    """How far arm <s> swings out in a fall: `out`, or SHIELD_OUT for a shield arm."""
    return max(out, SHIELD_OUT) if _shield(poser, s) else out


def _bisect(f, lo, hi, want, steps=16):
    """x in lo..hi where f(x), rising with x, reaches `want` (lo or hi when it never does)."""
    if f(lo) >= want:
        return lo
    if f(hi) <= want:
        return hi
    for _ in range(steps):
        mid = 0.5 * (lo + hi)
        lo, hi = (mid, hi) if f(mid) < want else (lo, mid)
    return 0.5 * (lo + hi)


LEG_REACH = 10.0  # cm: a lying leg's ankle aimed this far past its reach, so the leg lies straight
# cm: how far above the ground a lying man's head (its hat too) may hang, and how near it settle then lays it
HEAD_HANG, HEAD_LAID = 15.0, 4.0
# cm: the most a key opens the seams between the head and the body (Skin.neck), about as far as a plain neck opens
# with the face turned 70 degrees onto its side as he lies on it (Death). A quiver or a toga's shoulder worn across the
# neck sits up to 30 cm from the head's joint, and turned that far it tore open 31 cm (Archer_American).
NECK = 10.0


def _neck(poser, pose):
    """Turns a key's head back toward the chest (`pose`, changed in place) by the least share of its turn that keeps the
    neck within NECK, and the chest back toward the hips likewise if the head straight on it is not enough (a quiver
    from the hips to the head, its middle on neither). A plain neck is left as it is."""
    for bone in ("head", "spine"):
        if bone not in pose:
            continue

        def opened(share, bone=bone, angles=tuple(pose[bone])):
            q = dict(pose, **{bone: tuple(share * x for x in angles)})
            return poser.skin.neck(poser.pose(q, q, 0.0))
        wide = opened(1.0)
        if wide <= NECK:
            break
        share = _bisect(opened, 0.0, 1.0, NECK)
        if opened(share) < wide:  # (a chest turned back that leaves the neck as open is left as it was)
            pose[bone] = tuple(share * x for x in pose[bone])
        if share > 0.0:
            break
    return pose


def settle(poser, pose, how, arms=None):
    """Lays a fall's key `pose` (a dict, changed in place) on the ground, measured on the posed body (Poser.skin):
    - "clear": nothing goes below FLOOR (the hips, then the wrists, raised if it would);
    - "knees": the hips lowered or raised until the lowest of the body, legs and head (a knee, as he topples over his
      knees, or the seat as he sits down) lies at FLOOR, the wrists raised so neither arm nor what it holds goes below;
    - "down", landed: the hips until the chest lies at FLOOR; each leg straight from its hip, swung down about it until
      it lies there too; the head turned up (the chin raised, or brought round) by the least that keeps a hat, crown or
      crest out of the ground rather than propping the man up on it, or down onto it if it would hang more than
      HEAD_HANG above it, each turn tried scored against NECK (one that opens the neck wider costs more: a quiver
      across it; settle does not call _neck here, nor turn the chest); and each wrist lowered or raised until the arm or what its hand holds (a shield face down under the
      forearm) lies at FLOOR, after `arms` (if given) has laid the arms out from where the shoulders now are (_limp).
    The wrists and ankles become world targets."""
    skin = poser.skin

    def low(p, *parts):
        M = poser.pose(p, p, 0.0)
        return min(skin.low(M, part) for part in parts)

    def hips(p, dz):
        q = dict(p)
        x = list(p.get("pelvis", ())) + [0.0] * (7 - len(p.get("pelvis", ())))
        x[2] += dz
        q["pelvis"] = tuple(x)
        return q

    body = ("torso", "leg_r", "leg_l", "head")  # (the feet stand on the ground until he lands)
    if how == "down":
        for _ in range(2):  # (the shoulders lie with the chest, and the arms hang from them: twice settles both)
            pose.update(hips(pose, FLOOR - low(pose, "torso", "upper_r", "upper_l")))
            M = poser.pose(pose, pose, 0.0)
            for s in ("r", "l"):
                hip = M["thigh_" + s].translation
                reach = poser.chains["leg" + s]["l1"] + poser.chains["leg" + s]["l2"] + LEG_REACH
                flat = Vector(pose.get("ankle_" + s, poser.rest_spec["ankle_" + s])[1]) - hip
                flat.z = 0.0
                flat.normalize()
                side = flat.cross(Vector((0.0, 0.0, 1.0)))
                # The foot as the key turns it against the leg (degrees up from the line of the leg).
                knee = M["shin_" + s] @ (poser.rest["shin_" + s].inverted() @ poser.at["shin_" + s][1]) - hip
                foot = M["foot_" + s].to_3x3() @ Vector((0.0, 1.0, 0.0))
                bent = math.degrees(math.atan2(foot.z, foot.dot(flat)) - math.atan2(knee.z, knee.dot(flat)))

                def leg(drop, up, out, s=s, hip=hip, flat=flat, reach=reach, side=side, bent=bent):
                    line = Matrix.Rotation(math.radians(-drop), 3, side) @ flat
                    toe = Matrix.Rotation(math.radians(out if s == "r" else -out), 3, line) @ Matrix.Rotation(math.radians(bent + up - drop), 3, side) @ flat
                    return dict(pose, **{"ankle_" + s: ("world", tuple(hip + line * reach)), "toe_" + s: ("world", tuple(toe))})
                # For each turn of the foot (up or down from its key, and out to the side as a dead man's feet fall),
                # the leg swung down until it or the foot (a heel under a man on his back) lies at FLOOR; then the turn
                # that lets the leg lie lowest, as little as may be.
                best = None
                for up in range(-60, 70, 10):
                    for out in (-30, 0, 30, 60):
                        drop = _bisect(lambda d: -low(leg(d, up, out), "leg_" + s, "foot_" + s), -30.0, 60.0, -FLOOR)
                        q = leg(drop, up, out)
                        score = low(q, "leg_" + s) + 0.05 * abs(up) + 0.03 * abs(out)
                        if best is None or score < best[0]:
                            best = (score, q)
                pose.update(best[1])
            head = pose.get("head", (0.0, 0.0, 0.0))
            M = poser.pose(pose, pose, 0.0)
            gap = skin.low(M, "head") - FLOOR
            if gap < 0.0 or gap > HEAD_HANG or skin.neck(M) > NECK:
                # By the least turn that clears the ground, or (hanging above it, a pack under his shoulders lifting
                # them) that lays it down within HEAD_LAID of it, as a dead man's head falls back: the chin raised
                # (pitched back) or lowered, and the face brought round toward straight down or up, scored by how far
                # it turns.
                top = HEAD_LAID if gap > HEAD_HANG else math.inf
                best = None
                for pitch in range(-75, 65, 5):
                    for turn in sorted({head[1], head[1] * 0.5, 0.0}):
                        q = dict(pose, head=(head[0] + pitch, turn, head[2]))
                        M = poser.pose(q, q, 0.0)
                        gap = skin.low(M, "head") - FLOOR
                        miss = -gap if gap < 0.0 else max(0.0, gap - top)
                        # (and, the ground cleared, no wider open at the neck than NECK: _neck)
                        score = abs(pitch) + 0.5 * abs(turn - head[1]) + 1e2 * max(0.0, skin.neck(M) - NECK) if not miss else 1e4 + miss
                        if best is None or score < best[0]:
                            best = (score, q["head"])
                pose["head"] = best[1]
            if arms:
                arms(pose)
            for s in ("r", "l"):
                M = poser.pose(pose, pose, 0.0)
                wrist = M["forearm_" + s] @ (poser.rest["forearm_" + s].inverted() @ poser.at["forearm_" + s][1])
                spec = pose.get("item_" + s)
                # A long item lying beside him is rolled about its own line (in place) to lie under the hand rather
                # than on it, held up by the arm: for each roll the wrist goes down until the arm or the item lies at
                # FLOOR, and the roll that lays the item lowest wins, the hand kept near the ground and the roll small.
                rolls = range(0, 360, 30) if spec and spec[0] == "long" and spec[3:4] == ("rest",) else (None,)
                best = None
                for roll in rolls:
                    turned = dict(pose, **{"item_" + s: spec[:4] + ((roll,) if roll else ())}) if roll is not None else pose

                    def arm(z, s=s, wrist=wrist, turned=turned):
                        return dict(turned, **{"wrist_" + s: ("world", (wrist.x, wrist.y, z))})
                    q = arm(_bisect(lambda z: low(arm(z), "fore_" + s, "item_" + s), -20.0, 60.0, FLOOR))
                    score = low(q, "item_" + s) + 0.3 * low(q, "upper_" + s, "fore_" + s) + 0.005 * min(roll or 0, 360 - (roll or 0))
                    if best is None or score < best[0]:
                        best = (score, q)
                pose.update(best[1])
        return pose
    if how == "knees":
        pose.update(hips(pose, _bisect(lambda dz: low(hips(pose, dz), *body), -40.0, 40.0, FLOOR)))
    elif low(pose, *body) < FLOOR:
        pose.update(hips(pose, _bisect(lambda dz: low(hips(pose, dz), *body), 0.0, 40.0, FLOOR)))
    M = poser.pose(pose, pose, 0.0)
    for s in ("r", "l"):
        if skin.low(M, "arm_" + s) < FLOOR:
            wrist = M["forearm_" + s] @ (poser.rest["forearm_" + s].inverted() @ poser.at["forearm_" + s][1])

            def arm(z, s=s, wrist=wrist):
                return dict(pose, **{"wrist_" + s: ("world", (wrist.x, wrist.y, z))})
            pose.update(arm(_bisect(lambda z: low(arm(z), "arm_" + s), wrist.z, wrist.z + 80.0, FLOOR)))
    return pose


def death(poser):
    """Death, falling forward: recoils (the chest and head jolted back) to 6, what the hands hold kept as it stands,
    still moving there ("off") as the knees give; they buckle with the feet planted, down through a crouch at 14 (the
    hips at about 52 cm, the knees still 33 cm off the ground) without stopping in it ("on" into it and on from it),
    what the hands hold tipped over beside him on the way (_flat); topples forward over his knees, which come down
    onto the ground at 19, and on at an even pace (the knees slow him there, but do not stop him) onto the face by 24
    (the hips pitched about 85 degrees, the legs straight out behind, the feet pointed: from the crouch to 19 each
    foot turns down against its shin, its heel lifted where it would go into the ground, Poser.pose); lands at 24 with
    a 2 cm bounce; lies still from 26 (ONCE: from 28). Eased in and out of the recoil and the crouch, he all but
    stopped in each (his vertices moving 1 and 3.5 cm a frame there, against 15 to 20 about them). Each key from the
    crouch on is laid on the ground (settle). The head ends ahead of the hips (-Y)."""
    planted = {}
    for s in ("r", "l"):
        planted["ankle_" + s] = poser.rest_spec["ankle_" + s]
    lying = {}
    for s in ("r", "l"):
        # The legs out behind the hips (which end about 80 cm ahead; settle lays them on the ground).
        lying["ankle_" + s] = ("world", tuple(poser.at["shin_" + s][1] + Vector((0.0, 4.0, 2.0))))
        lying["toe_" + s] = ("shin", (0.0, -0.25, -1.0))
        lying["item_" + s] = _flat(poser, s, -1.0)
    recoil = dict(planted, spine=(-12.0, 0.0, 0.0), head=(-15.0, 0.0, 0.0), item_r="rest", item_l="rest",
                  **poser.swung("r", _out(poser, "r", 15.0), 10.0, 10.0), **poser.swung("l", _out(poser, "l", 15.0), 10.0, 10.0))
    crouch = dict(planted, pelvis=(0.0, 4.0, -40.0, 20.0, 0.0, 0.0), spine=(25.0, 0.0, 0.0), head=(20.0, 0.0, 0.0),
                 **poser.swung("r", _out(poser, "r", 10.0), -25.0, 20.0), **poser.swung("l", _out(poser, "l", 10.0), -25.0, 20.0))
    for s in ("r", "l"):
        crouch["item_" + s] = _flat(poser, s, -1.0, share=TIPPED)
    # Toppling over the knees, which come down onto the ground about 35 cm ahead: the hips over them, a thigh's length
    # up, the shins going down flat behind, the arms reaching forward.
    falling = dict(lying, pelvis=(0.0, -46.0, -42.0, 45.0, 0.0, 0.0), spine=(20.0, 0.0, 0.0), head=(5.0, 30.0, 0.0),
                   **poser.swung("r", _out(poser, "r", 20.0), -60.0, 15.0), **poser.swung("l", _out(poser, "l", 20.0), -60.0, 15.0))
    for s in ("r", "l"):
        falling["ankle_" + s] = ("world", tuple(poser.at["shin_" + s][1] + Vector((0.0, 3.0, 3.0))))
        falling["toe_" + s] = ("shin", (0.0, -0.6, -1.0))
    down = dict(lying, pelvis=(0.0, -80.0, -78.0, 85.0, 0.0, 0.0), spine=(5.0, 0.0, 0.0), head=(0.0, 70.0, 0.0),
                **poser.swung("r", ARMS_OUT, 0.0, 10.0), **poser.swung("l", ARMS_OUT, 0.0, 10.0))
    for pose in (recoil, crouch, falling):
        _neck(poser, pose)
    settle(poser, crouch, "clear")
    settle(poser, falling, "knees")
    settle(poser, down, "down")
    bounce = dict(down, pelvis=tuple(down["pelvis"][:2]) + (down["pelvis"][2] + 2.0,) + tuple(down["pelvis"][3:]))
    return [(0, None, {}), (6, "off", recoil), (14, "on", crouch), (19, "on", falling), (24, "linear", down), (25, "out", bounce),
            (26, "in", down), (36, "linear", down)]


def _limp(poser, pose):
    """Lays each arm of a man on his back (DeathBack's down key, `pose`, its body settled; changed in place) limp on the
    ground beside him: out from the shoulder LIMP degrees from straight out toward his feet, the upper arm dropped to
    the ground as far as it reaches (from a shoulder a roll onto the other side has raised it hangs nearly straight
    down), the elbow bent down to it and the forearm lying on it. settle then lowers each wrist onto the ground."""
    M = poser.pose(pose, pose, 0.0)
    for s, x in (("r", 1.0), ("l", -1.0)):
        c = poser.chains["arm" + s]
        shoulder = M["upperarm_" + s].translation
        out = Vector((x * math.cos(math.radians(LIMP)), -math.sin(math.radians(LIMP)), 0.0))
        drop = max(0.0, shoulder.z - ELBOW_LIES)
        elbow = Vector((shoulder.x, shoulder.y, ELBOW_LIES)) + out * math.sqrt(max(0.0, c["l1"] ** 2 - drop ** 2))
        wrist = elbow + out * c["l2"]
        line = (wrist - shoulder).normalized()
        bend = (elbow - shoulder) - line * (elbow - shoulder).dot(line)
        pose["wrist_" + s] = ("world", tuple(wrist))
        pose["elbow_" + s] = ("world", tuple(bend.normalized()) if bend.length > 1.0 else (0.3 * x, 0.0, -1.0))
    return pose


def death_back(poser, roll=0.0):
    """DeathBack, falling onto the back: recoils to 6 (still moving there, "off"); staggers back by 14, the knees giving
    more and more (the hips about 10 cm back and SAG of the way down to his seat, about 33 cm, lower, the feet
    planted), what the hands hold tipped over beside him on the way (_flat); falls onto his seat at 21 (ONCE's
    "seat"), the feet sliding out in front, the knees still bent (SIT); and topples back onto the ground by 26 (ONCE's
    "down": the hips pitched about 80 degrees back, the legs straightening flat), his chest bouncing up off it
    (REBOUND), and lies still, rolled `roll` degrees onto his +X side (negative: his -X side), his arms limp on the
    ground beside him (_limp). From the recoil to his seat it is one fall, quickening all the way ("on" to the
    stagger, then "on"): the recoil keeps the hips up, so his knees give all at once at 6, the hips starting down at
    about 3 cm a frame, and they drop the 86 cm from standing in 15 frames, quickening by about 0.3 g on average and
    up to about 0.7 g into his seat (his legs give under him, they do not vanish), land still moving at about 10 cm a
    frame, and the chest goes on back from there. A man with a sword or a quiver hanging low at his hip lands on its
    tip, higher (Leader's and Leader_ARABIA's hips 50 cm up, Leader_JAPAN's 29, Archer_MiddleEast's 33), his stagger
    sinking that much less, so his hips start down slower (1.5 to 2.2 cm a frame), his fall quickens all the way too
    and lands still moving, at about 5 to 8 cm a frame; one whose back rests on a pack as he lies, so that his
    hips would rise by more than PROPPED after he sat (the Rifleman's, 21 cm, onto his pack), lands on it instead, his
    seat leant back as far as keeps his hips from rising once he is down. (Sat down in 5 frames, eased in and out, the
    hips fell 76 cm at up to 7.6 g and stopped dead on the seat for a frame before he toppled; eased out of the recoil
    and into the stagger, he stood all but still from frame 4 to 8.) Each key from the stagger on is laid on the
    ground (settle), the one he lies in first. The head ends behind the hips (+Y)."""
    planted = {}
    for s in ("r", "l"):
        planted["ankle_" + s] = poser.rest_spec["ankle_" + s]
    lying = {}
    for s in ("r", "l"):
        # The legs out in front of the hips (which end about 40 cm behind; settle lays them on the ground).
        lying["ankle_" + s] = ("world", tuple(poser.at["shin_" + s][1] + Vector((0.0, -42.0, 2.0))))
        lying["toe_" + s] = ("shin", (0.0, -1.0, -0.5))  # (relaxed, the toes up: the legs lie on the heels)
        lying["item_" + s] = _flat(poser, s, 1.0)
    recoil = dict(planted, spine=(-15.0, 0.0, 0.0), head=(-20.0, 0.0, 0.0), item_r="rest", item_l="rest",
                  **poser.swung("r", _out(poser, "r", 20.0), 15.0, 10.0), **poser.swung("l", _out(poser, "l", 20.0), 15.0, 10.0))
    stagger = dict(planted, pelvis=(0.0, 10.0, -24.0, -8.0, 0.0, 0.0), spine=(-10.0, 0.0, 0.0), head=(-8.0, 0.0, 0.0),
                   **poser.swung("r", _out(poser, "r", 30.0), 10.0, 25.0), **poser.swung("l", _out(poser, "l", 30.0), 10.0, 25.0))
    for s in ("r", "l"):
        stagger["item_" + s] = _flat(poser, s, 1.0, share=TIPPED_BACK)
    sitting = dict(lying, pelvis=(0.0, 30.0, -66.0, -25.0, 0.0, 0.0), spine=(-10.0, 0.0, 0.0), head=(5.0, 0.0, 0.0),
                   **poser.swung("r", _out(poser, "r", 35.0), 20.0, 20.0), **poser.swung("l", _out(poser, "l", 35.0), 20.0, 20.0))
    for s in ("r", "l"):
        sitting["ankle_" + s] = ("world", tuple(poser.at["shin_" + s][1] + Vector((0.0, -SIT, 2.0))))
        sitting["knee_" + s] = ("world", (0.0, -0.3, 1.0))
    # Down: rolled a little onto one side (as a man seldom lands square, and so that a sword or a quiver worn at the
    # other hip, sticking out behind, lies along the ground instead of propping him up), the arms limp on the ground
    # beside him (_limp).
    down = dict(lying, pelvis=(0.0, 40.0, -78.0, -82.0, 0.0, 0.0, roll), spine=(-5.0, 0.0, 0.0), head=(10.0, -25.0, 0.0))
    for s in ("r", "l"):
        down["knee_" + s] = ("pelvis", (0.0, -1.0, 0.0))
    for pose in (recoil, stagger, sitting):
        _neck(poser, pose)
    settle(poser, down, "down", lambda pose: _limp(poser, pose))
    seat = dict(sitting)
    settle(poser, sitting, "knees")

    def hips(pose):
        return poser.pose(pose, pose, 0.0)["pelvis"].translation.z
    if hips(down) - hips(sitting) > PROPPED:
        # Propped up as he lies by what he carries on his back: he lands on it, his seat leant back (and rolled) toward
        # how he will lie until his hips are as high as they will lie, so they do not rise once he is down.
        def leant(share):
            a, b = tuple(seat["pelvis"]) + (0.0,), down["pelvis"]
            q = dict(seat, pelvis=a[:3] + (a[3] + share * (b[3] - a[3]),) + a[4:6] + (share * b[6],))
            return settle(poser, q, "knees")
        sitting = leant(_bisect(lambda share: hips(leant(share)), 0.0, 1.0, hips(down)))
    # The stagger sinks SAG of the way down to wherever he lands (higher on a sword or a quiver at his hip, or a pack on
    # his back, which props him up), so the fall goes on at the pace it has reached.
    stagger["pelvis"] = stagger["pelvis"][:2] + (SAG * sitting["pelvis"][2],) + stagger["pelvis"][3:]
    settle(poser, stagger, "clear")
    # He lands with his chest bouncing up off the ground (REBOUND), not his hips, which go no higher once he is down.
    bounce = dict(down, spine=(down["spine"][0] + REBOUND,) + tuple(down["spine"][1:]))
    return [(0, None, {}), (6, "off", recoil), (14, "on", stagger), (ONCE["DeathBack"]["seat"], "on", sitting), (26, "on", down),
            (27, "out", bounce), (28, "in", down), (36, "linear", down)]


# Finishing ---------------------------------------------------------------------------------------------------------

def _fcurves(action):
    """An action's curves: action.fcurves before Blender 5, the layers' strips' channel bags since."""
    if hasattr(action, "fcurves"):
        return list(action.fcurves)
    return [fc for layer in action.layers for strip in layer.strips for bag in getattr(strip, "channelbags", []) for fc in bag.fcurves]


def play(arm, action):
    """Makes `action` the armature's (and, in Blender 5, its first slot)."""
    arm.animation_data.action = action
    if hasattr(arm.animation_data, "action_slot") and getattr(action, "slots", None) and len(action.slots):
        arm.animation_data.action_slot = action.slots[0]


def key_rest(arm, action):
    """Keys at rest, at frame 0, every bone `action` leaves alone (and the place of the hips and hands, PLACED, if it
    leaves that), so every action keys every bone: a bone with no curve keeps whatever pose it last had, which
    Blender's FBX bake (and check_rigs) would carry from one action into the next."""
    keyed = {(fc.data_path.split('"')[1], fc.data_path.rsplit(".", 1)[1]) for fc in _fcurves(action) if '"' in fc.data_path}
    arm.animation_data.action = action
    for pb in arm.pose.bones:
        if (pb.name, "rotation_euler") not in keyed and (pb.name, "rotation_quaternion") not in keyed:
            pb.rotation_mode = "XYZ"
            pb.rotation_euler = (0.0, 0.0, 0.0)
            pb.keyframe_insert("rotation_euler", frame=0)
        if pb.name in PLACED and (pb.name, "location") not in keyed:
            pb.location = (0.0, 0.0, 0.0)
            pb.keyframe_insert("location", frame=0)


def finish_once(action, n):
    """A one-shot: kept (fake user) over frames 0..n, its keys straight from one frame to the next (as the game plays
    the baked frames), not looping."""
    action.use_fake_user = True
    action.frame_range = (0, n)
    for fc in _fcurves(action):
        for k in fc.keyframe_points:
            k.interpolation = "LINEAR"


def _coords(body):
    """The posed body's vertices (cm, as the armature deforms them now), one row each."""
    ev = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    me = ev.to_mesh()
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    return co.reshape(-1, 3)


def _cloth(body):
    """Which of a figure's vertices are cloth the skirt bones swing (a robe, a tunic's hem, a cape's skirt): a mask."""
    skirt = {g.index for g in body.vertex_groups if g.name in SKIRT}
    return np.array([any(g.group in skirt and g.weight > 0.0 for g in v.groups) for v in body.data.vertices], dtype=bool)


def _lowest(body, keep=None):
    """The lowest point (cm) of the posed body, or of the vertices `keep` (a mask)."""
    co = _coords(body)
    return float((co if keep is None else co[keep])[:, 2].min())


SKIRT = ("skirt_f", "skirt_b")
# degrees: the further turns of a skirt bone tried (DRAPE_STEP apart, up to DRAPE_MOST either way) and the most one
# changes from a frame to the next; and what a path of turns costs: a degree turned 1, DRAPE_SMOOTH per square degree
# changed in a frame, DRAPE_DEEP per cm of cloth below where it may lie.
DRAPE_STEP, DRAPE_MOST, DRAPE_RATE = 5.0, 180.0, 15.0
DRAPE_SMOOTH, DRAPE_DEEP = 0.05, 200.0
ROBE_UP = 30.0  # cm: the most a robe rises above the hips (the pelvis bone's head) in a fall, DRAPE_DEEP per cm over it


def drape(arm, body, action, buckle, down, n, turns=None):
    """Keeps a robe out of the ground in a fall while he stands: until `buckle` the skirt bones swing further where the
    cloth they carry would go below FLOOR (or below the rest of the figure, if that is lower). From `buckle` on, as his
    knees give, the robe goes down with his legs (key_skirt) and into the ground where it must, which hides it: a rigid
    robe cannot fold, and one swung clear of the ground stands up off a fallen man as a tent, or folds through his legs
    and bares them. Nor does it rise more than ROBE_UP above the hips: swung up with a raised knee, a robe's rigid front
    stood up like a sail before his face. The turns, either way up to DRAPE_MOST, are found for the whole take at once
    (the cheapest path through each frame's choices: as little turned, changed, sunk and raised as may be), so they
    change by at most DRAPE_RATE degrees a frame and a robe never pops from one shape to another; from `down`, when he
    has landed, they hold, so the robe lies as still as he does. Worked out on the cloth's own vertices, skinned here as
    Blender's armature does (weights over the pelvis and skirt bones). Returns the further turns, per bone and frame;
    given them (`turns`), only applies them."""
    groups = {g.name: g.index for g in body.vertex_groups}
    bones = ("pelvis",) + SKIRT
    rows, weights, cloth = [], [], []
    for v in body.data.vertices:
        w = {g.group: g.weight for g in v.groups}
        if any(w.get(groups[b], 0.0) > 0.0 for b in SKIRT):
            rows.append(tuple(v.co) + (1.0,))
            weights.append([w.get(groups[b], 0.0) for b in bones])
            cloth.append(v.index)
    if not rows:
        return None
    co = np.array(rows)
    W = np.array(weights)
    W /= W.sum(axis=1, keepdims=True)
    rest = np.ones(len(body.data.vertices), dtype=bool)
    rest[cloth] = False
    B, P = arm.data.bones, arm.pose.bones
    inv = [np.array(B[b].matrix_local.inverted()) for b in bones]
    R0 = {b: B[b].matrix_local.to_3x3() for b in SKIRT}

    def swing(b, d):  # the skirt bone's further turn by d degrees (key_skirt's sense, forward positive), in its frame
        return (R0[b].transposed() @ Matrix.Rotation(math.radians(-d), 3, "X") @ R0[b]).to_4x4()

    play(arm, action)
    if turns is None:
        scene = bpy.context.scene
        steps = np.arange(-DRAPE_MOST, DRAPE_MOST + 0.5 * DRAPE_STEP, DRAPE_STEP)
        S = len(steps)
        # Each cloth vertex's height in each frame from the hips' part of it and from each skirt bone turned each step,
        # and how low and how high the cloth may go in that frame.
        hips, part, floors, ceilings = [], [], [], []
        for f in range(n + 1):
            scene.frame_set(f)
            M = {b: P[b].matrix.copy() for b in bones}
            ceilings.append(P["pelvis"].head.z + ROBE_UP)
            hips.append(W[:, 0] * (co @ (np.array(M["pelvis"]) @ inv[0])[2]))
            part.append([W[:, k + 1:k + 2] * np.stack([co @ (np.array(M[b] @ swing(b, d)) @ inv[k + 1])[2] for d in steps], axis=1)
                         for k, b in enumerate(SKIRT)])
            floors.append(min(FLOOR, max(float(_coords(body)[rest, 2].min()), 0.0)) if f < buckle else -np.inf)
        # Changing a bone's turn from one step to another in a frame, and the most it may; and from when he has
        # landed, not at all, so the robe lies as still as he does.
        jump = steps[:, None] - steps[None, :]
        change = np.where(np.abs(jump) <= DRAPE_RATE + 1e-6, DRAPE_SMOOTH * jump ** 2, np.inf)
        pinned = np.where(jump == 0.0, 0.0, np.inf)
        turned = np.abs(steps)
        cost, back = None, []
        for f in range(n + 1):
            z = hips[f][:, None, None] + part[f][0][:, :, None] + part[f][1][:, None, :]
            here = turned[:, None] + turned[None, :] + DRAPE_DEEP * (np.maximum(0.0, floors[f] - z.min(axis=0)) +
                                                                     np.maximum(0.0, z.max(axis=0) - ceilings[f]))
            if cost is None:  # frame 0 is the rest pose: no further turn
                cost = np.full((S, S), np.inf)
                cost[S // 2, S // 2] = here[S // 2, S // 2]
                continue
            # The cheapest way into each pair of turns: from the best front turn, then the best back turn.
            step = pinned if f > down else change
            via_f = cost[:, None, :] + step[:, :, None]  # [front before, front now, back before]
            from_f = via_f.argmin(axis=0)
            via_b = via_f.min(axis=0)[:, :, None] + step[None, :, :]  # [front now, back before, back now]
            from_b = via_b.argmin(axis=1)
            cost = via_b.min(axis=1) + here
            back.append((from_f, from_b))
        i, j = np.unravel_index(int(cost.argmin()), cost.shape)
        path = [(i, j)]
        for from_f, from_b in reversed(back):
            j_before = from_b[i, j]
            i, j = from_f[i, j_before], j_before
            path.append((i, j))
        path.reverse()
        turns = {b: [float(steps[ij[k]]) for ij in path] for k, b in enumerate(SKIRT)}
    curves = {(fc.data_path, fc.array_index): fc for fc in _fcurves(action)}
    for b in SKIRT:
        path = 'pose.bones["%s"].rotation_euler' % b
        prev = None
        for f in range(n + 1):
            basis = Euler([curves[(path, k)].evaluate(f) for k in range(3)], "XYZ").to_matrix().to_4x4() @ swing(b, turns[b][f])
            e = basis.to_euler("XYZ", prev) if prev else basis.to_euler("XYZ")
            prev = e
            for k in range(3):
                for key in curves[(path, k)].keyframe_points:
                    if int(round(key.co.x)) == f:
                        key.co.y = e[k]
                        key.handle_left.y = key.handle_right.y = e[k]
        for k in range(3):
            curves[(path, k)].update()
    return turns


def floor(arm, body, action, buckle, down, n):
    """The floor pass for a fall: from `buckle` on (once the feet may move) the hips are raised by however far anything
    of the figure but its robe (its shield or quiver as well; the robe goes into the ground, drape) would go into the
    ground, eased over the frames (never less); from BOUNCE frames after he lands (`down`) on they are also lowered, so
    its lowest point lies FLOOR above the ground. Per figure, as shields and quivers differ. Returns the lift, per
    frame."""
    play(arm, action)
    scene = bpy.context.scene
    need = [0.0] * (n + 1)
    settled = down + BOUNCE
    body_only = ~_cloth(body)
    for f in range(buckle, n + 1):
        scene.frame_set(f)
        low = _lowest(body, body_only)
        need[f] = FLOOR - low if f >= settled else max(0.0, -low)
    lift = list(need)
    for f in range(buckle, settled):  # eased: the upper envelope of parabolas (EASE), never below the need
        lift[f] = max([need[f]] + [need[g] - EASE * (f - g) ** 2 for g in range(buckle, settled)])
    curves = {fc.array_index: fc for fc in _fcurves(action) if fc.data_path == 'pose.bones["pelvis"].location'}
    fc = curves[1]  # the hips' bone points up: its Y is the armature's Z
    for k in fc.keyframe_points:
        f = int(round(k.co.x))
        if 0 <= f <= n and lift[f]:
            k.co.y += lift[f]
            k.handle_left.y += lift[f]
            k.handle_right.y += lift[f]
    fc.update()
    return lift


def _fall(poser, arm, body, take, keys, key_skirt):
    """Keys a fall (Death or DeathBack) from `keys`: twice, the second time with the first floor pass's lift in the
    hips' pose (and the first drape's turns), so hands it puts on the ground (world targets) stay there while a quiver
    or a cloak lifts the body, and a second floor pass takes up what is left. Returns the action and how high its hips
    end (cm)."""
    spec = ONCE[take]
    n = spec["frames"]
    action = lift = turns = None
    for _ in range(2):
        if action:
            bpy.data.actions.remove(action)
        action = poser.key(take, n, keys, None, lift, guard=True)
        key_skirt(arm, action, cycles=False)
        key_rest(arm, action)
        turns = drape(arm, body, action, spec["buckle"], spec["down"], n, turns)
        lift = floor(arm, body, action, spec["buckle"], spec["down"], n)
    bpy.context.scene.frame_set(n)
    return action, arm.pose.bones["pelvis"].head.z


def add(arm, body, cls, items, held, key_skirt):
    """Keys the one-shots (ONCE) for a figure that strikes as `cls` (strike_class), what its hands hold `items`
    (item_axes per side): each keyed as above, its skirt from its legs (`key_skirt`, rig_figures.key_skirt, not
    looping), every other bone at rest (key_rest), the falls' robes kept out of the ground (drape) and the falls laid
    on the ground (floor, _fall), finished as one-shots. Returns the actions by take."""
    poser = Poser(arm, items)
    poser.skin = Skin(arm, body, held)
    out = {}
    for take, keys in (("Strike", strike(poser, cls)), ("Hit", hit(poser))):
        for _, _, pose in keys:
            _neck(poser, pose)
        out[take] = poser.key(take, ONCE[take]["frames"], keys, UPPER)
        key_skirt(arm, out[take], cycles=False)
        key_rest(arm, out[take])
    out["Death"] = _fall(poser, arm, body, "Death", death(poser), key_skirt)[0]
    # On his back he rolls onto whichever side lets him lie lower (a sword or a quiver at one hip would prop him up).
    falls = sorted((_fall(poser, arm, body, "DeathBack", death_back(poser, roll), key_skirt) for roll in (ROLL, -ROLL)),
                   key=lambda fall: fall[1])
    bpy.data.actions.remove(falls[1][0])
    falls[0][0].name = "DeathBack"
    out["DeathBack"] = falls[0][0]
    for take, action in out.items():
        finish_once(action, ONCE[take]["frames"])
    return out


def line():
    """The line rig_figures prints after it exports (tools/art/README.md, One-shot actions)."""
    parts = []
    for take, spec in ONCE.items():
        if "impact" in spec:
            parts.append("%s %d fr (blow at %d, %.3f s)" % (take, spec["frames"], spec["impact"], spec["impact"] / float(FPS)))
        else:
            parts.append("%s %d fr" % (take, spec["frames"]))
    return "rig_figures: one-shot actions " + ", ".join(parts)
