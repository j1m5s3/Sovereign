"""Motion capture for the rigged people (rig_figures.py): WalkSlow, Walk, Run and Idle loops from the CMU Graphics Lab
Motion Capture Database (the clips are in tools/art/mocap/, its README names them and the credit), retargeted onto
the rig's skeleton (rig_figures.BONES; its skirt bones are keyed there, from the legs). Pure Python over mathutils:
the BVH is read and posed here, not by Blender's importer.

How a clip becomes an action:
1. The loop's frames (CLIPS) are sampled at the rig's frame rate. Every joint is turned so the subject walks (or
   faces) -Y, scaled from the subject's leg length to the rig's and stood on the ground (a planted ankle at the
   rig's ankle height).
2. Root motion comes out: the body is carried back at the loop's mean speed, which is recorded (`Motion.speed`, cm/s)
   so the game plays the clip at its own speed / that. The hips keep their surge, sway and bob around it.
3. The loop is closed: whatever differs between its last frame and its first is taken out evenly across it. A gait's
   is then turned round to start as the subject's left foot plants (CLIPS), so every gait's frame 0 is the same moment
   of the stride.
4. The trunk turns as the subject's does from standing (an idle's mean lean taken out, and its feet set under its
   hips on average, so it stands upright); arms and legs point where the subject's do. Legs are solved
   by two-bone IK to ankle targets; while a foot is planted its target holds still in the world (in place, it moves
   back at the recorded speed) and its foot lies flat, blended in and out over a few frames. The hips come down
   where a leg cannot reach (and, standing, rise until the legs are straight), and an ankle comes up where the shoe
   or shin would go into the ground.
5. A figure with something in a hand (spear, shield, staff, musket, basket: rig_figures.whole_parts finds it) keeps
   that arm near its modelled carry pose, so what it holds does not flail, and swings it back at the shoulder where a
   spear's butt would hit the ground: walking or running at one slope for the whole stride; standing, only where it
   would go into the ground more than a little (a staff may rest on it).

check_rigs.py measures the result (planted-foot slide, feet in the ground, what the hands hold, seams).
"""
import math
import os

from mathutils import Matrix, Vector

MOCAP = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mocap"))

# action: (file in tools/art/mocap/, first and last frame of the loop in that file, kind). "gait" clips travel and
# loop over one full stride (two steps); "stand" clips stay put with both feet planted. Every gait loop starts as the
# left foot plants: wherever its file's loop is cut, the closed loop is turned round so the rig's frame 0 is the first
# frame the subject's left foot (LEAD) is planted, as found below and locked. So the gaits are in step at the same
# phase and the game can blend two of them there.
CLIPS = {
    "WalkSlow": ("walkslow_16_33.bvh", 12, 149, "gait"),
    "Walk": ("walk_39_01.bvh", 12, 136, "gait"),
    "Run": ("run_35_18.bvh", 12, 105, "gait"),
    "Idle": ("idle_82_08.bvh", 0, 208, "stand"),
}
# How far each arm stays toward its modelled pose (0 = the subject's swing, 1 = still): the arm holding something,
# the other arm of a figure that holds something, everyone else.
CALM_CARRY, CALM_OTHER, CALM_FREE = 0.95, 0.4, 0.0
LOCK_BLEND = 2      # frames to blend a planted foot's lock in and out
LOCK_SPEED = 0.25   # a foot is planted while it moves slower than this share of the loop's speed (gait)
LOCK_FLOOR = 6.0    # cm: ... and its ankle is no higher than this above its planted height
LEAD = "r"          # the foot every gait loop starts on as it plants: the subject's left (the rig's +X side, SIDES)
CLEAR = 0.3         # cm: a planted shoe's lowest vertex this far above the ground
SWING_CLEAR = 1.5   # cm: and a moving one's this much further
REACH = 0.998       # the share of a leg's length its ankle may be from its hip (straighter snaps the knee)
ITEM_CLEAR = 2.0    # cm: what a hand carries (a spear's butt, a staff) kept this far above the ground walking
ITEM_REST = -3.0    # cm: ... and standing, how far it may stand in it (a staff's foot on the ground, not a twitch)


def available():
    return all(os.path.exists(os.path.join(MOCAP, c[0])) for c in CLIPS.values())


# BVH ---------------------------------------------------------------------------------------------------------------

# BVH is Y up, the subject facing +Z; the rig is Z up facing -Y. A quarter turn about X maps one onto the other.
_TO_RIG = Matrix(((1, 0, 0), (0, 0, -1), (0, 1, 0)))


class Bvh:
    """A BVH file: its joints (names, parents, offsets, channels, end sites) and its frames."""

    def __init__(self, path):
        with open(path) as f:
            tok = f.read().split()
        self.names, self.parents, self.offsets, self.channels, self.ends = [], [], [], [], {}
        i, stack, current = 0, [], None
        while tok[i] != "MOTION":
            t = tok[i]
            if t in ("ROOT", "JOINT"):
                self.names.append(tok[i + 1])
                self.parents.append(stack[-1] if stack else -1)
                self.offsets.append(Vector())
                self.channels.append([])
                current = len(self.names) - 1
                i += 2
            elif t == "End":
                current = ("end", stack[-1])
                i += 2
            elif t == "{":
                stack.append(current)
                i += 1
            elif t == "}":
                stack.pop()
                i += 1
            elif t == "OFFSET":
                v = Vector([float(x) for x in tok[i + 1:i + 4]])
                if isinstance(current, tuple):
                    self.ends[current[1]] = v
                else:
                    self.offsets[current] = v
                i += 4
            elif t == "CHANNELS":
                n = int(tok[i + 1])
                self.channels[current] = tok[i + 2:i + 2 + n]
                i += 2 + n
            else:
                i += 1
        n_frames = int(tok[i + 2])
        self.frame_time = float(tok[i + 5])
        values = [float(x) for x in tok[i + 6:]]
        width = sum(len(c) for c in self.channels)
        self.frames = [values[f * width:(f + 1) * width] for f in range(n_frames)]
        self.index = {n: j for j, n in enumerate(self.names)}

    @property
    def fps(self):
        return 1.0 / self.frame_time

    def pose(self, frame):
        """Every joint's position (rig axes, the file's units) at a frame; end sites as <joint>_end. Between two
        frames the positions are interpolated."""
        f0 = max(0, min(len(self.frames) - 1, int(math.floor(frame))))
        f1 = min(len(self.frames) - 1, f0 + 1)
        a = self._pose(f0)
        u = frame - f0
        if u <= 1e-9 or f1 == f0:
            return a
        b = self._pose(f1)
        return {k: a[k].lerp(b[k], u) for k in a}

    def _pose(self, frame):
        values = self.frames[frame]
        mats, out, c = [], {}, 0
        for j, name in enumerate(self.names):
            loc = self.offsets[j].copy()
            rot = Matrix.Identity(3)
            for ch in self.channels[j]:
                v = values[c]
                c += 1
                if ch.endswith("position"):
                    loc["XYZ".index(ch[0])] += v
                else:
                    rot = rot @ Matrix.Rotation(math.radians(v), 3, ch[0])
            m = Matrix.Translation(loc) @ rot.to_4x4()
            if self.parents[j] >= 0:
                m = mats[self.parents[j]] @ m
            mats.append(m)
            out[name] = _TO_RIG @ m.translation
            if j in self.ends:
                out[name + "_end"] = _TO_RIG @ (m @ self.ends[j])
        return out

    def rest(self):
        """The joints' positions with every channel at zero (the subject standing, as the file's offsets build it)."""
        saved = self.frames
        self.frames = [[0.0] * len(saved[0])]
        try:
            return self._pose(0)
        finally:
            self.frames = saved

    def leg_length(self):
        """Hip to ankle along the bones, both legs averaged."""
        return sum((self.offsets[self.index[side + "Leg"]].length + self.offsets[self.index[side + "Foot"]].length)
                   for side in ("Left", "Right")) / 2.0


# Clip analysis (the same for every figure) -------------------------------------------------------------------------

def _frame(y, x_ref):
    """An orthonormal frame (as a 3x3 matrix of columns x, y, z) with y along `y` and x as near `x_ref` as it can be."""
    y = y.normalized()
    x = x_ref - y * x_ref.dot(y)
    if x.length < 1e-6:
        x = Vector((1, 0, 0)) - y * y.x
    x.normalize()
    return Matrix((x, y, x.cross(y))).transposed()


# The subject's joints for each of ours. The rig's +X side ("_r") is the subject's left once it faces -Y.
SIDES = {"r": "Left", "l": "Right"}


def _mid_hip(j):
    return (j["LeftUpLeg"] + j["RightUpLeg"]) * 0.5


def _trunk(j):
    """The subject's pelvis, chest and head as (up, lateral) pairs: lateral runs from the rig's -X side to its +X."""
    hips = j["LeftUpLeg"] - j["RightUpLeg"]
    shoulders = j["LeftArm"] - j["RightArm"]
    return {"pelvis": (j["Spine"] - _mid_hip(j), hips), "spine": (j["Neck"] - j["Spine"], shoulders),
            "head": (j["Head_end"] - j["Neck1"], shoulders)}


class Motion:
    """A clip's loop sampled at `fps` for a rig whose legs are `leg` cm long with the ankle `ankle` cm up when the
    foot is flat: per frame, the subject's joints in the rig's in-place space (cm), and per foot when it is planted."""

    def __init__(self, action, fps, leg, ankle):
        path, first, last, kind = CLIPS[action]
        bvh = Bvh(os.path.join(MOCAP, path))
        self.action, self.kind = action, kind
        last = last or len(bvh.frames) - 1
        src_fps = bvh.fps
        duration = (last - first) / src_fps
        self.frames = n = max(8, int(round(duration * fps)))
        self.fps = fps
        k = leg / bvh.leg_length()  # cm per file unit
        # Every source frame of the loop (for contacts) and the rig's frames (for the action).
        dense = [bvh.pose(f) for f in range(first, last + 1)]
        sampled = [bvh.pose(first + (last - first) * f / n) for f in range(n + 1)]
        self.rest = {name: k * p for name, p in bvh.rest().items()}

        # Heading: the hips' travel over the loop (gait), or the way the pelvis faces on average (stand).
        if kind == "gait":
            travel = _mid_hip(dense[-1]) - _mid_hip(dense[0])
            travel.z = 0.0
            heading = travel.normalized()
        else:
            lat = sum((j["LeftUpLeg"] - j["RightUpLeg"] for j in dense), Vector())
            lat.z = 0.0
            heading = lat.normalized().cross(Vector((0, 0, 1)))  # facing = lateral x up
            travel = Vector()
        turn = Matrix.Rotation(-math.atan2(heading.x, -heading.y), 3, "Z")  # heading -> -Y
        self.speed = k * travel.length / (n / fps)  # cm/s once the loop plays in n frames at fps
        back = Vector((0, 1, 0))  # where a world-still point drifts in place: the body walks to -Y

        # Ground: the ankles' height when planted (the low eighth of their heights), so a planted ankle sits at `ankle`.
        lows = sorted(min(j["LeftFoot"].z, j["RightFoot"].z) for j in dense)
        ground = lows[len(lows) // 8] - ankle / k
        origin = _mid_hip(dense[0])
        origin = Vector((origin.x, origin.y, ground))

        def place(j, t):
            """A pose in the rig's in-place space at t seconds into the loop."""
            return {name: turn @ ((p - origin) * k) + back * self.speed * t for name, p in j.items()}

        self.joints = [place(j, f / fps) for f, j in enumerate(sampled)]
        # Close the loop: each joint's mismatch between the last frame and the first, taken out evenly.
        end, start = self.joints[n], self.joints[0]
        for f, j in enumerate(self.joints):
            for name in j:
                j[name] = j[name] - (end[name] - start[name]) * (f / n)
        # Centre the hips over the origin (in place, on average).
        mean = sum((_mid_hip(j) for j in self.joints[:n]), Vector()) / n
        shift = Vector((mean.x, mean.y, 0.0))
        for j in self.joints:
            for name in j:
                j[name] = j[name] - shift
        if kind == "stand":
            # Standing, the feet under the hips on average (fore and aft): CMU's standing subjects often hold their
            # hips forward of their ankles, which reads as leaning back. Their weight shifts stay.
            ahead = sum(((j["LeftFoot"] + j["RightFoot"]) * 0.5 - _mid_hip(j) for j in self.joints[:n]), Vector()) / n
            fix = Vector((0.0, -ahead.y, 0.0))
            for j in self.joints:
                for name in j:
                    if name.startswith(("LeftFoot", "RightFoot", "LeftToe", "RightToe")):
                        j[name] = j[name] + fix

        # Planted feet, found at the source's own rate: slow in the world and near the ground.
        self.planted = {}
        for s, src in SIDES.items():
            ankle_name = src + "Foot"
            pts = [turn @ ((j[ankle_name] - origin) * k) for j in dense]
            m = len(pts)
            if kind == "stand":
                flags = [True] * n
            else:
                z0 = sorted(p.z for p in pts)[m // 8]
                speed = [(pts[min(i + 1, m - 1)] - pts[max(i - 1, 0)]).length * src_fps / (min(i + 1, m - 1) - max(i - 1, 0))
                         for i in range(m)]
                down = [speed[i] < LOCK_SPEED * self.speed and pts[i].z < z0 + LOCK_FLOOR for i in range(m)]
                # One flag per rig frame: planted when the source is planted at that moment.
                flags = [down[min(m - 1, int(round(f * (m - 1) / n)))] for f in range(n)]
            self.planted[s] = flags

        # A gait starts as the left foot plants: the loop (and its planted flags) turned round so frame 0 is the first
        # frame of LEAD's longest planted stretch. It is closed, so it stays seamless; the last frame is the first again.
        stances = self.windows(LEAD) if kind == "gait" else []
        if stances:
            a = max(stances, key=lambda w: w[1] - w[0])[0] % n
            self.joints = self.joints[a:n] + self.joints[:a]
            self.joints.append({name: p.copy() for name, p in self.joints[0].items()})
            for s in self.planted:
                self.planted[s] = self.planted[s][a:] + self.planted[s][:a]

    def windows(self, side):
        """The planted stretches of one foot as (first, last) rig frames; a stretch over the loop's seam runs past
        `frames` (or before 0)."""
        flags, n = self.planted[side], self.frames
        if all(flags):
            return [(0, n - 1)]
        out = []
        start = next(f for f in range(n) if not flags[f])  # begin from a lifted frame so no stretch is cut
        f = start + 1
        while f <= start + n:
            if flags[f % n]:
                a = f
                while flags[f % n] and f < start + n + n:
                    f += 1
                out.append((a, f - 1))
            f += 1
        return out


_MOTIONS = {}


def motion(action, fps, leg, ankle):
    key = (action, fps, round(leg, 3), round(ankle, 3))
    if key not in _MOTIONS:
        _MOTIONS[key] = Motion(action, fps, leg, ankle)
    return _MOTIONS[key]


# Retargeting onto one figure --------------------------------------------------------------------------------------

def _two_bone(hip, target, pole, l1, l2):
    """Knee and ankle for a thigh of l1 and a shin of l2 from `hip` toward `target`, the knee bending toward `pole`."""
    d = target - hip
    dist = min(max(d.length, abs(l1 - l2) + 0.01), (l1 + l2) * 0.9999)
    u = d.normalized()
    p = pole - u * pole.dot(u)
    if p.length < 1e-6:
        p = Vector((0, -1, 0)) - u * (-u.y)
    p.normalize()
    cos_a = max(-1.0, min(1.0, (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist)))
    a = math.acos(cos_a)
    knee = hip + (u * math.cos(a) + p * math.sin(a)) * l1
    return knee, hip + u * dist, p.cross(u)


def _off_segment(p, a, b):
    """How far p is from the segment a-b."""
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return (p - (a + ab * t)).length


class Retarget:
    """Poses one rigged figure (armature `arm`, skinned mesh `body`, centimetres) from Motion frames. `held`: the
    vertices of what each hand holds ({"r" or "l": indices}, rig_figures.whole_parts)."""

    def __init__(self, arm, body, held):
        self.arm = arm
        B = arm.data.bones
        self.rest = {b.name: b.matrix_local.copy() for b in B}
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in B}
        self.order = [b.name for b in B]  # parents before children, as the armature was built
        self.dir = {b.name: (b.tail_local - b.head_local) for b in B}
        self.l1 = B["thigh_r"].length
        self.l2 = B["shin_r"].length
        self.ankle = B["shin_r"].tail_local.z
        self.leg = self.l1 + self.l2
        # What can touch the ground, each in its bone's space: the shoes and the shins' lowest vertices (to keep them
        # out of it), and what the hands carry (to keep a spear's butt off it).
        groups = {g.index: g.name for g in body.vertex_groups}
        owner = {}
        for v in body.data.vertices:
            g = max(v.groups, key=lambda g: g.weight, default=None)
            owner[v.index] = groups[g.group] if g else ""
        self.low = {}
        for s in ("r", "l"):
            for bone in ("foot_" + s, "shin_" + s):
                inv = self.rest[bone].inverted()
                pts = sorted((v.co for v in body.data.vertices if owner[v.index] == bone), key=lambda c: c.z)[:300]
                self.low[bone] = [inv @ c for c in pts]
        # What a hand carries (a spear, staff, shield, bow, musket or basket), its lowest points in the forearm's space:
        # that arm is calmed, and swung back where it would meet the ground.
        self.carry = {}
        for s in ("r", "l"):
            inv = self.rest["forearm_" + s].inverted()
            pts = sorted((body.data.vertices[i].co for i in held.get(s, ())), key=lambda c: c.z)[:200]
            self.carry[s] = [inv @ c for c in pts]

    def _child_head(self, M, parent, child):
        """Where a child bone's head is when its parent's pose matrix is M."""
        return M @ (self.rest[parent].inverted() @ self.rest[child].translation)

    def _rot(self, bone, frame_t, frame_rest):
        """A bone's armature-space rotation that turns its rest frame `frame_rest` onto `frame_t`."""
        return frame_t @ frame_rest.transposed() @ self.rest[bone].to_3x3()

    @staticmethod
    def _lowest(points, M):
        return min((M @ c).z for c in points) if points else 1e9

    def key(self, action_name, mo):
        """Key `action_name` on the armature from Motion `mo`; returns the action."""
        import bpy

        arm, n = self.arm, mo.frames
        P = arm.pose.bones
        for b in P:
            b.rotation_mode = "XYZ"
        action = bpy.data.actions.new(action_name)
        arm.animation_data.action = action
        X = Vector((1, 0, 0))
        rest_src = _trunk(mo.rest)
        rest_frames = {b: _frame(*rest_src[b]) for b in rest_src}
        armed = bool(self.carry["r"] or self.carry["l"])
        calm = {s: CALM_CARRY if self.carry[s] else (CALM_OTHER if armed else CALM_FREE) for s in ("r", "l")}

        def foot_frame(j, src):
            # Rolled by the hips' lateral, never along the toe (the horizontal across a toe pointing down flips).
            return _frame(j[src + "ToeBase"] - j[src + "Foot"], j["LeftUpLeg"] - j["RightUpLeg"])

        src_foot_rest = {s: foot_frame(mo.rest, src) for s, src in SIDES.items()}

        # The trunk turns as the subject's does from standing; the pelvis sits where the subject's hips are (Motion's
        # heights are above the ground already).
        poses = []
        for f in range(n):
            j = mo.joints[f]
            tr = _trunk(j)
            D = {b: _frame(*tr[b]) @ rest_frames[b].transposed() for b in tr}
            for s, src in SIDES.items():
                D["foot_" + s] = foot_frame(j, src) @ src_foot_rest[s].transposed()
            poses.append({"j": j, "D": D})
        if mo.kind == "stand":
            # Standing upright on average: the subject's mean lean of the hips, back and head (back, with the hips held
            # forward) comes out; the sway, breathing and glances around stay.
            for b in ("pelvis", "spine", "head"):
                lean = sum(_pitch(pose["D"][b]) for pose in poses) / n
                fix = Matrix.Rotation(-lean, 3, "X")
                for pose in poses:
                    pose["D"][b] = fix @ pose["D"][b]
        for pose in poses:
            pose["M"] = {"pelvis": Matrix.Translation(_mid_hip(pose["j"])) @ (pose["D"]["pelvis"] @ self.rest["pelvis"].to_3x3()).to_4x4()}

        # Ankle targets: the subject's ankle from its own hip, hung from ours; the knee toward the subject's.
        for pose in poses:
            j, Mp = pose["j"], pose["M"]["pelvis"]
            pose["hip"], pose["target"], pose["pole"] = {}, {}, {}
            ahead = (j["LeftUpLeg"] - j["RightUpLeg"]).cross(Vector((0, 0, 1))).normalized()  # the hips' forward
            for s, src in SIDES.items():
                hip = self._child_head(Mp, "pelvis", "thigh_" + s)
                pose["hip"][s] = hip
                pose["target"][s] = hip + (j[src + "Foot"] - j[src + "UpLeg"])
                toe = j[src + "ToeBase"] - j[src + "Foot"]
                knee = j[src + "Leg"] - (j[src + "UpLeg"] + j[src + "Foot"]) * 0.5
                # Toward the toe by its horizontal share, fading out as it points down (where its horizontal swings
                # round and would roll the leg), and a little forward so a straight leg's pole never vanishes.
                pose["pole"][s] = knee + Vector((toe.x, toe.y, 0.0)) * (8.0 / max(toe.length, 1e-6)) + ahead * 2.0

        # Planted feet hold still in the world (in place, they drift back at the loop's speed), flat on the ground
        # and turned as the subject's on average over the stance.
        back = Vector((0, 1, 0)) * (mo.speed / mo.fps)
        for s in ("r", "l"):
            weight = [0.0] * n
            locked = [None] * n
            for a, b in mo.windows(s):
                frames = list(range(a, b + 1))
                q = sum(((poses[f % n]["target"][s] - back * (f - a)) for f in frames), Vector()) / len(frames)
                q.z = self.ankle
                turn = sum((_yaw(poses[f % n]["D"]["foot_" + s]) for f in frames)) / len(frames)
                flat = Matrix.Rotation(turn, 3, "Z").to_quaternion()
                for f in range(a - LOCK_BLEND, b + LOCK_BLEND + 1):
                    w = 1.0 if a <= f <= b else 1.0 - (min(abs(f - a), abs(f - b))) / (LOCK_BLEND + 1.0)
                    w = w * w * (3 - 2 * w)
                    i = f % n
                    if w > weight[i]:
                        weight[i] = w
                        locked[i] = (q + back * (f - a), flat)
            for f in range(n):
                poses[f].setdefault("lock", {})[s] = weight[f]
                if weight[f] > 0.0:
                    pose = poses[f]
                    pose["target"][s] = pose["target"][s].lerp(locked[f][0], weight[f])
                    q = pose["D"]["foot_" + s].to_quaternion()
                    pose["D"]["foot_" + s] = q.slerp(locked[f][1], weight[f]).to_matrix()

        def slack(pose):
            """How far the hips could rise before a leg would be straight (negative: how far they must come down)."""
            out = 1e9
            for s in ("r", "l"):
                v = pose["target"][s] - pose["hip"][s]
                out = min(out, math.sqrt(max((self.leg * REACH) ** 2 - v.x * v.x - v.y * v.y, 0.0)) + v.z)
            return out

        def lift(pose, dz):
            pose["M"]["pelvis"] = Matrix.Translation((0, 0, dz)) @ pose["M"]["pelvis"]
            for s in ("r", "l"):
                pose["hip"][s] = pose["hip"][s] + Vector((0, 0, dz))

        if mo.kind == "stand":
            # Standing, the legs nearly straight: on CMU's skeleton a standing subject's knees read 15-30 degrees
            # bent (the hip-ankle distance a percent or two short of the bones), which looks like a crouch here.
            up = min(slack(pose) for pose in poses)
            if up > 0.0:
                for pose in poses:
                    lift(pose, up)
        # The hips come down where a leg cannot reach its target (smoothly: an upper envelope of parabolas).
        need = [max(0.0, -slack(pose)) for pose in poses]
        drop = _envelope(need, 0.4)  # cm per frame squared
        for f, pose in enumerate(poses):
            if drop[f] > 0.0:
                lift(pose, -drop[f])

        # Legs: two-bone IK to the ankle target, the foot turned as the subject's (or flat while planted); then the
        # ankle comes up as far as the shoe or shin would go into the ground.
        rest_leg = {s: (_frame(self.dir["thigh_" + s], X), _frame(self.dir["shin_" + s], X)) for s in ("r", "l")}
        for pose in poses:
            M, D = pose["M"], pose["D"]
            for s in ("r", "l"):
                target = pose["target"][s].copy()
                rf = (D["foot_" + s] @ self.rest["foot_" + s].to_3x3()).to_4x4()
                clear = CLEAR + SWING_CLEAR * (1.0 - pose["lock"][s])  # a moving foot does not scrape the ground
                for _ in range(3):
                    knee, ankle, lat = _two_bone(pose["hip"][s], target, pose["pole"][s], self.l1, self.l2)
                    rt = self._rot("thigh_" + s, _frame(knee - pose["hip"][s], lat), rest_leg[s][0])
                    rs = self._rot("shin_" + s, _frame(ankle - knee, lat), rest_leg[s][1])
                    Ms = Matrix.Translation(knee) @ rs.to_4x4()
                    Mf = Matrix.Translation(ankle) @ rf
                    low = min(self._lowest(self.low["foot_" + s], Mf), self._lowest(self.low["shin_" + s], Ms))
                    if low >= clear - 0.05:
                        break
                    target.z += clear - low
                M["thigh_" + s] = Matrix.Translation(pose["hip"][s]) @ rt.to_4x4()
                M["shin_" + s] = Ms
                M["foot_" + s] = Mf
            M["spine"] = Matrix.Translation(self._child_head(M["pelvis"], "pelvis", "spine")) @ (D["spine"] @ self.rest["spine"].to_3x3()).to_4x4()
            M["head"] = Matrix.Translation(self._child_head(M["spine"], "spine", "head")) @ (D["head"] @ self.rest["head"].to_3x3()).to_4x4()

        # Arms: where the subject's point, kept out of the body and toward the modelled pose by `calm`; a hand that
        # carries something swings back at the shoulder as far as keeps that off the ground (smoothly over the frames).
        rest_arm = {s: (_frame(self.dir["upperarm_" + s], X), _frame(self.dir["forearm_" + s], X)) for s in ("r", "l")}
        home_of = {b: self.rest["spine"].to_3x3().transposed() @ self.dir[b].normalized() for b in self.dir}

        def arm_pose(pose, s, bend):
            j, M = pose["j"], pose["M"]
            src = SIDES[s]
            chest = M["spine"].to_3x3()
            sign = 1.0 if s == "r" else -1.0
            out, parent = {}, M["spine"]
            parent_name = "spine"
            for bone, a, b in (("upperarm_" + s, src + "Arm", src + "ForeArm"), ("forearm_" + s, src + "ForeArm", src + "Hand")):
                want = chest.transposed() @ (j[b] - j[a]).normalized()  # in the chest's frame: x lateral, y up, z forward
                home = home_of[bone]
                margin = 0.0 if bone.startswith("upper") else 0.06
                if sign * want.x < sign * home.x - margin:  # not into the body
                    want.x = home.x - sign * margin
                    want.normalize()
                want = want.lerp(home, calm[s]).normalized()
                if bend:
                    want = Matrix.Rotation(math.radians(bend), 3, "X") @ want  # the whole arm back, the hand up
                head = self._child_head(parent, parent_name, bone)
                rot = self._rot(bone, _frame(chest @ want, chest.col[0]), rest_arm[s][0 if bone.startswith("upper") else 1])
                out[bone] = parent = Matrix.Translation(head) @ rot.to_4x4()
                parent_name = bone
            return out

        clear = ITEM_CLEAR if mo.kind == "gait" else ITEM_REST
        for s in ("r", "l"):
            bends = [0.0] * n
            if self.carry[s]:
                for f, pose in enumerate(poses):
                    for bend in range(0, 46, 3):
                        if self._lowest(self.carry[s], arm_pose(pose, s, bend)["forearm_" + s]) >= clear:
                            break
                    bends[f] = bend
                # Walking or running, held at one slope the whole stride (a spear that pumps back and forth flails);
                # standing, only while it needs to be.
                bends = [max(bends)] * n if mo.kind == "gait" else _envelope(bends, 0.5)  # degrees per frame squared
            for f, pose in enumerate(poses):
                pose["M"].update(arm_pose(pose, s, bends[f]))

        # Key the pose bones' local transforms (the root stays at rest: no root motion; rig_figures keys the skirt).
        prev = {}
        for f in range(n + 1):
            M = poses[f % n]["M"]
            for name in self.order:
                if name not in M:
                    continue
                par = self.parent[name]
                Mp = M[par] if par in M else self.rest[par]
                basis = self.rest[name].inverted() @ self.rest[par] @ Mp.inverted() @ M[name]
                loc, rot, _ = basis.decompose()
                pb = P[name]
                e = rot.to_euler("XYZ", prev[name]) if name in prev else rot.to_euler("XYZ")
                prev[name] = e
                pb.rotation_euler = e
                pb.keyframe_insert("rotation_euler", frame=f)
                if name == "pelvis":
                    pb.location = loc
                    pb.keyframe_insert("location", frame=f)
        return action


def _yaw(R):
    """How far a rotation turns the rig's forward (-Y) about Z, in radians."""
    v = R @ Vector((0, -1, 0))
    return math.atan2(v.x, -v.y) if abs(v.x) + abs(v.y) > 1e-6 else 0.0


def _pitch(R):
    """How far a rotation leans the rig's up (+Z) forward (toward -Y), in radians."""
    v = R @ Vector((0, 0, 1))
    return math.atan2(-v.y, v.z)


def _envelope(values, ease):
    """The lowest curve at or above every value made of parabolas `ease * frames^2` deep, over a looping list: a
    smooth stand-in for the values that never falls short of them."""
    n = len(values)
    return [max(values[g] - ease * min(abs(f - g), n - abs(f - g)) ** 2 for g in range(n)) for f in range(n)]
