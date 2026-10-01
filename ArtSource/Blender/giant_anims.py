"""Keyframed animation clips for the giant cymbal monkey boss (Tripo model, Mixamo rig, Blender scene "MonkeyFBX").

Poses are described with a few readable parameters (degrees / centimeters) and applied as armature-space rotations
around each bone head, parents first, then keyed as local quaternions. Rig facing: front = -Y, left = +X, up = +Z.
Clips are in-place (the boss actor moves itself) and exported one FBX per action next to SK_GiantMonkey.fbx.
"""
import bpy, math, os
from mathutils import Matrix, Vector

OUT = r"D:\Game\CRANK\ArtSource\Export\Giant\Anims"
FPS = 30

DEFAULTS = dict(hz=0.0, hy=0.0, hroll=0.0, hyaw=0.0, hpitch=0.0, spine=0.0, sroll=0.0, head=0.0, hyw=0.0, hrl=0.0,
                oL=0.0, oR=0.0, dL=0.0, dR=0.0, fL=0.0, fR=0.0, tL=0.0, tR=0.0, kL=0.0, kR=0.0)

SLUMP = dict(spine=26, dL=55, dR=55, oL=-8, oR=-8, head=22, hz=-14)
CROUCH = dict(hz=-60, tL=35, kL=60, tR=35, kR=60, spine=14, oL=14, oR=14, dL=-12, dR=-12, head=-6)


def P(base=None, **kw):
    d = dict(DEFAULTS)
    if base:
        d.update(base)
    d.update(kw)
    return d


def mirror(p):
    """Swap left / right for the mirrored throw."""
    m = dict(p)
    for a, b in (("oL", "oR"), ("dL", "dR"), ("fL", "fR"), ("tL", "tR"), ("kL", "kR")):
        m[a], m[b] = p[b], p[a]
    m["hroll"] = -p["hroll"]
    m["sroll"] = -p["sroll"]
    m["hyw"] = -p["hyw"]
    m["hrl"] = -p["hrl"]
    m["hyaw"] = -p["hyaw"]
    return m


# name: (frames, loop, [(frame, params)])
CLIPS = {
    "A_Giant_Idle": (60, True, [
        (0, P(oL=4, oR=4, spine=3)),
        (15, P(hz=8, oL=-2, oR=-2, hroll=2, head=3, spine=4)),
        (30, P(oL=4, oR=4, spine=3)),
        (45, P(hz=8, oL=-2, oR=-2, hroll=-2, head=-2, spine=4)),
        (60, P(oL=4, oR=4, spine=3)),
    ]),
    "A_Giant_Walk": (30, True, [
        (0, P(hroll=7, tL=24, kL=28, tR=-8, kR=6, hz=4, oL=12, oR=-6, spine=6, head=2)),
        (8, P(hz=16, tL=8, kL=12, hroll=0, oL=3, oR=3, spine=5)),
        (15, P(hroll=-7, tR=24, kR=28, tL=-8, kL=6, hz=4, oL=-6, oR=12, spine=6, head=2)),
        (23, P(hz=16, tR=8, kR=12, hroll=0, oL=3, oR=3, spine=5)),
        (30, P(hroll=7, tL=24, kL=28, tR=-8, kR=6, hz=4, oL=12, oR=-6, spine=6, head=2)),
    ]),
    "A_Giant_Dormant": (60, True, [
        (0, P(spine=14, head=20, dL=40, dR=40, oL=-6, oR=-6, hz=-6)),
        (30, P(spine=16, head=23, dL=42, dR=42, oL=-6, oR=-6, hz=-8)),
        (60, P(spine=14, head=20, dL=40, dR=40, oL=-6, oR=-6, hz=-6)),
    ]),
    "A_Giant_ClapWindup": (30, False, [
        (0, P()),
        (12, P(oL=20, oR=20, spine=-5, hz=6, head=-5)),
        (24, P(oL=27, oR=27, spine=-8, hz=10, head=-8, dL=-8, dR=-8)),
        (26, P(oL=24, oR=30, spine=-8, hz=10, head=-8, dL=-8, dR=-8)),
        (28, P(oL=30, oR=24, spine=-8, hz=10, head=-8, dL=-8, dR=-8)),
        (30, P(oL=27, oR=27, spine=-8, hz=10, head=-8, dL=-8, dR=-8)),
    ]),
    "A_Giant_Clap": (17, False, [
        (0, P(oL=27, oR=27, spine=-8, hz=10, head=-8, dL=-8, dR=-8)),
        (4, P(oL=-52, oR=-52, fL=16, fR=16, spine=10, hz=-12, head=6)),
        (6, P(oL=-49, oR=-49, fL=14, fR=14, spine=11, hz=-10, head=7)),
        (8, P(oL=-46, oR=-46, fL=12, fR=12, spine=9, hz=-6, head=5)),
        (17, P()),
    ]),
    "A_Giant_HopWindup": (18, False, [
        (0, P()),
        (18, P(CROUCH)),
    ]),
    "A_Giant_HopAir": (27, False, [
        (0, P(CROUCH)),
        (5, P(hz=20, tL=-10, kL=5, tR=-10, kR=5, oL=30, oR=30, dL=-32, dR=-32, spine=-8, head=-8)),
        (14, P(hz=30, tL=42, kL=72, tR=42, kR=72, oL=26, oR=26, dL=-24, dR=-24, spine=2)),
        (22, P(hz=10, tL=15, kL=25, tR=15, kR=25, oL=20, oR=20, dL=-14, dR=-14)),
        (27, P(hz=-45, tL=30, kL=55, tR=30, kR=55, spine=12, oL=10, oR=10, head=6)),
    ]),
    "A_Giant_Land": (12, False, [
        (0, P(hz=-50, tL=32, kL=58, tR=32, kR=58, spine=14, oL=8, oR=8, head=8)),
        (12, P()),
    ]),
    "A_Giant_ThrowWindupL": (22, False, [
        (0, P()),
        (22, P(oL=46, dL=-16, fL=-10, spine=-6, hroll=-6, oR=8, head=-5, hyaw=-10)),
    ]),
    "A_Giant_ThrowL": (15, False, [
        (0, P(oL=46, dL=-16, fL=-10, spine=-6, hroll=-6, oR=8, head=-5, hyaw=-10)),
        (5, P(oL=-56, dL=4, fL=14, spine=10, hroll=4, hyaw=12)),
        (15, P(oL=-8, spine=3)),
    ]),
    "A_Giant_WindDownEnter": (21, False, [
        (0, P()),
        (10, P(spine=18, dL=35, dR=35, head=12, hz=-8)),
        (21, P(SLUMP)),
    ]),
    "A_Giant_WoundDown": (60, True, [
        (0, P(SLUMP, hroll=2)),
        (30, P(SLUMP, spine=28, head=24, hroll=-2, hz=-16)),
        (60, P(SLUMP, hroll=2)),
    ]),
    "A_Giant_Rewind": (48, False, [
        (0, P(SLUMP)),
        (8, P(SLUMP, spine=24, hroll=5)),
        (12, P(SLUMP, spine=23, hroll=-5)),
        (16, P(spine=18, hroll=4, dL=40, dR=40, head=14, hz=-10)),
        (24, P(spine=10, hroll=-3, dL=20, dR=20, head=8)),
        (32, P(spine=4, hroll=2, dL=6, dR=6, hz=8)),
        (40, P(spine=-4, oL=10, oR=10, hz=12, head=-4)),
        (48, P()),
    ]),
    "A_Giant_HitReact": (18, False, [
        (0, P(SLUMP)),
        (3, P(spine=8, head=-25, dL=18, dR=18, oL=26, oR=26, hz=22, hroll=3)),
        (8, P(spine=17, head=6, dL=40, dR=40, oL=8, oR=8, hz=0, hroll=-2)),
        (18, P(SLUMP)),
    ]),
    "A_Giant_Stagger": (45, False, [
        (0, P()),
        (4, P(spine=-14, head=-20, oL=32, oR=20, hroll=7, hz=10)),
        (12, P(spine=10, head=12, oL=-10, oR=36, hroll=-9, hyw=15)),
        (20, P(spine=-10, head=-12, oL=30, oR=-6, hroll=8, hyw=-15)),
        (30, P(spine=6, head=6, oL=6, oR=16, hroll=-4)),
        (45, P()),
    ]),
    "A_Giant_PhaseRoar": (78, False, [
        (0, P()),
        (18, P(oL=32, oR=32, dL=-42, dR=-42, spine=-14, head=-22, hz=20)),
        (24, P(oL=36, oR=28, dL=-44, dR=-40, spine=-15, head=-24, hz=22)),
        (28, P(oL=28, oR=36, dL=-40, dR=-44, spine=-15, head=-24, hz=22)),
        (32, P(CROUCH, oL=30, oR=30, dL=-30, dR=-30, hz=-45)),
        (36, P(hz=150, tL=40, kL=70, tR=40, kR=70, oL=34, oR=34, dL=-40, dR=-40, spine=-6)),
        (39, P(hz=-55, tL=34, kL=62, tR=34, kR=62, oL=-40, oR=-40, fL=12, fR=12, spine=18, head=10)),
        (55, P(oL=12, oR=12, spine=4, hz=0)),
        (78, P()),
    ]),
    "A_Giant_Defeat": (42, False, [
        (0, P()),
        (10, P(hz=10, spine=-12, oL=30, oR=40, dL=-20, dR=-10, head=-15, hy=20)),
        (24, P(hpitch=-45, hz=-50, hy=60, tL=30, tR=20, oL=36, oR=46, dL=10, dR=20, head=-20)),
        (34, P(hpitch=-78, hz=-74, hy=110, spine=2, oL=42, oR=52, dL=34, dR=42, tL=62, kL=24, tR=50, kR=12, head=-10)),
        (38, P(hpitch=-72, hz=-60, hy=110, spine=4, oL=40, oR=50, dL=30, dR=38, tL=58, kL=22, tR=46, kR=10, head=-6)),
        (42, P(hpitch=-78, hz=-74, hy=110, spine=2, oL=42, oR=52, dL=34, dR=42, tL=62, kL=24, tR=50, kR=12, head=-10)),
    ]),
}
# right-hand throws are mirrors of the left ones
CLIPS["A_Giant_ThrowWindupR"] = (22, False, [(f, mirror(p)) for f, p in CLIPS["A_Giant_ThrowWindupL"][2]])
CLIPS["A_Giant_ThrowR"] = (15, False, [(f, mirror(p)) for f, p in CLIPS["A_Giant_ThrowL"][2]])


def _rot_about(arm, pb, axis, deg):
    if abs(deg) < 1e-4:
        return
    M = pb.matrix.copy()
    pivot = M.to_translation()
    R = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(deg), 4, Vector(axis)) @ Matrix.Translation(-pivot)
    pb.matrix = R @ M
    bpy.context.view_layer.update()


def apply_pose(arm, p):
    pose = arm.pose.bones
    for pb in pose:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)
    hips = pose["Hips"]
    # whole body: forward pitch (+ tips toward -Y), side rock (+ tips toward +X), yaw, then height / back shift
    _rot_about(arm, hips, Y, p["hroll"])
    _rot_about(arm, hips, X, p["hpitch"])
    _rot_about(arm, hips, Z, p["hyaw"])
    if p["hz"] or p["hy"]:
        M = hips.matrix.copy()
        M.translation = M.translation + Vector((0, p["hy"], p["hz"]))
        hips.matrix = M
        bpy.context.view_layer.update()
    for name, w in (("Spine", 0.3), ("Spine1", 0.3), ("Spine2", 0.4)):
        _rot_about(arm, pose[name], Y, p["sroll"] * w)
        _rot_about(arm, pose[name], X, p["spine"] * w)
    _rot_about(arm, pose["Head"], Y, p["hrl"])
    _rot_about(arm, pose["Head"], X, p["head"])
    _rot_about(arm, pose["Head"], Z, p["hyw"])
    # arms: open (+ = back) around the vertical axis, droop (+ = down) around the forward axis
    _rot_about(arm, pose["LeftArm"], Y, p["dL"])
    _rot_about(arm, pose["LeftArm"], Z, p["oL"])
    _rot_about(arm, pose["RightArm"], Y, -p["dR"])
    _rot_about(arm, pose["RightArm"], Z, -p["oR"])
    _rot_about(arm, pose["LeftForeArm"], Z, -p["fL"])
    _rot_about(arm, pose["RightForeArm"], Z, p["fR"])
    # legs: thigh lift forward (+), knee bend (+), feet kept level
    for side, t, k in (("Left", p["tL"], p["kL"]), ("Right", p["tR"], p["kR"])):
        _rot_about(arm, pose[side + "UpLeg"], X, -t)
        _rot_about(arm, pose[side + "Leg"], X, k)
        _rot_about(arm, pose[side + "Foot"], X, t - k)


KEYED = ["Hips", "Spine", "Spine1", "Spine2", "Neck", "Head", "LeftArm", "RightArm", "LeftForeArm", "RightForeArm",
         "LeftUpLeg", "RightUpLeg", "LeftLeg", "RightLeg", "LeftFoot", "RightFoot"]


def build_clip(arm, name, frames, loop, keys):
    act = bpy.data.actions.get(name)
    if act:
        bpy.data.actions.remove(act)
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    if not arm.animation_data:
        arm.animation_data_create()
    arm.animation_data.action = act
    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    for frame, params in keys:
        apply_pose(arm, params)
        for bn in KEYED:
            pb = arm.pose.bones[bn]
            pb.keyframe_insert("rotation_quaternion", frame=frame, group=bn)
            if bn == "Hips":
                pb.keyframe_insert("location", frame=frame, group=bn)
    act.frame_range = (0, frames)
    return act


def build_all(arm):
    return [build_clip(arm, n, f, l, k) for n, (f, l, k) in CLIPS.items()]


def export_clips(arm, mesh, acts):
    os.makedirs(OUT, exist_ok=True)
    scene = bpy.context.scene
    scene.render.fps = FPS
    for act in acts:
        arm.animation_data.action = act
        try:
            arm.animation_data.action_slot = act.slots[0]
        except Exception:
            pass
        fr = act.frame_range
        scene.frame_start = int(fr[0])
        scene.frame_end = max(int(fr[1]), int(fr[0]) + 1)
        for o in scene.objects:
            o.select_set(False)
        arm.select_set(True)
        bpy.context.view_layer.objects.active = arm
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, act.name + ".fbx"), use_selection=True,
                                 object_types={'ARMATURE'}, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
                                 axis_forward='-Z', axis_up='Y', add_leaf_bones=False, use_armature_deform_only=False,
                                 primary_bone_axis='Y', secondary_bone_axis='X', armature_nodetype='NULL',
                                 bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                                 bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                                 bake_anim_step=1.0, bake_anim_simplify_factor=0.0)
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
