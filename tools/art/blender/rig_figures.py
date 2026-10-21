"""The Figures kit's people rigged and animated for the street scene and battles: each figure (kit_figures.py) gets
the same simple skeleton (18 bones: hips, chest, head, arms, hands, legs, feet, and a skirt's front and back), its
vertices bound rigidly to one limb (shoes to the feet, what a hand holds to that hand and a scabbard to the hips, whole;
the legs' own blended across the hip and knee; the cloth below the hips shared with the skirt bones, which swing with
the legs), and four looping actions from motion capture (mocap.py, the clips in tools/art/mocap/): WalkSlow, Walk, Run
and Idle, in place, each authored at a speed it prints (cm/s; the game plays a loop at its own speed / that, choosing
the loop by speed: tools/art/README.md). Without the clips it falls back to a procedural Walk and Idle (no WalkSlow or
Run). A figure that fights also gets the one-shots for live battles (rig_actions.py, by its weapon class, which
this script prints as it rigs the figure): Strike, Hit, Death and DeathBack. Every action keys every bone. Each
figure is checked (check_rigs.py: skin, planted feet, ground, what the hands hold, each gait starting as the left foot
plants, and the one-shots) before it is exported as an FBX skeletal mesh with its animations to
<build>/Rigged/SK_<Name>.fbx for tools/art/ue_import_rigged.py; one that fails is not exported (an older export of it
is removed) and the script exits 1, but one that fails only the one-shots' checks (or whose one-shots could not be
keyed) is exported without them, saying why.

  blender -b --factory-startup --python-exit-code 1 --python tools/art/blender/rig_figures.py -- art/build [Citizen ...]

Figures stand at the origin facing -Y, metres, as the kits are built.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Euler, Matrix, Vector  # noqa: E402

import kit_figures  # noqa: E402
import kit_leaders  # noqa: E402
import check_rigs  # noqa: E402
import kitlib  # noqa: E402
import mocap  # noqa: E402
import rig_actions  # noqa: E402


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
# Foot soldiers in their cultures' dress (kit_figures.py), marching in live battles as the plain ones do. Riders stay still:
# this skeleton has no horse.
for _arm in ("Soldier", "Archer", "Musketeer"):
    for _style in ("MiddleEast", "Asian", "African", "American"):
        PEOPLE["%s_%s" % (_arm, _style)] = getattr(kit_figures, "%s_%s" % (_arm.lower(), _style.lower()))
FPS = 24
CM = 100.0  # the rig is built in centimetres (see rig)

# name: (head, tail, parent); sides are r (+x) and l (-x).
BONES = {"root": ((0, 0, 0), (0, 0, 0.15), None), "pelvis": ((0, 0, 0.92), (0, 0, 1.05), "root"), "spine": ((0, 0, 1.05), (0, 0, 1.4), "pelvis"),
         "head": ((0, 0, 1.42), (0, 0, 1.75), "spine")}
for s, n in ((1, "r"), (-1, "l")):
    BONES["thigh_" + n] = ((s * 0.09, 0, 0.92), (s * 0.1, -0.01, 0.5), "pelvis")
    BONES["shin_" + n] = ((s * 0.1, -0.01, 0.5), (s * 0.1, 0.01, 0.08), "thigh_" + n)
    BONES["foot_" + n] = ((s * 0.1, 0.01, 0.08), (s * 0.1, -0.12, 0.03), "shin_" + n)  # ankle to the ball of the foot
    BONES["upperarm_" + n] = ((s * 0.215, 0, 1.37), (s * 0.255, 0.01, 1.11), "spine")
    BONES["forearm_" + n] = ((s * 0.255, 0.01, 1.11), (s * 0.265, -0.03, 0.8), "upperarm_" + n)
# The front and back of a skirt, hung from the hips' axis and swung by the legs (key_skirt).
BONES["skirt_f"] = ((0, -0.02, 0.92), (0, -0.12, 0.45), "pelvis")
BONES["skirt_b"] = ((0, 0.02, 0.92), (0, 0.12, 0.45), "pelvis")
# The hands, from the wrist (the forearm's tail) a hand's length on along the forearm's line: what a hand holds rides
# its hand bone, so a strike can turn it at the wrist (a spear levelled, a musket shouldered: rig_actions.py). At rest
# a hand lies along its forearm, so the loops are as they were before it. (Last, so the other bones keep their order.)
for n in ("r", "l"):
    _wrist = Vector(BONES["forearm_" + n][1])
    BONES["hand_" + n] = (tuple(_wrist), tuple(_wrist + (_wrist - Vector(BONES["forearm_" + n][0])).normalized() * 0.08), "forearm_" + n)


def bone_for(x, y, z, hands=True, arms=True, legs=True):
    """Which limb carries a vertex: weapons with the hands that hold them, limbs by where they are, cloth with the body.
    (Shoes and whatever is held at the side go whole by whole_parts first, which also says which of the hand, arm and
    leg rules a solid escapes: see there.)"""
    side = "r" if x > 0 else "l"
    # Below the elbow: above it, out that far is a broad upper arm or sleeve (on the forearm it tore at every swing).
    if hands and x > 0.29 and z < 1.12:
        return "forearm_r"  # the spear, staff or musket in the right hand
    if hands and x < -0.3 and z < 1.12:
        return "forearm_l"  # the shield or bow in the left
    if arms and abs(x) > 0.18 and 0.72 < z < 1.42 and abs(y) < 0.09:
        return ("upperarm_" if z > 1.12 else "forearm_") + side
    if z < 0.92:
        # A leg only within its own limb's radius; a skirt or robe around it stays with the hips.
        if legs and math.hypot(abs(x) - 0.095, y) < 0.09:
            return ("thigh_" if z > 0.5 else "shin_") + side
        return "pelvis"
    if z > 1.43:
        return "head"
    return "pelvis" if z < 1.05 else "spine"


HIPS, SKIRT_FULL = 0.92, 0.1  # m: the skirt bones' axis (the hips'), and how far below it cloth rides them wholly
HEM = 0.14  # m: a robe's hem below this is lifted toward it
ITEM_LIFT = 0.08  # m: how far a held spear or staff that stands on the ground is raised in the hand


def _smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3.0 - 2.0 * t)


def _off_limb(p, bones):
    """How far a point (metres) is from a chain of bones, each (name, how far past its tail it reaches)."""
    out = 1e9
    for bone, reach in bones:
        a, b = (Vector(c) for c in BONES[bone][:2])
        out = min(out, mocap._off_segment(p, a, b + (b - a).normalized() * reach))
    return out


# A leg's own vertices near the hip and the knee share the joint with the bone across it: SHARE at the joint, none
# BLEND (m) along the leg from it. The thigh's front then creases into the hips as it swings forward instead of
# turning up and out through a robe's front, and a knee bends without opening a gap. The leg keeps the larger share
# (it stays the vertex's owner for mocap.py and check_rigs.py).
HIP_BLEND, KNEE_BLEND, SHARE = 0.15, 0.06, 0.45


def _joint_weights(bone, p):
    """{bone: weight} for a vertex (metres) bone_for put on a thigh or shin."""
    head, tail, parent = BONES[bone]
    head, tail = Vector(head), Vector(tail)
    length = (tail - head).length
    t = (p - head).dot((tail - head) / length)  # along the leg from its upper joint
    if bone.startswith("thigh_"):
        across = ((parent, t, HIP_BLEND), ("shin_" + bone[-1], length - t, KNEE_BLEND))
    else:
        across = ((parent, t, KNEE_BLEND),)
    weights = {bone: 1.0}
    for other, d, blend in across:
        w = min(SHARE, SHARE * (1.0 - d / blend))
        if w > 0.0:
            weights[other] = w
            weights[bone] -= w
    return weights


NO_ARMS = {"hands": False, "arms": False}
CLOTH = dict(NO_ARMS, legs=False)
WRIST = 0.82      # m: the forearm's tail (0.8) and a little; what a hand holds reaches down to it, or is long
ON_ITEM = 0.1     # m: a small piece out past the hand this near what the hand holds is part of it
SHOULDERS = 1.6   # m: a piece out past the hands above the elbow sits on the shoulders if it starts below this


def whole_parts(mesh):
    """Solids bound by what they are rather than vertex by vertex (every solid is its own island of the mesh), so
    none is torn between two bones that move apart. Returns {vertex: bone} for those that ride one bone:
    - each shoe (kit_figures._sandals' foot box, low under one leg) on its foot;
    - anything out at the side below the elbow (centred past the hips), whole: what the hand holds (a spear, staff,
      shield, bow, musket or basket: mostly off the arm, and long or down to the hand) on that hand, and the small bits
      of it (a staff's caps, a spear's head, a bow's limbs, a bayonet) with it; or the forearm's own sleeve or cuff on
      the forearm;
    - a small piece of the arm (a hand, a sleeve, a shoulder's ball) wholly on the arm, and a small piece out past the
      hand on that forearm, or on that hand if it sits on what the hand holds (a shield's boss);
    - a small piece out past the hands above the elbow (a vest's winged shoulder, a headdress's outer plume) on the
      chest or the head;
    - a scabbard at the hip (long at the side, off the arm) on the hips, rigid (not swung as a skirt);
    and {vertex: bone_for's limits} for the rest, which its hand and arm rules leave alone: the limbs' own pieces
    keep its leg rule, while cloth (a tunic, robe or cloak, its hem flaring past the hands) and the small things on
    neither limb (a satchel, a pteryx) are no leg either: around the legs they are a skirt (see rig). Third, what the
    hands hold as (lowest corner, hand bone, vertices)."""
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
    out, fore, others, loose = {}, [], [], {}
    for members in islands.values():
        co = [mesh.vertices[i].co / CM for i in members]
        lo = [min(c[k] for c in co) for k in range(3)]
        hi = [max(c[k] for c in co) for k in range(3)]
        cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
        side = "r" if cx > 0 else "l"
        size = max(h - l for h, l in zip(hi, lo))
        arm_chain = (("upperarm_" + side, 0.0), ("forearm_" + side, 0.08))
        off_arm = sum(1 for c in co if _off_limb(c, arm_chain) > 0.09) * 5 > len(members)
        centre = Vector([(h + l) / 2 for h, l in zip(hi, lo)])
        on_arm = _off_limb(centre, arm_chain) <= 0.06
        if hi[2] < 0.1 and abs(abs(cx) - 0.1) < 0.04 and -0.12 < cy < 0.02 and hi[1] - lo[1] < 0.32 and hi[0] - lo[0] < 0.14:
            out.update((i, "foot_" + side) for i in members)
        elif abs(cx) > 0.25 and lo[2] < 1.0 and size > 0.2:
            # Held, on the hand, unless it is the forearm's own sleeve or a wide cuff (round the arm, short of the hand)
            # or a short piece off the arm (a bow's limb: it finishes what is held, below).
            hand = off_arm and (size >= 0.3 or lo[2] < WRIST)
            bone = ("hand_" if hand else "forearm_") + side
            out.update((i, bone) for i in members)
            fore.append((lo, hi, bone, list(members), "hand" if hand else "sleeve"))
            if not hand and not on_arm:
                others.append((members, lo, hi, on_arm, abs(cx) > 0.25))
        else:
            others.append((members, lo, hi, on_arm, abs(cx) > 0.25 and not on_arm))
            on_leg = _off_limb(centre, (("thigh_" + side, 0.0), ("shin_" + side, 0.0))) <= 0.06
            if abs(cx) < 0.15 and hi[0] - lo[0] > 0.25:
                loose.update((i, CLOTH) for i in members)  # wide on the body: cloth or the torso
            elif size >= 0.3:
                if abs(cx) < 0.15:
                    loose.update((i, NO_ARMS) for i in members)  # a leg
                elif on_arm and lo[2] > 0.72:
                    # A long sleeve down the arm (a robe's): above the elbow on the upper arm, below on the forearm. (A
                    # scabbard's middle can sit by the hand too, but it hangs on below it.)
                    out.update((i, ("upperarm_" if c[2] > 1.12 else "forearm_") + side) for i, c in zip(members, co))
                elif off_arm:
                    out.update((i, "pelvis") for i in members)  # long at the side but off the arm: a scabbard at the hip
                    fore.append((lo, hi, "pelvis", list(members), "worn"))
            elif on_arm:
                if any(bone_for(*c).startswith(("upperarm_", "forearm_")) for c in co):
                    # All of it on the arm, above the elbow on the upper arm (bone_for's split), none on the chest.
                    out.update((i, ("upperarm_" if c[2] > 1.12 else "forearm_") + side) for i, c in zip(members, co))
            elif sum(1 for c in co if c[0] > 0.29 or c[0] < -0.3) * 2 >= len(members):
                if hi[2] < 1.12:
                    # Mostly out past the hand, below the elbow: on the forearm, or on the hand if it sits on what that
                    # hand holds (the last pass, below).
                    out.update((i, "forearm_" + side) for i in members)
                else:
                    # Out there above the elbow (unless it finishes a held thing, below): a winged shoulder rides the
                    # chest and a headdress's outer plume the head, whole (bone_for would tear it at the neck).
                    out.update((i, "spine" if lo[2] < SHOULDERS else "head") for i in members)
            else:
                loose.update((i, NO_ARMS if on_leg else CLOTH) for i in members)
    # The small solids that finish a held thing go with it: within its box (a staff's end caps, a shield's rim), or
    # out at the side off the arm, within its box across and overlapping it in height (a spear's head, a bow's
    # limbs, a bayonet past a rifle's muzzle). Each piece joined is tested against in turn (a bow's next limb), by its
    # own box: one box grown over all of them would reach the body. A sleeve keeps only the arm's pieces within it
    # (an elbow's ball), not a sword's grip at the hip beside it. Then, from what is left, a scabbard takes its
    # sword's hilt (within its box across, overlapping it in height), not the hand that hangs by it; a long piece of
    # a held thing that only looked like a scabbard (a bow's limb above the hand) has gone to the hand by then.
    pending = list(others)

    def finish(pieces, joins):
        grew = True
        while grew:
            grew = False
            for entry in list(pending):
                members, lo, hi, on_arm, beside = entry
                for hlo, hhi, bone, item, kind in pieces:
                    across = all(hlo[k] - 0.04 <= lo[k] and hi[k] <= hhi[k] + 0.04 for k in range(2))
                    inside = across and hlo[2] - 0.04 <= lo[2] and hi[2] <= hhi[2] + 0.04
                    overlap = across and lo[2] <= hhi[2] + 0.04 and hi[2] >= hlo[2] - 0.04
                    if joins(kind, inside, overlap, on_arm, beside):
                        out.update((i, bone) for i in members)
                        item.extend(members)
                        pieces.append((lo, hi, bone, item, kind))
                        pending.remove(entry)
                        grew = True
                        break

    finish([entry for entry in fore if entry[4] != "worn"], lambda kind, inside, overlap, on_arm, beside:
           (kind == "hand" and (inside or (overlap and beside))) or (kind == "sleeve" and inside and on_arm))
    pending[:] = [entry for entry in pending if out.get(entry[0][0]) != "pelvis"]  # the scabbards themselves
    finish([entry for entry in fore if entry[4] == "worn" and out[entry[3][0]] == "pelvis"],
           lambda kind, inside, overlap, on_arm, beside: overlap and not on_arm)
    # Last, a small piece out past the hand that is still on the forearm but sits on what that hand holds (a shield's
    # boss standing off its face, the feathers hung from its rim, each piece joined tested against in turn) goes with
    # it: left on the forearm it would stay behind as the hand turns the shield.
    pieces = [(lo, hi, bone, item) for lo, hi, bone, item, kind in fore if kind == "hand"]
    grew = True
    while grew:
        grew = False
        for entry in list(pending):
            members, lo, hi, on_arm, beside = entry
            bone = out.get(members[0], "")
            if on_arm or not bone.startswith("forearm_"):
                continue
            for hlo, hhi, held_by, item in pieces:
                if held_by == "hand_" + bone[-1] and all(hlo[k] - ON_ITEM <= hi[k] and lo[k] <= hhi[k] + ON_ITEM for k in range(3)):
                    out.update((i, held_by) for i in members)
                    item.extend(members)
                    pieces.append((lo, hi, held_by, item))
                    pending.remove(entry)
                    grew = True
                    break
    return out, loose, [(lo, bone, item) for lo, hi, bone, item, kind in fore if kind == "hand"]


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
    # bone_for reads metres; the mesh is already in centimetres here. (Bound in centimetres, every vertex went to a
    # forearm or the head and the legs never bent, from #360 until 2026-10-20.)
    whole, loose, held = whole_parts(body.data)
    HELD[name] = {s: [i for lo, bone, item in held if bone == "hand_" + s for i in item] for s in ("r", "l")}
    for lo, bone, item in held:
        if lo[2] < 0.05:
            # A spear or staff stood on the ground slides up through the hand a little, as it is carried: the hips
            # walk lower than they stand, and a grounded butt would make the arm swing back to keep it out of the ground.
            for i in item:
                body.data.vertices[i].co.z += ITEM_LIFT * CM
    G = body.vertex_groups
    for v in body.data.vertices:
        x, y, z = v.co / CM
        bone = whole.get(v.index) or bone_for(x, y, z, **loose.get(v.index, {}))
        cloth = bone == "pelvis" and v.index not in whole  # (a scabbard rides the hips whole: a sword does not bend)
        if cloth and z < HEM:
            # A hem near the ground comes up a little: the hips dip as much as 8 cm in a long stride.
            v.co.z = (HEM - (HEM - z) * 0.3) * CM
        # Cloth below the hips (a tunic's skirt, a robe, a cloak's tail) rides the skirt bones, its front the front one
        # and its back the back one, so a stride swings it rather than walking out through it: none at the hips' axis,
        # all of it from SKIRT_FULL below (a thigh swung forward carries its front up and out there; above, the thigh's
        # own top shares the hips, _joint_weights).
        a = min(1.0, (HIPS - z) / SKIRT_FULL) if cloth and z < HIPS else 0.0
        if a > 0.0:
            f = _smooth(0.5 - (y - 0.04) / 0.12)  # the front reaches round the sides: the leg ahead swings furthest
            if a < 1.0:
                G["pelvis"].add([v.index], 1.0 - a, "REPLACE")
            for bname, w in (("skirt_f", a * f), ("skirt_b", a * (1.0 - f))):
                if w > 0.0:
                    G[bname].add([v.index], w, "REPLACE")
        elif v.index not in whole and bone.startswith(("thigh_", "shin_")):
            for bname, w in _joint_weights(bone, Vector((x, y, z))).items():
                G[bname].add([v.index], w, "REPLACE")
        else:
            G[bone].add([v.index], 1.0, "REPLACE")
    carried = {g.group for v in body.data.vertices for g in v.groups if g.weight > 0.0}
    bare = [b for b in ("pelvis", "spine", "head", "thigh_r", "thigh_l", "shin_r", "shin_l", "foot_r", "foot_l")
            if body.vertex_groups[b].index not in carried]
    if bare:
        raise RuntimeError("%s: no vertices on %s" % (name, ", ".join(bare)))
    # What each hand holds as it now is at rest, and so how the figure strikes (rig_actions.strike_class).
    rig_actions.ERRORS.pop(name, None)
    co = [v.co.copy() for v in body.data.vertices]
    try:
        ITEMS[name] = {s: rig_actions.item_axes(co, HELD[name][s], Vector(BONES["forearm_" + s][1]) * CM) if HELD[name][s] else None
                       for s in ("r", "l")}
        CLASSES[name] = rig_actions.strike_class(name, ITEMS[name])
    except Exception as e:
        # (as an error keying the one-shots, animate): a figure that fights loses only those, check_rigs.check_once
        # saying why.
        ITEMS[name], CLASSES[name] = {"r": None, "l": None}, None
        if name.startswith(rig_actions.FIGHTERS):
            rig_actions.ERRORS[name] = "%s: %s" % (type(e).__name__, e)
    item = ITEMS[name]["r"] or ITEMS[name]["l"]
    print("class %s: %s" % (name, "unknown (%s)" % rig_actions.ERRORS[name] if name in rig_actions.ERRORS else
                            "none (never fights)" if not CLASSES[name] else
                            "%s (%s)" % (CLASSES[name], "held %.2f m" % (item["length"] / CM) if item else "nothing held")))
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    arm.animation_data_create()
    animate(arm, body, name, HELD[name])
    return arm, body


# The speed (cm/s) each looping action was authored at: the game plays it at its own speed / this. Filled by animate.
SPEEDS = {}
# What each figure's hands hold (whole_parts): {name: {"r" or "l": vertex indices}}, for mocap.py and check_rigs.py.
HELD = {}
# The same as rig_actions.item_axes ({name: {"r" or "l": axes, or None}}), and how each figure strikes (None: it never
# fights), for rig_actions.py and check_rigs.py.
ITEMS = {}
CLASSES = {}


def animate(arm, body, name, held):
    """WalkSlow, Walk, Run and Idle from motion capture (mocap.py); without the clips, the procedural Walk and Idle
    below. Then, for a figure that fights, the one-shots (rig_actions.py). Every bone is keyed in every action (the
    hands, at rest in the loops, and the root: rig_actions.key_rest). `held`: the vertices each hand holds ({"r" or
    "l": indices})."""
    if mocap.available():
        rt = mocap.Retarget(arm, body, held)
        for action in mocap.CLIPS:
            mo = mocap.motion(action, FPS, rt.leg, rt.ankle)
            act = rt.key(action, mo)
            finish(act, mo.frames)
            key_skirt(arm, act)
            rig_actions.key_rest(arm, act)
            SPEEDS[action] = mo.speed
    else:
        print("rig_figures: no motion capture in %s, the procedural Walk and Idle (no WalkSlow or Run)" % mocap.MOCAP)
        for act in (walk(arm, held), idle(arm, name)):
            key_skirt(arm, act)
            rig_actions.key_rest(arm, act)
        SPEEDS.update(Walk=WALK_SPEED, Idle=0.0)
    if CLASSES.get(name):
        try:
            rig_actions.add(arm, body, CLASSES[name], ITEMS[name], held, key_skirt)
        except Exception as e:
            # (under another Blender, say): its loops stand, the one-shots keyed so far go, and check_rigs.check_once
            # reports why, so the figure is exported without them and the figures after it are still rigged.
            for action in [a for a in bpy.data.actions if a.name.split(".")[0] in rig_actions.ONCE]:
                bpy.data.actions.remove(action)
            rig_actions.ERRORS[name] = "%s: %s" % (type(e).__name__, e)


def key_skirt(arm, action, cycles=True):
    """Keys the skirt bones from the legs already keyed in `action` (their pitch from the hips, forward positive): the
    front swings forward as far as the thigh most ahead, the back back as far as the one most behind, so the leg
    that strides out stays inside a robe and lifts its hem. Looping (`cycles`), or not, for a one-shot."""
    curves = {(fc.data_path, fc.array_index): fc for fc in _fcurves(action)}
    B, P = arm.data.bones, arm.pose.bones
    n = int(round(action.frame_range[1]))
    pitch = {}
    for s in ("r", "l"):
        path = 'pose.bones["thigh_%s"].rotation_euler' % s
        R0 = B["thigh_" + s].matrix_local.to_3x3()
        d0 = R0 @ Vector((0, 1, 0))
        rest = math.atan2(-d0.y, -d0.z)
        pitch[s] = []
        for f in range(n + 1):
            e = Euler([curves[(path, k)].evaluate(f) if (path, k) in curves else 0.0 for k in range(3)], "XYZ")
            d = R0 @ e.to_matrix() @ Vector((0, 1, 0))
            pitch[s].append(math.atan2(-d.y, -d.z) - rest)
    arm.animation_data.action = action
    for name, pick in (("skirt_f", max), ("skirt_b", min)):
        R0 = B[name].matrix_local.to_3x3()
        pb = P[name]
        pb.rotation_mode = "XYZ"
        prev = None
        for f in range(n + 1):
            basis = R0.transposed() @ Matrix.Rotation(-pick(pitch["r"][f], pitch["l"][f], 0.0), 3, "X") @ R0
            prev = basis.to_euler("XYZ", prev) if prev else basis.to_euler("XYZ")
            pb.rotation_euler = prev
            pb.keyframe_insert("rotation_euler", frame=f)
    for fc in _fcurves(action):
        if cycles and "skirt_" in fc.data_path and not fc.modifiers:
            fc.modifiers.new("CYCLES")


def key(pb, frame, rx=0.0, ry=0.0, rz=0.0, loc=None):
    pb.rotation_mode = "XYZ"
    pb.rotation_euler = (math.radians(rx), math.radians(ry), math.radians(rz))
    pb.keyframe_insert("rotation_euler", frame=frame)
    if loc is not None:
        pb.location = loc
        pb.keyframe_insert("location", frame=frame)


# The fallback walk (no motion capture): a stride in WALK_FRAMES at FPS, each foot on the ground for STANCE of it and
# moving back at a steady WALK_SPEED while it is (so planted feet do not slide), then lifted and swung forward.
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


def walk(arm, held):
    """The legs by inverse kinematics along _foot (each planted foot holds still on the ground), the hips lowered
    as far as the planted legs need and bobbing, the arms swinging against the legs (an arm that holds something, in
    `held`, hardly at all, as mocap.py calms it)."""
    action = bpy.data.actions.new("Walk")
    arm.animation_data.action = action
    P = arm.pose.bones
    for b in P:
        b.rotation_mode = "XYZ"
    armed = any(held.values())
    swing = {s: 1.0 - (mocap.CALM_CARRY if held.get(s) else (mocap.CALM_OTHER if armed else mocap.CALM_FREE)) for s in ("r", "l")}
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
        # The hips bob with the stride, no sway: the legs are solved in their own plane, so a sway (a roll of the hips)
        # would slide the planted feet sideways.
        key(P["pelvis"], f, loc=(0, (hz - hip0) * CM, 0))  # bone space: Y runs along the bone, up
        bpy.context.view_layer.update()
        for side, (fwd, z) in feet.items():
            a = last[side] = _solve_leg(arm, side, (-fwd * CM, z * CM), last[side])  # forward is -Y
            P["thigh_" + side].rotation_euler = (math.radians(a[0]), 0, 0)
            P["shin_" + side].rotation_euler = (math.radians(a[1]), 0, 0)
            P["thigh_" + side].keyframe_insert("rotation_euler", frame=f)
            P["shin_" + side].keyframe_insert("rotation_euler", frame=f)
            key(P["foot_" + side], f, rx=-a[0] - a[1])  # the foot kept flat, so a planted shoe holds still on the ground
        # Arms against the legs: the right arm forward as the left leg is.
        key(P["upperarm_r"], f, rx=-16 * sw * swing["r"])
        key(P["upperarm_l"], f, rx=16 * sw * swing["l"])
        key(P["forearm_r"], f, rx=8 + 6 * sw * swing["r"])
        key(P["forearm_l"], f, rx=8 - 6 * sw * swing["l"])
        key(P["spine"], f, rz=-3 * sw, rx=3)
        key(P["head"], f, rz=1.5 * sw)
    finish(action, n)
    return action


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
        key(P["pelvis"], f, loc=(0, 0, 0))  # (the walk moves it)
        for b in ("thigh_r", "thigh_l", "shin_r", "shin_l", "foot_r", "foot_l", "forearm_r", "forearm_l"):
            key(P[b], f)
    finish(action, n)
    return action


def finish(action, n):
    action.use_fake_user = True
    action.frame_range = (0, n)
    for fc in _fcurves(action):
        fc.modifiers.new("CYCLES")


def _fcurves(action):
    """An action's curves: action.fcurves before Blender 5, the layers' strips' channel bags since."""
    if hasattr(action, "fcurves"):
        return list(action.fcurves)
    return [fc for layer in action.layers for strip in layer.strips for bag in getattr(strip, "channelbags", []) for fc in bag.fcurves]


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    build = os.path.abspath(args[0] if args else os.path.join("art", "build"))
    wanted = args[1:] or list(PEOPLE)
    out = os.path.join(build, "Rigged")
    os.makedirs(out, exist_ok=True)
    failed, without = [], []
    for name in wanted:
        arm, body = rig(name, PEOPLE[name])
        problems, line = check_rigs.check(arm, body, name, SPEEDS, FPS, HELD[name])
        print(line)
        try:
            once, once_line = check_rigs.check_once(arm, body, name, HELD[name], CLASSES[name], ITEMS[name])
        except Exception as error:  # (the once checks failing, under another Blender say, cost the one-shots only)
            once = ["its one-shots could not be checked (%s: %s)" % (type(error).__name__, error)]
            once_line = "check %s once (%s): FAIL (%s)" % (name, CLASSES[name] or "unknown", once[0])
        if once_line:
            print(once_line)
        path = os.path.join(out, "SK_%s.fbx" % name)
        if problems:
            # Not exported, and an older export goes too, so ue_import_rigged.py cannot bring back a stale rig.
            failed.append(name)
            if os.path.exists(path):
                os.remove(path)
            continue
        if once:
            # Its loops are sound: exported without the one-shots (the game falls back to standing and the old roll
            # where a take is missing) rather than taking a ruler off the street for a pose.
            without.append(name)
            arm.animation_data.action = bpy.data.actions.get("Idle")
            for take in rig_actions.ONCE:
                if bpy.data.actions.get(take):
                    bpy.data.actions.remove(bpy.data.actions[take])
            print("rig_figures: %s exported without its one-shot actions: %s" % (name, "; ".join(once)))
        bpy.ops.object.select_all(action="DESELECT")
        arm.select_set(True)
        body.select_set(True)
        bpy.context.view_layer.objects.active = arm
        # Every key kept, as rig_riders.py does: with Blender's default simplifying, which keys a take loses depends on
        # the pose the armature is in when the export starts (here a fall's last frame, as check_rigs.check_once leaves
        # it), and a fighter's loops came out up to 0.8 cm off the same loops exported from Idle. With every key kept
        # each take comes back exactly as it was keyed, whatever that pose.
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, mesh_smooth_type="FACE",
                                 colors_type="SRGB", apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", add_leaf_bones=False, bake_anim=True,
                                 bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                                 bake_anim_simplify_factor=0.0, primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL")
        print("rigged %s: %d vertices -> %s" % (name, len(body.data.vertices), path))
    # The speed each loop was authored at (the same for every figure: they share one skeleton), for the game.
    print("rig_figures: authored speeds " + ", ".join("%s %.1f cm/s" % (a, v) for a, v in SPEEDS.items()))
    # The one-shots' frames and blow (the same for every figure), for the game (SovGait.h's kSovStrikeImpact and
    # kSovStrikeLength, which PR B of Plan G step 5 adds).
    print(rig_actions.line())
    if without:
        print("rig_figures: exported without one-shot actions (failed check_rigs' once checks): " + ", ".join(without))
    if failed:
        print("rig_figures: not exported (any older SK_<Name>.fbx removed), failed check_rigs: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
