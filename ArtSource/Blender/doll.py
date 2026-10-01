"""Windup doll: skeletal mesh + animations + scatter parts + key.  Run inside Blender."""
import bpy, math, os, sys
from mathutils import Vector, Matrix, Quaternion
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Blender")
import importlib, crank_kit
importlib.reload(crank_kit)
from crank_kit import *

FPS = 30
TIN = "#e6dfcc"
TIN_DARK = "#6b675f"
WHITE = "#ffffff"
GLOVE = "#f3eee2"
BOOT = "#3a2a21"
BRASS = "#d8a446"
PAINT_W = "#ffffff"      # multiplied by player color
PAINT_D = "#9a9a9a"      # darker paint areas
CHEEK = "#f28b82"
BLACK = "#121212"
MOUTH = "#5a1d1d"

SIDES = {"l": 1, "r": -1}


# ------------------------------------------------------------------------------------------
# Geometry (shared between skinned mesh and scatter parts)

def build_head(b, o=Vector((0, 0, 0)), group="head"):
    b.cylinder(tuple(o + Vector((0, 0, 0.712))), 0.052, 0.035, TIN_DARK, "Metal", verts=14, group=group)
    b.lathe([(0.0, 0.725), (0.118, 0.725), (0.134, 0.738), (0.141, 0.80), (0.141, 0.885), (0.13, 0.932), (0.10, 0.962), (0.05, 0.978), (0.0, 0.981)],
            TIN, "Metal", seg=22, loc=tuple(o), group=group)
    # cap + antenna ball (painted)
    b.cylinder(tuple(o + Vector((0, 0, 0.985))), 0.062, 0.028, PAINT_W, "Paint", verts=16, bevel=0.006, group=group)
    b.cylinder(tuple(o + Vector((0, 0, 1.005))), 0.012, 0.03, TIN_DARK, "Metal", verts=8, group=group)
    b.sphere(tuple(o + Vector((0, 0, 1.025))), 0.02, BRASS, "Brass", subdiv=1, group=group)
    for s in (1, -1):
        # eyes
        b.cylinder(tuple(o + Vector((0.131, 0.05 * s, 0.862))), 0.033, 0.012, WHITE, "Gloss", verts=16, rot=(0, 90, 0), group=group)
        b.cylinder(tuple(o + Vector((0.138, 0.047 * s, 0.858))), 0.019, 0.012, BLACK, "Gloss", verts=12, rot=(0, 90, 0), group=group)
        b.sphere(tuple(o + Vector((0.145, 0.040 * s, 0.868))), 0.006, WHITE, "Gloss", subdiv=1, group=group)
        # cheeks
        b.sphere(tuple(o + Vector((0.124, 0.088 * s, 0.814))), 0.022, CHEEK, "Matte", subdiv=1, scale=(0.4, 1, 0.8), group=group)
        # ear bolts
        b.cylinder(tuple(o + Vector((0, 0.143 * s, 0.85))), 0.026, 0.02, BRASS, "Brass", verts=6, rot=(90, 0, 0), group=group)
    # mouth (little smile slot)
    b.box(tuple(o + Vector((0.136, 0, 0.787))), (0.012, 0.052, 0.012), MOUTH, "Matte", bevel=0.004, group=group, segments=1)


def build_torso(b, o=Vector((0, 0, 0)), pelvis_group="pelvis", spine_group="spine"):
    b.cylinder(tuple(o + Vector((0, 0, 0.368))), 0.128, 0.07, PAINT_D, "Paint", verts=18, bevel=0.01, group=pelvis_group)
    b.lathe([(0.0, 0.40), (0.138, 0.40), (0.154, 0.44), (0.161, 0.55), (0.156, 0.65), (0.134, 0.70), (0.0, 0.702)],
            PAINT_W, "Paint", seg=22, loc=tuple(o), group=spine_group)
    b.cylinder(tuple(o + Vector((0, 0, 0.412))), 0.158, 0.026, "#2b2420", "Gloss", verts=22, group=spine_group)
    b.box(tuple(o + Vector((0.158, 0, 0.415))), (0.014, 0.05, 0.034), BRASS, "Brass", bevel=0.004, group=spine_group, segments=1)
    b.box(tuple(o + Vector((0.152, 0, 0.58))), (0.024, 0.15, 0.13), BRASS, "Brass", bevel=0.01, group=spine_group)
    for y in (-0.06, 0.06):
        for z in (0.528, 0.632):
            b.sphere(tuple(o + Vector((0.166, y, z))), 0.009, "#8a6a2a", "Brass", subdiv=1, group=spine_group)
    # three little buttons
    for i, z in enumerate((0.555, 0.58, 0.605)):
        b.sphere(tuple(o + Vector((0.168, 0, z))), 0.011, ["#d23b2b", "#2a6fd6", "#f2c12e"][i], "Gloss", subdiv=1, group=spine_group)
    # key socket on the back
    b.cylinder(tuple(o + Vector((-0.158, 0, 0.58))), 0.028, 0.025, BRASS, "Brass", verts=12, rot=(0, 90, 0), group=spine_group)


def build_arm(b, side, o=Vector((0, 0, 0)), groups=None):
    s = SIDES[side]
    g_up, g_low, g_hand = groups or ("upperarm_" + side, "lowerarm_" + side, "hand_" + side)
    b.sphere(tuple(o + Vector((0, 0.168 * s, 0.655))), 0.05, TIN, "Metal", subdiv=2, group=g_up)
    b.cylinder(tuple(o + Vector((0, 0.19 * s, 0.585))), 0.036, 0.12, PAINT_W, "Paint", verts=12, group=g_up)
    b.sphere(tuple(o + Vector((0, 0.19 * s, 0.52))), 0.034, TIN_DARK, "Metal", subdiv=1, group=g_low)
    b.cylinder(tuple(o + Vector((0, 0.19 * s, 0.465))), 0.032, 0.10, TIN, "Metal", verts=12, group=g_low)
    b.sphere(tuple(o + Vector((0.006, 0.19 * s, 0.392))), 0.045, GLOVE, "Gloss", subdiv=2, scale=(1.0, 0.82, 1.08), group=g_hand)


def build_leg(b, side, o=Vector((0, 0, 0)), groups=None):
    s = SIDES[side]
    g_th, g_calf, g_foot = groups or ("thigh_" + side, "calf_" + side, "foot_" + side)
    b.cylinder(tuple(o + Vector((0, 0.075 * s, 0.268))), 0.042, 0.14, PAINT_D, "Paint", verts=12, group=g_th)
    b.sphere(tuple(o + Vector((0, 0.075 * s, 0.19))), 0.039, TIN_DARK, "Metal", subdiv=1, group=g_calf)
    b.cylinder(tuple(o + Vector((0, 0.075 * s, 0.125))), 0.034, 0.12, TIN, "Metal", verts=12, group=g_calf)
    b.box(tuple(o + Vector((0.022, 0.075 * s, 0.036))), (0.13, 0.08, 0.072), BOOT, "Gloss", bevel=0.018, group=g_foot)
    b.box(tuple(o + Vector((0.022, 0.075 * s, 0.006))), (0.135, 0.085, 0.012), "#1b1512", "Matte", bevel=0.004, group=g_foot, segments=1)


def build_key(b, o=Vector((0, 0, 0)), group=None):
    """Butterfly windup key: shaft along +X from origin."""
    b.cylinder(tuple(o + Vector((0.05, 0, 0))), 0.014, 0.10, BRASS, "Brass", verts=10, rot=(0, 90, 0), group=group)
    b.cylinder(tuple(o + Vector((0.105, 0, 0))), 0.024, 0.02, BRASS, "Brass", verts=10, rot=(0, 90, 0), group=group)
    for s in (1, -1):
        # two rounded lobes (flattened cylinders) side by side in the Y-Z plane
        b.cylinder(tuple(o + Vector((0.115, 0.06 * s, 0))), 0.055, 0.016, BRASS, "Brass", verts=16, rot=(0, 90, 0), group=group, bevel=0.004)
        b.cylinder(tuple(o + Vector((0.115, 0.06 * s, 0))), 0.026, 0.02, "#8a6a2a", "Brass", verts=12, rot=(0, 90, 0), group=group)


# ------------------------------------------------------------------------------------------
# Armature

BONES = [
    # name, head, tail, parent
    ("root", (0, 0, 0), (0, 0, 0.1), None),
    ("pelvis", (0, 0, 0.34), (0, 0, 0.40), "root"),
    ("spine", (0, 0, 0.40), (0, 0, 0.70), "pelvis"),
    ("head", (0, 0, 0.70), (0, 0, 0.98), "spine"),
]
for side, s in SIDES.items():
    BONES += [
        ("upperarm_" + side, (0, 0.17 * s, 0.655), (0, 0.19 * s, 0.52), "spine"),
        ("lowerarm_" + side, (0, 0.19 * s, 0.52), (0, 0.19 * s, 0.42), "upperarm_" + side),
        ("hand_" + side, (0, 0.19 * s, 0.42), (0, 0.19 * s, 0.35), "lowerarm_" + side),
        ("thigh_" + side, (0, 0.075 * s, 0.34), (0, 0.075 * s, 0.19), "pelvis"),
        ("calf_" + side, (0, 0.075 * s, 0.19), (0, 0.075 * s, 0.06), "thigh_" + side),
        ("foot_" + side, (0, 0.075 * s, 0.06), (0.09, 0.075 * s, 0.03), "calf_" + side),
    ]


def build_armature():
    arm_data = bpy.data.armatures.new("DollArmature")
    arm_obj = bpy.data.objects.new("Armature", arm_data)
    bpy.context.scene.collection.objects.link(arm_obj)
    select_only([arm_obj])
    bpy.ops.object.mode_set(mode='EDIT')
    for name, head, tail, parent in BONES:
        eb = arm_data.edit_bones.new(name)
        eb.head = head
        eb.tail = tail
        eb.roll = 0.0
        if parent:
            eb.parent = arm_data.edit_bones[parent]
            eb.use_connect = False
    bpy.ops.object.mode_set(mode='OBJECT')
    return arm_obj


# ------------------------------------------------------------------------------------------
# Animation helpers: rotations are given around ARMATURE (rest pose) axes.

AX = {"X": Vector((1, 0, 0)), "Y": Vector((0, 1, 0)), "Z": Vector((0, 0, 1))}


def local_quat(arm_obj, bone, rots):
    """rots: list of (axis_letter_or_vec, degrees) applied in order (armature rest space)."""
    rest = arm_obj.data.bones[bone].matrix_local.to_3x3()
    inv = rest.inverted()
    q = Quaternion()
    for axis, deg in rots:
        a = AX[axis] if isinstance(axis, str) else Vector(axis)
        la = (inv @ a).normalized()
        q = Quaternion(la, math.radians(deg)) @ q
    return q


def key_pose(arm_obj, frame, pose):
    """pose: {bone: {"r": [(axis, deg), ...], "t": (x, y, z) armature-space offset}}"""
    for pb in arm_obj.pose.bones:
        spec = pose.get(pb.name, {})
        pb.rotation_mode = 'QUATERNION'
        pb.rotation_quaternion = local_quat(arm_obj, pb.name, spec.get("r", []))
        t = spec.get("t", (0, 0, 0))
        rest = arm_obj.data.bones[pb.name].matrix_local.to_3x3()
        pb.location = rest.inverted() @ Vector(t)
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        pb.keyframe_insert("location", frame=frame)


def make_action(arm_obj, name, keys, loop=True):
    """keys: list of (frame, pose). Creates and returns the action."""
    act = bpy.data.actions.new("A_Doll_" + name)
    arm_obj.animation_data_create()
    arm_obj.animation_data.action = act
    for frame, pose in keys:
        key_pose(arm_obj, frame, pose)
    if loop:
        key_pose(arm_obj, keys[-1][0] + (keys[1][0] - keys[0][0] if len(keys) > 1 else 10), keys[0][1])
    # Blender 5 layered actions: fcurves live in channelbags; set interpolation if possible
    try:
        for layer in act.layers:
            for strip in layer.strips:
                for cb in strip.channelbags:
                    for fc in cb.fcurves:
                        for kp in fc.keyframe_points:
                            kp.interpolation = 'BEZIER'
    except Exception:
        pass
    act.use_fake_user = True
    return act


def cyc(t, period):
    return math.sin(2 * math.pi * t / period)


def locomotion_keys(period_frames, swing, arm_swing, lean, bob, knee=12, waddle=4, steps=8):
    keys = []
    for i in range(steps):
        f = i * period_frames / steps
        ph = 2 * math.pi * i / steps
        s = math.sin(ph)
        c = math.cos(ph)
        lift_l = max(0.0, -math.sin(ph))   # knee bends while leg swings forward
        lift_r = max(0.0, math.sin(ph))
        pose = {
            "pelvis": {"r": [("X", waddle * s)], "t": (0, 0, bob * abs(c) - bob * 0.5)},
            "spine": {"r": [("Y", lean), ("Z", -4 * s)]},
            "head": {"r": [("Y", -lean * 0.4), ("X", -waddle * 0.6 * s)]},
            "thigh_l": {"r": [("Y", swing * s)]},
            "thigh_r": {"r": [("Y", -swing * s)]},
            "calf_l": {"r": [("Y", knee * lift_l)]},
            "calf_r": {"r": [("Y", knee * lift_r)]},
            "upperarm_l": {"r": [("Y", -arm_swing * s), ("X", 8)]},
            "upperarm_r": {"r": [("Y", arm_swing * s), ("X", -8)]},
            "lowerarm_l": {"r": [("Y", -15)]},
            "lowerarm_r": {"r": [("Y", -15)]},
        }
        keys.append((int(round(f)) + 1, pose))
    return keys


def build_animations(arm_obj):
    acts = []
    # --- Idle: gentle breathing, tick-tock head
    idle = []
    for i in range(4):
        s = math.sin(2 * math.pi * i / 4)
        idle.append((1 + i * 15, {
            "pelvis": {"t": (0, 0, 0.004 * s)},
            "spine": {"r": [("Y", 1.5 * s)]},
            "head": {"r": [("X", 4 * math.cos(2 * math.pi * i / 4))]},
            "upperarm_l": {"r": [("X", 6 + 2 * s), ("Y", -3)]},
            "upperarm_r": {"r": [("X", -6 - 2 * s), ("Y", -3)]},
            "lowerarm_l": {"r": [("Y", -10)]},
            "lowerarm_r": {"r": [("Y", -10)]},
        }))
    acts.append(make_action(arm_obj, "Idle", idle))

    acts.append(make_action(arm_obj, "Walk", locomotion_keys(24, 24, 26, 2, 0.012, knee=14, waddle=6)))
    acts.append(make_action(arm_obj, "Run", locomotion_keys(18, 38, 42, 7, 0.02, knee=28, waddle=4)))
    acts.append(make_action(arm_obj, "Sprint", locomotion_keys(14, 52, 65, 13, 0.028, knee=40, waddle=3)))

    # --- Crawl: prone army crawl, arms paddle, legs drag
    crawl = []
    for i in range(8):
        ph = 2 * math.pi * i / 8
        s = math.sin(ph)
        crawl.append((1 + i * 4, {
            "pelvis": {"r": [("Y", 88), ("X", 4 * s)], "t": (-0.22, 0, -0.235)},
            "head": {"r": [("Y", -55)]},
            "upperarm_l": {"r": [("Y", -150 + 45 * s), ("X", 10)]},
            "upperarm_r": {"r": [("Y", -150 - 45 * s), ("X", -10)]},
            "lowerarm_l": {"r": [("Y", -25 - 20 * max(0, s))]},
            "lowerarm_r": {"r": [("Y", -25 - 20 * max(0, -s))]},
            "thigh_l": {"r": [("Y", 6 * s), ("X", 6)]},
            "thigh_r": {"r": [("Y", -6 * s), ("X", -6)]},
            "foot_l": {"r": [("Y", 40)]},
            "foot_r": {"r": [("Y", 40)]},
        }))
    acts.append(make_action(arm_obj, "Crawl", crawl))

    # --- Fall (in the air): arms flail up
    fall = []
    for i in range(4):
        s = math.sin(2 * math.pi * i / 4)
        fall.append((1 + i * 5, {
            "spine": {"r": [("Y", -6)]},
            "upperarm_l": {"r": [("X", 120 + 15 * s)]},
            "upperarm_r": {"r": [("X", -120 + 15 * s)]},
            "lowerarm_l": {"r": [("Y", -30)]},
            "lowerarm_r": {"r": [("Y", -30)]},
            "thigh_l": {"r": [("Y", -25 + 10 * s)]},
            "thigh_r": {"r": [("Y", 10 - 10 * s)]},
            "calf_l": {"r": [("Y", 30)]},
            "calf_r": {"r": [("Y", 20)]},
        }))
    acts.append(make_action(arm_obj, "Fall", fall))

    # --- Fly (launched): superman
    fly = []
    for i in range(4):
        s = math.sin(2 * math.pi * i / 4)
        fly.append((1 + i * 4, {
            "pelvis": {"r": [("Y", 78), ("X", 3 * s)]},
            "head": {"r": [("Y", -60)]},
            "upperarm_l": {"r": [("Y", -168), ("X", 12 + 4 * s)]},
            "upperarm_r": {"r": [("Y", -168), ("X", -12 - 4 * s)]},
            "thigh_l": {"r": [("X", 8), ("Y", 6 * s)]},
            "thigh_r": {"r": [("X", -8), ("Y", -6 * s)]},
            "foot_l": {"r": [("Y", 35)]},
            "foot_r": {"r": [("Y", 35)]},
        }))
    acts.append(make_action(arm_obj, "Fly", fly))

    # --- Wind (winder): arms forward on the key, cranking circle
    wind = []
    for i in range(8):
        ph = 2 * math.pi * i / 8
        s, c = math.sin(ph), math.cos(ph)
        wind.append((1 + i * 2, {
            "spine": {"r": [("Y", 8), ("X", 3 * s)]},
            "head": {"r": [("Y", 10)]},
            "upperarm_l": {"r": [("Y", -82 + 10 * s), ("Z", -18 + 8 * c)]},
            "upperarm_r": {"r": [("Y", -82 + 10 * s), ("Z", 18 + 8 * c)]},
            "lowerarm_l": {"r": [("Y", -22)]},
            "lowerarm_r": {"r": [("Y", -22)]},
            "thigh_l": {"r": [("Y", -10)]},
            "thigh_r": {"r": [("Y", 12)]},
        }))
    acts.append(make_action(arm_obj, "Wind", wind))

    # --- Pull (slingshot draw): lean back, braced
    pull = [(1, {
        "pelvis": {"t": (-0.04, 0, -0.02)},
        "spine": {"r": [("Y", -18)]},
        "head": {"r": [("Y", 12)]},
        "upperarm_l": {"r": [("Y", -92), ("Z", -14)]},
        "upperarm_r": {"r": [("Y", -92), ("Z", 14)]},
        "lowerarm_l": {"r": [("Y", -8)]},
        "lowerarm_r": {"r": [("Y", -8)]},
        "thigh_l": {"r": [("Y", -28)]},
        "thigh_r": {"r": [("Y", 22)]},
        "calf_l": {"r": [("Y", 8)]},
        "calf_r": {"r": [("Y", 18)]},
    }), (16, {
        "pelvis": {"t": (-0.07, 0, -0.035)},
        "spine": {"r": [("Y", -26)]},
        "head": {"r": [("Y", 16)]},
        "upperarm_l": {"r": [("Y", -96), ("Z", -12)]},
        "upperarm_r": {"r": [("Y", -96), ("Z", 12)]},
        "thigh_l": {"r": [("Y", -36)]},
        "thigh_r": {"r": [("Y", 26)]},
        "calf_l": {"r": [("Y", 10)]},
        "calf_r": {"r": [("Y", 28)]},
    })]
    acts.append(make_action(arm_obj, "Pull", pull, loop=False))

    # --- Wound (being wound): arms out, happy wiggle
    wound = []
    for i in range(4):
        s = math.sin(2 * math.pi * i / 4)
        wound.append((1 + i * 6, {
            "pelvis": {"t": (0, 0, 0.006 * abs(s))},
            "spine": {"r": [("X", 4 * s)]},
            "head": {"r": [("Z", 8 * s), ("Y", -6)]},
            "upperarm_l": {"r": [("X", 32 + 6 * s)]},
            "upperarm_r": {"r": [("X", -32 + 6 * s)]},
            "lowerarm_l": {"r": [("Y", -40)]},
            "lowerarm_r": {"r": [("Y", -40)]},
        }))
    acts.append(make_action(arm_obj, "Wound", wound))

    # --- Carry (holding something in front)
    carry = []
    for i in range(2):
        s = 1 if i == 0 else -1
        carry.append((1 + i * 12, {
            "upperarm_l": {"r": [("Y", -78 + 2 * s), ("Z", -10)]},
            "upperarm_r": {"r": [("Y", -78 - 2 * s), ("Z", 10)]},
            "lowerarm_l": {"r": [("Y", -40)]},
            "lowerarm_r": {"r": [("Y", -40)]},
        }))
    acts.append(make_action(arm_obj, "Carry", carry))

    # --- Carried (draped like a sack)
    carried = []
    for i in range(4):
        s = math.sin(2 * math.pi * i / 4)
        carried.append((1 + i * 8, {
            "pelvis": {"r": [("Y", 90)], "t": (-0.28, 0, -0.26)},
            "head": {"r": [("Y", -10 + 6 * s)]},
            "upperarm_l": {"r": [("Y", -92 + 10 * s)]},
            "upperarm_r": {"r": [("Y", -88 - 10 * s)]},
            "thigh_l": {"r": [("Y", -85 - 8 * s)]},
            "thigh_r": {"r": [("Y", -95 + 8 * s)]},
        }))
    acts.append(make_action(arm_obj, "Carried", carried))

    # --- Reach poses (single frame)
    acts.append(make_action(arm_obj, "ReachL", [(1, {"upperarm_l": {"r": [("Y", -84), ("Z", -12)]}, "lowerarm_l": {"r": [("Y", -6)]}})], loop=False))
    acts.append(make_action(arm_obj, "ReachR", [(1, {"upperarm_r": {"r": [("Y", -84), ("Z", 12)]}, "lowerarm_r": {"r": [("Y", -6)]}})], loop=False))

    # --- Splat: flattened X pose
    acts.append(make_action(arm_obj, "Splat", [(1, {
        "head": {"r": [("Y", -15)]},
        "upperarm_l": {"r": [("X", 115)]},
        "upperarm_r": {"r": [("X", -115)]},
        "thigh_l": {"r": [("X", 22)]},
        "thigh_r": {"r": [("X", -22)]},
    })], loop=False))

    # --- Dizzy
    dizzy = []
    for i in range(8):
        ph = 2 * math.pi * i / 8
        s, c = math.sin(ph), math.cos(ph)
        dizzy.append((1 + i * 4, {
            "spine": {"r": [("X", 8 * s), ("Y", 8 * c)]},
            "head": {"r": [("X", 14 * c), ("Y", 10 * s)]},
            "upperarm_l": {"r": [("X", 28 + 8 * s)]},
            "upperarm_r": {"r": [("X", -28 + 8 * c)]},
            "lowerarm_l": {"r": [("Y", -20)]},
            "lowerarm_r": {"r": [("Y", -20)]},
            "thigh_l": {"r": [("X", 6)]},
            "thigh_r": {"r": [("X", -6)]},
        }))
    acts.append(make_action(arm_obj, "Dizzy", dizzy))

    # --- Frozen (stiff toy on a shelf)
    acts.append(make_action(arm_obj, "Frozen", [(1, {
        "upperarm_l": {"r": [("Y", -35), ("X", 6)]},
        "upperarm_r": {"r": [("Y", -35), ("X", -6)]},
        "lowerarm_l": {"r": [("Y", -20)]},
        "lowerarm_r": {"r": [("Y", -20)]},
        "head": {"r": [("X", 6)]},
    })], loop=False))

    # --- Emotes
    wave = []
    for i in range(6):
        s = math.sin(2 * math.pi * i / 3)
        wave.append((1 + i * 6, {
            "spine": {"r": [("X", -4)]},
            "head": {"r": [("X", -8), ("Z", 6 * s)]},
            "upperarm_r": {"r": [("X", -150)]},
            "lowerarm_r": {"r": [("X", -28 * s)]},
            "upperarm_l": {"r": [("X", 8)]},
        }))
    acts.append(make_action(arm_obj, "Emote_Wave", wave))

    dance = []
    for i in range(8):
        s = math.sin(2 * math.pi * i / 4)
        up_l = max(0.0, s)
        up_r = max(0.0, -s)
        dance.append((1 + i * 5, {
            "pelvis": {"t": (0, 0, 0.02 * abs(s)), "r": [("X", 8 * s)]},
            "spine": {"r": [("X", -10 * s)]},
            "head": {"r": [("X", 12 * s)]},
            "thigh_l": {"r": [("Y", -55 * up_l)]},
            "calf_l": {"r": [("Y", 60 * up_l)]},
            "thigh_r": {"r": [("Y", -55 * up_r)]},
            "calf_r": {"r": [("Y", 60 * up_r)]},
            "upperarm_l": {"r": [("X", 70 + 20 * s)]},
            "upperarm_r": {"r": [("X", -70 + 20 * s)]},
            "lowerarm_l": {"r": [("X", 60)]},
            "lowerarm_r": {"r": [("X", -60)]},
        }))
    acts.append(make_action(arm_obj, "Emote_Dance", dance))

    cheer = []
    for i in range(4):
        s = math.sin(2 * math.pi * i / 2)
        cheer.append((1 + i * 5, {
            "pelvis": {"t": (0, 0, 0.035 * max(0, s))},
            "spine": {"r": [("Y", -6)]},
            "head": {"r": [("Y", -14)]},
            "upperarm_l": {"r": [("X", 158 + 8 * s)]},
            "upperarm_r": {"r": [("X", -158 - 8 * s)]},
            "calf_l": {"r": [("Y", 15 * max(0, -s))]},
            "calf_r": {"r": [("Y", 15 * max(0, -s))]},
        }))
    acts.append(make_action(arm_obj, "Emote_Cheer", cheer))

    bow = [(1, {}), (12, {
        "spine": {"r": [("Y", 42)]},
        "head": {"r": [("Y", 12)]},
        "upperarm_r": {"r": [("Y", -40), ("Z", 40)]},
        "lowerarm_r": {"r": [("Z", 70)]},
        "upperarm_l": {"r": [("Y", 30)]},
    }), (40, {
        "spine": {"r": [("Y", 42)]},
        "head": {"r": [("Y", 12)]},
        "upperarm_r": {"r": [("Y", -40), ("Z", 40)]},
        "lowerarm_r": {"r": [("Z", 70)]},
        "upperarm_l": {"r": [("Y", 30)]},
    }), (55, {})]
    acts.append(make_action(arm_obj, "Emote_Bow", bow, loop=False))
    return acts


# ------------------------------------------------------------------------------------------

def build_doll():
    clear_scene()
    bpy.context.scene.render.fps = FPS

    b = Builder("SK_Doll")
    build_head(b)
    build_torso(b)
    for side in ("l", "r"):
        build_arm(b, side)
        build_leg(b, side)
    mesh_obj = b.finish()
    new_material_slots_order(mesh_obj, ["Paint", "Metal", "Gloss", "Matte", "Brass"])

    arm_obj = build_armature()
    mesh_obj.parent = arm_obj
    mod = mesh_obj.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm_obj
    # make sure every bone has a vertex group (empty ones are fine)
    for name, *_ in BONES:
        if name not in mesh_obj.vertex_groups:
            mesh_obj.vertex_groups.new(name=name)
    return arm_obj, mesh_obj


def bake_scale(arm_obj, mesh_obj, acts, factor=100.0):
    """Unreal's legacy FBX importer ignores the uniform scale option for skeletal meshes:
    bake centimeters into the armature, mesh and location keys instead."""
    arm_obj.animation_data.action = None
    for pb in arm_obj.pose.bones:
        pb.rotation_quaternion = Quaternion()
        pb.location = Vector()
    arm_obj.scale = (factor, factor, factor)
    select_only([arm_obj, mesh_obj])
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for act in acts:
        try:
            curves = [fc for layer in act.layers for strip in layer.strips for cb in strip.channelbags for fc in cb.fcurves]
        except Exception:
            curves = list(act.fcurves)
        for fc in curves:
            if fc.data_path.endswith(".location"):
                for kp in fc.keyframe_points:
                    kp.co[1] *= factor
                    kp.handle_left[1] *= factor
                    kp.handle_right[1] *= factor


def export_doll(arm_obj, mesh_obj, acts):
    out = os.path.join(EXPORT_ROOT, "Doll")
    os.makedirs(out, exist_ok=True)
    scene = bpy.context.scene
    if arm_obj.data.bones["spine"].length < 1.0:
        bake_scale(arm_obj, mesh_obj, acts)

    # Mesh + skeleton in bind pose
    arm_obj.animation_data.action = None
    for pb in arm_obj.pose.bones:
        pb.rotation_quaternion = Quaternion()
        pb.location = Vector()
    select_only([arm_obj, mesh_obj])
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, "SK_Doll.fbx"), use_selection=True,
                             object_types={'ARMATURE', 'MESH'}, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                             axis_forward='-Z', axis_up='Y', add_leaf_bones=False, use_armature_deform_only=False,
                             primary_bone_axis='Y', secondary_bone_axis='X', armature_nodetype='NULL',
                             mesh_smooth_type='FACE', bake_anim=False, colors_type='SRGB')

    # One FBX per action (armature + mesh so the importer sees a skeletal mesh)
    for act in acts:
        arm_obj.animation_data.action = act
        try:
            arm_obj.animation_data.action_slot = act.slots[0]
        except Exception:
            pass
        fr = act.frame_range
        scene.frame_start = int(fr[0])
        scene.frame_end = max(int(fr[1]), int(fr[0]) + 1)
        select_only([arm_obj])
        bpy.ops.export_scene.fbx(filepath=os.path.join(out, act.name + ".fbx"), use_selection=True,
                                 object_types={'ARMATURE'}, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                                 axis_forward='-Z', axis_up='Y', add_leaf_bones=False, use_armature_deform_only=False,
                                 primary_bone_axis='Y', secondary_bone_axis='X', armature_nodetype='NULL',
                                 mesh_smooth_type='FACE', bake_anim=True, bake_anim_use_all_bones=True,
                                 bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                                 bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
                                 colors_type='SRGB')
    arm_obj.animation_data.action = acts[0]


def build_parts():
    """Static meshes for the scatter parts + the key."""
    out = []
    # head: origin at head center
    b = Builder("SM_DollPart_Head")
    build_head(b, o=Vector((0, 0, -0.85)))
    o = b.finish(); new_material_slots_order(o, ["Paint", "Metal", "Gloss", "Matte", "Brass"]); out.append(o)
    # torso
    b = Builder("SM_DollPart_Torso")
    build_torso(b, o=Vector((0, 0, -0.53)))
    o = b.finish(); new_material_slots_order(o, ["Paint", "Metal", "Gloss", "Matte", "Brass"]); out.append(o)
    # arm (left arm moved to origin)
    b = Builder("SM_DollPart_Arm")
    build_arm(b, "l", o=Vector((0, -0.18, -0.52)), groups=("a", "a", "a"))
    o = b.finish(); new_material_slots_order(o, ["Paint", "Metal", "Gloss", "Matte", "Brass"]); out.append(o)
    # leg
    b = Builder("SM_DollPart_Leg")
    build_leg(b, "l", o=Vector((0, -0.075, -0.17)), groups=("a", "a", "a"))
    o = b.finish(); new_material_slots_order(o, ["Paint", "Metal", "Gloss", "Matte", "Brass"]); out.append(o)
    # key
    b = Builder("SM_Doll_Key")
    build_key(b)
    o = b.finish(); out.append(o)
    for obj in out:
        obj.location.x += 0  # keep at origin for export
        export_static(obj, obj.name + ".fbx", sub="Doll")
    return out


if __name__ == "__main__":
    arm, mesh = build_doll()
    acts = build_animations(arm)
    export_doll(arm, mesh, acts)
    print("doll exported", [a.name for a in acts])
