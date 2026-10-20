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
- and, reported only: how far the seams between two bones open ("seam": the edge between two bones' parts
  that stretches most beyond its length at rest, in cm, such as a knee or hip in a long stride), how far the cloth
  the skirt bones swing stretches ("cloth": a robe flares as its front and back part, by design), and the lowest
  point of anything else ("other": a robe's hem, a spear's butt). SOV_CHECK_DEBUG=1 names the vertices behind the
  worst seam, slide and lowest point.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
import numpy as np  # noqa: E402

import mocap  # noqa: E402

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


def _coords(obj, depsgraph):
    ev = obj.evaluated_get(depsgraph)
    me = ev.to_mesh()
    co = np.empty(len(me.vertices) * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    return co.reshape(-1, 3)


def check(arm, body, name, speeds, fps, held=None):
    """Checks one rigged figure; returns (problems, report line). `held`: the vertices each hand holds ({"r" or "l":
    indices}, rig_figures.HELD[name])."""
    problems = []
    groups = {g.index: g.name for g in body.vertex_groups}
    n = len(body.data.vertices)
    owner = [""] * n
    for v in body.data.vertices:
        best = max(v.groups, key=lambda g: g.weight, default=None)
        owner[v.index] = groups[best.group] if best else ""
    counts = {b: owner.count(b) for b in LIMBS}
    bare = [b for b in LIMBS if counts[b] == 0]
    if bare:
        problems.append("no vertices on " + ", ".join(bare))
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
        if problems:
            failed.append(name)
    print("check_rigs: %d of %d figures ok%s" % (len(names) - len(failed), len(names), (": failed " + ", ".join(failed)) if failed else ""))
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
