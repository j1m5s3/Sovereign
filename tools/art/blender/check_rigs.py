"""Checks the rigged people (rig_figures.py): rigs each figure (all of them, or those named) and fails when one is
broken, so a bad skin or a sliding foot never reaches Unreal. rig_figures.py runs the same check on each figure
before it exports it. A horseman named (a name beginning "Rider": Rider, Rider_Asian...) is rigged and checked by
rig_riders.py's own check instead, as rig_riders.py does before it exports one; no names checks the people only.

  blender -b --factory-startup --python-exit-code 1 --python tools/art/blender/check_rigs.py [-- Townsman Soldier Rider ...]
  python -I tools/art/blender/check_rigs.py [Townsman Rider_Asian ...]      (pip's bpy)

Per figure:
- every limb bone carries vertices (a skin bound at the wrong scale leaves the legs on nothing);
- in each looping action, no planted foot slides more than SLIDE cm: a vertex of a foot that stays on the ground
  (within 1 cm) for 3 frames or more must hold still in the world while the body moves on at the action's speed;
- no foot vertex goes more than SINK cm into the ground;
- what a hand holds (rig_figures.HELD: a spear, shield, musket...) does not flail: against the chest, none of its
  vertices moves over more than SWING cm in a loop (a steady slope is fine; a swing with the arm is not);
- each gait (an action with a speed) starts as the left foot plants (mocap.CLIPS), so the game can blend two at one
  phase: the first frame of the longest stretch over which that shoe holds still on the ground ("plant0", frames
  from frame 0, negative before it at the loop's end) is within PLANT_AT frames of 0;
- what a hand holds rides that hand's bone (hand_r, hand_l), and every action keys every bone, and the place of the
  hips and hands (a bone with no curve would carry another action's pose into this one in the FBX bake);
- and, reported only: how far the seams between two bones open ("seam": the edge between two bones' parts
  that stretches most beyond its length at rest, in cm, such as a knee or hip in a long stride), how far the cloth
  the skirt bones swing stretches ("cloth": a robe flares as its front and back part, by design), and the lowest
  point of anything else ("other": a robe's hem, a spear's butt). SOV_CHECK_DEBUG=1 names the vertices behind the
  worst seam, slide and lowest point.
A figure that fights has its one-shots checked too (check_once, rig_actions.ONCE: one lap, its last frame included, on
its own line "check <Name> once (<class>): ..."): in Strike and Hit no foot vertex moves (the game plays a Strike over
the walk's legs and adds a Hit on top of whatever plays) and the last frame is the first, the rest pose; in Death and
DeathBack the feet hold still on the ground until the knees buckle, the figure lies still from the frame ONCE says
(and in DeathBack its hips' drop never slows by more than SLOW in a frame before it lands on its seat), and it ends on
the ground, its head ahead of its hips (Death) or behind them (DeathBack), its hips and chest, its legs and what its
hands hold each within PART of it (not propped up on a hat or a shield's rim); in every take nothing goes more than
SINK into the ground (but a robe in a fall once the knees give: rig_actions.drape lets the ground hide it),
nothing moves more than JUMP and no bone turns more than TURN in a frame (a Strike's blow aside), no robe rises more
than ROBE above the hips in a fall, and every bone is keyed; at the Strike's blow what it strikes with is where a blow
puts it (a spear's or gun's line forward and level, AIM, a gun's muzzle ahead and a spear's tip in a foe at the
battle's 140 cm, never out through him; a club's head ahead, low and moving; a fist ahead and moving, a fist or the
crook short of a foe at 140; a bow upright, its grip ahead and about shoulder high); and what the hands hold, tested
at points along its edges (a spear's shaft has vertices only at its ends), never comes inside the chest or head (BODY)
in the Strike or Hit of a spear, club or fist (the fist its hand bone; a bow's or gun's only reported, held against the
body), nor inside the chest, head or thighs in a fall; nor, in the Strike or Hit, do the forearms and hands come more
than ARM_IN inside the chest where they were not at frame 0 (a draw hand brought down through the belly); and in no
take do the seams between the head and the body open more than NECK (reported as "neck"). A figure failing only these,
or whose one-shots could not be keyed at all (rig_actions.ERRORS), is exported without its one-shots (rig_figures.py).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
import numpy as np  # noqa: E402

import mocap  # noqa: E402
import rig_actions  # noqa: E402

LIMBS = ("pelvis", "spine", "head", "thigh_r", "thigh_l", "shin_r", "shin_l", "foot_r", "foot_l", "upperarm_r", "upperarm_l", "forearm_r",
         "forearm_l")
SLIDE = 3.0   # cm
SINK = 1.0    # cm
CONTACT = 1.0  # cm: a foot vertex this near the ground is on it
PLANTED = 3   # frames on the ground before a vertex counts as planted
SWING = 15.0  # cm: how far what a hand holds may move against the chest over a loop
STILL = 0.3   # cm: a planted shoe's vertices move less than this a frame in the world
PLANT_AT = 1  # frames: how far from frame 0 a gait's left foot may plant
ACTIONS = ("WalkSlow", "Walk", "Run", "Idle")
# One-shots (check_once): feet still within STEADY cm in Strike and Hit; a take's last frame within RETURN cm of its
# first and its frame 0 within REST degrees of the rest pose; a fall's every frame from the one it lies still from
# (rig_actions.ONCE) within LIE cm of its last, its hips no higher than HIPS cm and its lowest point (its robe aside)
# within -SINK..LYING cm of the ground at the end.
STEADY = 0.1
RETURN = 0.5
REST = 0.5
LIE = 0.2
HIPS = 30.0
LYING = 3.0
# cm: the body, as cylinders that nothing a man strikes with (or drops as he falls) enters: the chest about the line
# from the hips' joint to the neck, the head about its bone, each thigh about its bone. What a hand holds is tested at
# points ALONG cm apart along its edges.
TORSO, HEAD, THIGH, ALONG = 11.0, 10.0, 7.0, 2.0
BODY = {"chest": ("spine", TORSO), "head": ("head", HEAD), "thigh_r": ("thigh_r", THIGH), "thigh_l": ("thigh_l", THIGH)}
# cm: no vertex moves further than JUMP in one frame, but in the BLOW_FRAMES frames into a Strike's impact (the blow
# is the fastest thing in it); at a fall's end its hips and chest, its legs and what its hands hold each lie within PART
# of the ground.
JUMP, BLOW_FRAMES, PART = 60.0, 4, 4.0
# degrees: no bone turns further than TURN in one frame, the Strike's blow aside (a spear's or club's wind-up, the arm
# raised beside the ear and rolled to bring the elbow out in 6 frames, turns it up to about 64; a foot turned over or an
# arm rolled half over about its own length in a frame turned 115 or more); cm: in a fall no robe rises more than ROBE
# above the hips (the pelvis bone's head; rig_actions.drape keeps it within rig_actions.ROBE_UP).
TURN, ROBE = 70.0, 30.0
# cm a frame: how much a man falling back onto his seat (DeathBack, until rig_actions.ONCE's "seat") may slow his hips'
# drop from one frame to the next: the fall quickens all the way (his legs give under him; they do not catch him in
# mid-air, as they did for a man propped up by a sword at his hip, 5.4 to 1.9 cm a frame).
SLOW = 0.5
# cm: how far the seams between the head and the body (its hips and chest) may open in a one-shot: rig_actions.NECK,
# which its keys keep to, and half a cm for the frames between them (a quiver worn across the neck tore 31 cm open as
# a fallen man's face turned onto its side).
NECK = rig_actions.NECK + 0.5
# cm: how far inside the chest (TORSO about its line) a forearm or hand may come in a Strike or Hit where it was not at
# frame 0 (an archer's draw hand at his chin comes about 0.5 cm inside, its line ending at the neck).
ARM_IN = 1.0
# At the blow, by class: how far off forward in plan and off level its line may be (degrees), and how far ahead of the
# origin its business end must be, and (a spear) may be, in that frame or any other (cm): a spear's tip in a foe at
# the battle's 140 cm (rig_actions.REACH), between his chest and his back, not out through it; a club's head at
# least CLUB_AHEAD ahead, no higher than CLUB_HIGH, and moving at least BLOW cm over the two frames before (a blow, not
# a pose), as must a fist, FIST_AHEAD ahead (a fist or EGYPT's crook falls well short of a foe at 140, reaching as far
# as an arm does; the macuahuitl, a club, reaches him); a bow within BOW_UPRIGHT degrees of upright, its grip
# BOW_AHEAD ahead and BOW_LOW..BOW_HIGH high.
AIM = {"spear": (20.0, 25.0, 130.0, 160.0), "gun": (20.0, 25.0, 80.0, None)}
CLUB_AHEAD, CLUB_HIGH, BLOW, FIST_AHEAD = 90.0, 150.0, 25.0, 70.0
BOW_UPRIGHT, BOW_AHEAD, BOW_LOW, BOW_HIGH = 25.0, 35.0, 120.0, 165.0


def _coords(obj, depsgraph):
    ev = obj.evaluated_get(depsgraph)
    me = ev.to_mesh()
    co = np.empty(len(me.vertices) * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    return co.reshape(-1, 3)


def unkeyed(arm, action):
    """The bones `action` has no rotation curve for (and the hips and hands, rig_actions.PLACED, if it does not place
    them)."""
    keyed = {(fc.data_path.split('"')[1], fc.data_path.rsplit(".", 1)[1]) for fc in rig_actions._fcurves(action) if '"' in fc.data_path}
    out = [b.name for b in arm.pose.bones if (b.name, "rotation_euler") not in keyed and (b.name, "rotation_quaternion") not in keyed]
    return out + ["%s's place" % b for b in rig_actions.PLACED if b in arm.pose.bones and (b, "location") not in keyed]


def _owners(body):
    """Each vertex's bone: the group weighing most on it."""
    groups = {g.index: g.name for g in body.vertex_groups}
    owner = [""] * len(body.data.vertices)
    for v in body.data.vertices:
        best = max(v.groups, key=lambda g: g.weight, default=None)
        owner[v.index] = groups[best.group] if best else ""
    return owner


def check(arm, body, name, speeds, fps, held=None):
    """Checks one rigged figure; returns (problems, report line). `held`: the vertices each hand holds ({"r" or "l":
    indices}, rig_figures.HELD[name])."""
    problems = []
    n = len(body.data.vertices)
    owner = _owners(body)
    counts = {b: owner.count(b) for b in LIMBS}
    bare = [b for b in LIMBS if counts[b] == 0]
    if bare:
        problems.append("no vertices on " + ", ".join(bare))
    for side, held_by in sorted((held or {}).items()):
        stray = sum(1 for i in held_by if owner[i] != "hand_" + side)
        if stray:
            problems.append("%d vertices held in hand_%s not on it" % (stray, side))
    rest = np.empty(n * 3)
    body.data.vertices.foreach_get("co", rest)
    rest = rest.reshape(-1, 3)
    feet = np.array([i for i in range(n) if owner[i].startswith("foot_") or (owner[i].startswith("shin_") and rest[i, 2] < 16.0)], dtype=np.int64)
    lead = np.array([k for k, i in enumerate(feet) if owner[i] == "foot_" + mocap.LEAD], dtype=np.int64)  # the left shoe, in feet
    others = np.array([i for i in range(n) if not owner[i].startswith(("shin_", "foot_"))], dtype=np.int64)
    # Seams: edges between two bones' parts (a knee, hip, elbow or neck); cloth: edges of what the skirt bones
    # swing (a robe flares as its front and back part, so it stretches by design).
    skirt = {g.index for g in body.vertex_groups if g.name.startswith("skirt_")}
    swung = [any(g.group in skirt and g.weight > 0.0 for g in v.groups) for v in body.data.vertices]
    pairs = [(e.vertices[0], e.vertices[1]) for e in body.data.edges]
    seam_edges = [(a, b) for a, b in pairs if owner[a] != owner[b] and not swung[a] and not swung[b]]
    cloth_edges = [(a, b) for a, b in pairs if swung[a] or swung[b]]

    def edge_set(chosen):
        e = np.array(chosen, dtype=np.int64).reshape(-1, 2)
        length = np.linalg.norm(rest[e[:, 0]] - rest[e[:, 1]], axis=1)
        keep = length > 0.2
        return e[keep], length[keep]

    edges, rest_len = edge_set(seam_edges)
    cloth, cloth_len = edge_set(cloth_edges)
    carried = np.array(sorted({i for side in (held or {}).values() for i in side}), dtype=np.int64)
    spine_rest = np.array(arm.data.bones["spine"].matrix_local)
    scene = bpy.context.scene
    parts = []
    for act_name in ACTIONS:
        act = bpy.data.actions.get(act_name)
        if act is None:
            continue
        loose = unkeyed(arm, act)
        if loose:
            problems.append("%s: no keys for %s" % (act_name, ", ".join(loose)))
        arm.animation_data.action = act
        if hasattr(arm.animation_data, "action_slot") and getattr(act, "slots", None) and len(act.slots):
            arm.animation_data.action_slot = act.slots[0]
        frames = int(round(act.frame_range[1] - act.frame_range[0]))
        speed = speeds.get(act_name, 0.0)
        foot_track, sink, low_other, stretch, flare, item_track = [], 1e9, 1e9, 0.0, 0.0, []
        for f in range(frames):
            scene.frame_set(int(act.frame_range[0]) + f)
            co = _coords(body, bpy.context.evaluated_depsgraph_get())
            if len(carried):
                # Back into the chest's rest frame (the body is the armature's child at its origin).
                M = spine_rest @ np.linalg.inv(np.array(arm.pose.bones["spine"].matrix))
                item_track.append(co[carried] @ M[:3, :3].T + M[:3, 3])
            if len(feet):
                foot_track.append(co[feet].copy())
                sink = min(sink, float(co[feet, 2].min()))
            if len(others):
                if os.environ.get("SOV_CHECK_DEBUG") and float(co[others, 2].min()) < low_other - 0.5:
                    i = others[int(np.argmin(co[others, 2]))]
                    print("  %s f%d lowest other %.1f: vertex %d (%s) at rest %s" % (act_name, f, co[i, 2], i, owner[i], rest[i].round(1)))
                low_other = min(low_other, float(co[others, 2].min()))
            if len(edges):
                length = np.linalg.norm(co[edges[:, 0]] - co[edges[:, 1]], axis=1)
                if os.environ.get("SOV_CHECK_DEBUG") and float((length - rest_len).max()) > stretch + 0.5:
                    e = int(np.argmax(length - rest_len))
                    a, b = edges[e]
                    print("  %s f%d seam +%.1f: %s %s - %s %s" % (act_name, f, length[e] - rest_len[e], owner[a], rest[a].round(1), owner[b], rest[b].round(1)))
                stretch = max(stretch, float((length - rest_len).max()))
            if len(cloth):
                length = np.linalg.norm(co[cloth[:, 0]] - co[cloth[:, 1]], axis=1)
                flare = max(flare, float((length - cloth_len).max()))
        # Planted-foot slide over two laps of the loop (so a stance across its seam counts whole), in the world: the
        # figure walks to -Y at `speed`.
        slide, plant = 0.0, None
        if foot_track:
            track = np.array(foot_track + foot_track)  # (2 * frames, feet, 3)
            track[:, :, 1] -= (speed / fps) * np.arange(2 * frames)[:, None]
            down = track[:, :, 2] < CONTACT
            for k in range(track.shape[1]):
                f = 0
                while f < 2 * frames:
                    if down[f, k]:
                        g = f
                        while g + 1 < 2 * frames and down[g + 1, k]:
                            g += 1
                        if g - f + 1 >= PLANTED and f > 0 and g < 2 * frames - 1:
                            xy = track[f:g + 1, k, :2]
                            d = float(np.linalg.norm(xy - xy[0], axis=1).max())
                            if d > slide and os.environ.get("SOV_CHECK_DEBUG"):
                                i = feet[k]
                                print("  %s slide %.1f: vertex %d (%s) at rest %s, frames %d-%d" % (act_name, d, i, owner[i], rest[i].round(1),
                                                                                                f % frames, g % frames))
                            slide = max(slide, d)
                        f = g + 1
                    else:
                        f += 1
            # A gait's left foot plants at frame 0: where the longest stretch over which its shoe holds still on the
            # ground begins (counted round the loop from a frame where it does not, so no stretch is cut).
            if speed > 0.0 and len(lead):
                shoe = track[:frames + 1, lead]
                still = (np.linalg.norm(shoe[1:] - shoe[:-1], axis=2).max(axis=1) < STILL) & (shoe[:-1, :, 2].min(axis=1) < CONTACT)
                if still.any() and not still.all():
                    start, run, best = int(np.argmin(still)), 0, 0
                    for k in range(1, frames + 1):
                        f = (start + k) % frames
                        run = run + 1 if still[f] else 0
                        if run > best:
                            best, plant = run, (f - run + 1) % frames
                    plant = plant if plant <= frames // 2 else plant - frames
        # What the hands hold: the span of each vertex's path against the chest, the widest.
        swing = 0.0
        if item_track:
            track = np.array(item_track)  # (frames, vertices, 3)
            swing = float(np.linalg.norm(track.max(axis=0) - track.min(axis=0), axis=1).max())
        if slide > SLIDE:
            problems.append("%s: a planted foot slides %.1f cm" % (act_name, slide))
        if sink < -SINK:
            problems.append("%s: a foot goes %.1f cm into the ground" % (act_name, -sink))
        if swing > SWING:
            problems.append("%s: what a hand holds swings %.1f cm" % (act_name, swing))
        if speed > 0.0 and (plant is None or abs(plant) > PLANT_AT):
            problems.append("%s: the left foot plants %s" % (act_name, "nowhere" if plant is None else "at frame %d, not 0" % plant))
        parts.append("%s %d fr %.1f cm/s slide %.1f foot %.1f other %.1f seam +%.1f cloth +%.1f held %.1f%s" % (
            act_name, frames, speed, slide, sink, low_other, stretch, flare, swing,
            "" if speed <= 0.0 else " plant0 " + ("-" if plant is None else "%d" % plant)))
    line = "check %s: %s | %s" % (name, "ok" if not problems else "FAIL (" + "; ".join(problems) + ")", " | ".join(parts))
    return problems, line


def _off_line(points, a, b):
    """How far each point (rows) is from the segment a-b."""
    ab = b - a
    t = np.clip((points - a) @ ab / max(float(ab @ ab), 1e-9), 0.0, 1.0)
    return np.linalg.norm(points - (a + t[:, None] * ab), axis=1)


def _along(edges, rest, indices):
    """Points along every edge of a solid (its vertices `indices`), at most ALONG cm apart, as (a, b, t): each point is
    vertex a + (vertex b - vertex a) * t, as it moves (the solid rides one bone, so its edges stay straight). A
    spear's shaft has vertices only at its ends; these see the whole of it."""
    keep = set(indices)
    a, b, t = list(indices), list(indices), [0.0] * len(indices)
    for i, j in edges:
        if i in keep and j in keep:
            m = int(np.linalg.norm(rest[i] - rest[j]) // ALONG)
            for k in range(1, m + 1):
                a.append(i)
                b.append(j)
                t.append(k / (m + 1.0))
    return np.array(a, dtype=np.int64), np.array(b, dtype=np.int64), np.array(t)[:, None]


def check_once(arm, body, name, held, cls, items):
    """Checks a figure's one-shot actions (rig_actions.ONCE, those it has; see the module's notes) over one lap each,
    its last frame included. `held`: rig_figures.HELD[name]; `cls`: how it strikes (rig_actions.strike_class); `items`:
    rig_actions.item_axes per side. Returns (problems, report line), or ([], "") for a figure with none."""
    if rig_actions.ERRORS.get(name):
        problem = "its one-shots could not be keyed (%s)" % rig_actions.ERRORS[name]
        return [problem], "check %s once (%s): FAIL (%s)" % (name, cls or "unknown", problem)
    takes = [t for t in rig_actions.ONCE if bpy.data.actions.get(t)]
    if not takes:
        return [], ""
    problems, parts = [], []
    owner = _owners(body)
    n_verts = len(owner)
    rest = np.empty(n_verts * 3)
    body.data.vertices.foreach_get("co", rest)
    rest = rest.reshape(-1, 3)
    feet = np.array([i for i in range(n_verts) if owner[i].startswith("foot_") or (owner[i].startswith("shin_") and rest[i, 2] < 16.0)],
                    dtype=np.int64)
    skirt = {g.index for g in body.vertex_groups if g.name.startswith("skirt_")}
    swung = [any(g.group in skirt and g.weight > 0.0 for g in v.groups) for v in body.data.vertices]
    swung_idx = np.array([i for i in range(n_verts) if swung[i]], dtype=np.int64)
    edges = [(e.vertices[0], e.vertices[1]) for e in body.data.edges]
    e = np.array([(a, b) for a, b in edges if owner[a] != owner[b] and not swung[a] and not swung[b]], dtype=np.int64).reshape(-1, 2)
    e_len = np.linalg.norm(rest[e[:, 0]] - rest[e[:, 1]], axis=1)
    e, e_len = e[e_len > 0.2], e_len[e_len > 0.2]
    ends = [{owner[a], owner[b]} for a, b in e]
    neck = np.array([k for k, pair in enumerate(ends) if pair in ({"head", "pelvis"}, {"head", "spine"})], dtype=np.int64)
    held = held or {}
    carried = {i for side in held.values() for i in side}
    along = {s: _along(edges, rest, held[s]) for s in ("r", "l") if held.get(s)}
    # At a fall's end each of these lies on the ground (by the bones they ride, cloth aside): the hips and chest, the
    # legs and what each hand holds.
    lying = {"chest": [i for i in range(n_verts) if owner[i] in ("pelvis", "spine") and not swung[i] and i not in carried],
             "legs": [i for i in range(n_verts) if owner[i].startswith(("thigh_", "shin_")) and not swung[i]]}
    lying.update(("item_" + s, held[s]) for s in ("r", "l") if held.get(s))
    # The forearms and hands themselves (what they hold aside), kept out of the chest in a Strike or Hit.
    arms = np.array([i for i in range(n_verts) if owner[i].startswith(("forearm_", "hand_")) and not swung[i] and i not in carried],
                    dtype=np.int64)
    scene = bpy.context.scene
    P = arm.pose.bones
    for take in takes:
        spec = rig_actions.ONCE[take]
        n = spec["frames"]
        act = bpy.data.actions[take]
        found = []
        loose = unkeyed(arm, act)
        if loose:
            found.append("no keys for " + ", ".join(loose))
        rig_actions.play(arm, act)
        falls = take not in ("Strike", "Hit")
        # What the body keeps out of it, in this take: the chest and head from what a spear, club or fist fighter
        # strikes with (a bow's or a gun's only reported: they are held against the body), and in a fall the thighs too,
        # from whatever the hands hold as they drop it.
        if falls:
            keep_out = ("chest", "head", "thigh_r", "thigh_l")
        else:
            keep_out = ("chest", "head")
        track, bones, turns, stretch, opened, clear, arm_in = [], [], [], 0.0, (0.0, 0), (1e9, "", 0), (0.0, 0)
        for f in range(n + 1):
            scene.frame_set(f)
            co = _coords(body, bpy.context.evaluated_depsgraph_get())
            track.append(co)
            at = {b: (np.array(P[b].head), np.array(P[b].tail)) for b in ("pelvis", "spine", "head", "hand_r", "hand_l", "thigh_r", "thigh_l")}
            bones.append(at)
            turns.append({b.name: b.matrix.to_quaternion() for b in P})
            if f == 0:
                turned = max(P[b.name].matrix_basis.to_quaternion().angle for b in arm.data.bones)
                moved = P["pelvis"].matrix_basis.to_translation().length
            if len(e):
                seams = np.linalg.norm(co[e[:, 0]] - co[e[:, 1]], axis=1) - e_len
                stretch = max(stretch, float(seams.max()))
                if len(neck) and float(seams[neck].max()) > opened[0]:
                    opened = (float(seams[neck].max()), f)
            if cls == "fist" and not falls:
                what = np.array([at["hand_r"][0], at["hand_r"][1]])
            else:
                what = [co[a] + (co[b] - co[a]) * t for a, b, t in along.values()]
                what = np.concatenate(what) if what else np.zeros((0, 3))
            if not falls and len(arms):
                # How far inside the chest each forearm and hand vertex is (negative: outside), against where it was at
                # frame 0 (a hand at rest may lie that close to the hips).
                depth = TORSO - _off_line(co[arms], at["pelvis"][0], at["spine"][1])
                if f == 0:
                    inside0 = depth > 0.0
                deep = float(depth[~inside0].max()) if (~inside0).any() else 0.0
                if deep > arm_in[0]:
                    arm_in = (deep, f)
            if len(what):
                for part in keep_out:
                    bone, radius = BODY[part]
                    start = at["pelvis"][0] if part == "chest" else at[bone][0]
                    gap = float(_off_line(what, start, at[bone][1]).min()) - radius
                    if gap < clear[0]:
                        clear = (gap, part, f)
        track = np.array(track)
        # Nothing goes into the ground but, in a fall from the buckle on, the robe (rig_actions.drape: the ground
        # hides it).
        body_only = ~np.array(swung, dtype=bool)
        low = float(track[:, :, 2].min()) if not falls else min(float(track[:spec["buckle"], :, 2].min()),
                                                                 float(track[spec["buckle"]:, body_only, 2].min()))
        if low < -SINK:
            found.append("something goes %.1f cm into the ground" % -low)
        # How far anything moves in one frame, the Strike's blow aside (the frames into its impact).
        step = np.linalg.norm(track[1:] - track[:-1], axis=2).max(axis=1)
        if take == "Strike":
            step[spec["impact"] - BLOW_FRAMES:spec["impact"]] = 0.0
        jump = (float(step.max()), int(step.argmax()))
        if jump[0] > JUMP:
            found.append("something moves %.0f cm from frame %d to %d" % (jump[0], jump[1], jump[1] + 1))
        # How far any bone turns in one frame (armature space), the Strike's blow aside: a foot turned over or an arm
        # rolled half over about its own length moves little but shows.
        quick = range(spec["impact"] - BLOW_FRAMES, spec["impact"]) if take == "Strike" else ()
        turn = max((math.degrees(2.0 * math.acos(min(1.0, abs(turns[f][b].dot(turns[f + 1][b]))))), f, b)
                   for f in range(n) if f not in quick for b in turns[f])
        if turn[0] > TURN:
            found.append("%s turns %.0f degrees from frame %d to %d" % (turn[2], turn[0], turn[1], turn[1] + 1))
        if clear[0] < 0.0 and (falls or cls in ("spear", "club", "fist")):
            found.append("%s %.1f cm inside the %s at frame %d" % ("what it holds goes" if cls != "fist" or falls else "the fist goes",
                                                                    -clear[0], clear[1], clear[2]))
        if arm_in[0] > ARM_IN:
            found.append("a forearm or hand goes %.1f cm inside the chest at frame %d" % arm_in)
        if opened[0] > NECK:
            found.append("its neck opens %.1f cm at frame %d" % opened)
        if take in ("Strike", "Hit"):
            steady = float(np.linalg.norm(track[:, feet] - track[0, feet], axis=2).max()) if len(feet) else 0.0
            back = float(np.linalg.norm(track[n] - track[0], axis=1).max())
            if steady > STEADY:
                found.append("a foot moves %.2f cm" % steady)
            if back > RETURN:
                found.append("its last frame is %.1f cm from its first" % back)
            if math.degrees(turned) > REST or moved > RETURN:
                found.append("frame 0 is not the rest pose (%.1f degrees, %.1f cm)" % (math.degrees(turned), moved))
            note = "low %.1f arm %.1f" % (low, arm_in[0])
        else:
            buckle = spec["buckle"]
            ground = feet[track[0, feet, 2] < CONTACT] if len(feet) else feet
            slide = float(np.linalg.norm(track[:buckle + 1, ground, :2] - track[0, ground, :2], axis=2).max()) if len(ground) else 0.0
            if slide > SLIDE:
                found.append("a planted foot slides %.1f cm by frame %d" % (slide, buckle))
            still = float(np.linalg.norm(track[spec["still"]:] - track[n], axis=2).max())
            hips = float(bones[n]["pelvis"][0][2])
            end = float(track[n, body_only, 2].min())
            ahead = bones[n]["head"][0][1] - bones[n]["pelvis"][0][1]
            if still > LIE:
                found.append("it still moves %.1f cm after frame %d" % (still, spec["still"]))
            if hips > HIPS:
                found.append("its hips end %.0f cm up" % hips)
            if not -SINK <= end <= LYING:
                found.append("its lowest point ends %.1f cm from the ground" % end)
            # The robe (cloth the skirt bones swing) no higher than ROBE above the hips at any frame.
            robe = float(max(track[f, swung_idx, 2].max() - bones[f]["pelvis"][0][2] for f in range(n + 1))) if len(swung_idx) else -1e9
            if robe > ROBE:
                found.append("its robe rises %.0f cm above its hips" % robe)
            if "seat" in spec:
                # The hips' drop into each frame until he lands on his seat, and the most it slows in a frame.
                drop = [bones[f - 1]["pelvis"][0][2] - bones[f]["pelvis"][0][2] for f in range(1, spec["seat"] + 1)]
                slows = max(float(a - b) for a, b in zip(drop, drop[1:]))
                if slows > SLOW:
                    found.append("its hips' drop slows %.1f cm a frame before it lands on its seat at %d" % (slows, spec["seat"]))
            if (ahead > 0.0) == (take == "Death"):
                found.append("its head ends %s its hips" % ("behind" if take == "Death" else "ahead of"))
            up = {part: float(track[n, idx, 2].min()) for part, idx in lying.items() if len(idx)}
            high = ["%s %.0f cm" % (part, z) for part, z in up.items() if z > PART]
            if high:
                found.append("it ends off the ground: " + ", ".join(high))
            note = "low %.1f slide %.1f end %.1f hips %.0f lies %s" % (low, slide, end, hips, "/".join("%.0f" % z for z in up.values()))
            if len(swung_idx):
                note += " robe %.0f" % robe
            if "seat" in spec:
                note += " slows %.1f" % slows
        note += (" clear %.1f jump %.0f" % (clear[0], jump[0]) if clear[1] else " jump %.0f" % jump[0]) + " turn %.0f" % turn[0]
        if take == "Strike":
            f = spec["impact"]
            co = track[f]
            blow = None
            if cls in AIM and items.get("r"):
                tip, butt = co[items["r"]["tip"]], co[items["r"]["butt"]]
                line = (tip - butt) / np.linalg.norm(tip - butt)
                plan = math.degrees(math.atan2(abs(line[0]), -line[1])) if line[1] < 0.0 else 180.0
                level = math.degrees(math.asin(min(1.0, abs(line[2]))))
                off, tilt, reach, most = AIM[cls]
                if plan > off or level > tilt or -tip[1] < reach or (most and -tip[1] > most):
                    found.append("at the blow its line is %.0f degrees off forward and %.0f off level, %.0f cm ahead" % (plan, level, -tip[1]))
                furthest = float(-track[:, items["r"]["tip"], 1].min())
                if most and furthest > most:
                    found.append("its tip goes %.0f cm ahead, out through a foe at %.0f" % (furthest, rig_actions.REACH))
                blow = tip
            elif cls == "club" and items.get("r"):
                blow = co[items["r"]["tip"]]
                swing = float(np.linalg.norm(blow - track[f - 2][items["r"]["tip"]]))
                if -blow[1] < CLUB_AHEAD or blow[2] > CLUB_HIGH or swing < BLOW:
                    found.append("at the blow its head is %.0f cm ahead and %.0f up, moving %.0f cm" % (-blow[1], blow[2], swing))
            elif cls == "bow" and items.get("l"):
                tip, butt = co[items["l"]["tip"]], co[items["l"]["butt"]]
                upright = math.degrees(math.acos(min(1.0, abs(tip[2] - butt[2]) / np.linalg.norm(tip - butt))))
                blow = co[items["l"]["grip"]]
                if upright > BOW_UPRIGHT or -blow[1] < BOW_AHEAD or not BOW_LOW <= blow[2] <= BOW_HIGH:
                    found.append("at the loose the bow is %.0f degrees off upright, its grip %.0f cm ahead and %.0f up" % (upright, -blow[1], blow[2]))
            elif cls == "fist":
                blow = bones[f]["hand_r"][1]
                swing = float(np.linalg.norm(blow - bones[f - 2]["hand_r"][1]))
                if -blow[1] < FIST_AHEAD or swing < BLOW:
                    found.append("at the blow the fist is %.0f cm ahead, moving %.0f cm" % (-blow[1], swing))
            if blow is not None:
                note = "blow %.0f ahead %.0f up, " % (-blow[1], blow[2]) + note
        problems += ["%s: %s" % (take, p) for p in found]
        parts.append("%s %d fr %s seam +%.1f neck +%.1f%s" % (take, n, note, stretch, opened[0], "" if not found else " FAIL"))
    line = "check %s once (%s): %s | %s" % (name, cls, "ok" if not problems else "FAIL (" + "; ".join(problems) + ")", " | ".join(parts))
    return problems, line


def main():
    import rig_figures as R

    # Figure names follow "--" under Blender (without one, Blender's own arguments are all there is); pip's bpy takes
    # them as plain arguments.
    under_blender = any(a in sys.argv for a in ("--python", "-P"))
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ([] if under_blender else sys.argv[1:])
    names = args or list(R.PEOPLE)
    failed = []
    for name in names:
        if name.startswith("Rider"):
            import rig_riders

            if name not in rig_riders.RIDERS:
                print("check %s: no such horseman (rig_riders.RIDERS: %s)" % (name, ", ".join(rig_riders.RIDERS)))
                failed.append(name)
                continue
            arm, body = rig_riders.rig(name, rig_riders.RIDERS[name])
            problems, lines = rig_riders.check(arm, body, name)
            print("\n".join(lines))
        else:
            arm, body = R.rig(name, R.PEOPLE[name])
            problems, line = check(arm, body, name, R.SPEEDS, R.FPS, R.HELD.get(name))
            print(line)
            try:
                once, once_line = check_once(arm, body, name, R.HELD.get(name), R.CLASSES.get(name), R.ITEMS.get(name))
            except Exception as error:  # (as rig_figures.main: reported as a failure, and the next figure checked)
                once = ["its one-shots could not be checked (%s: %s)" % (type(error).__name__, error)]
                once_line = "check %s once (%s): FAIL (%s)" % (name, R.CLASSES.get(name) or "unknown", once[0])
            if once_line:
                print(once_line)
            problems = problems + once
        if problems:
            failed.append(name)
    print("check_rigs: %d of %d figures ok%s" % (len(names) - len(failed), len(names), (": failed " + ", ".join(failed)) if failed else ""))
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
