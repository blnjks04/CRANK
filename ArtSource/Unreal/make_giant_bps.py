"""BP subclasses for zone F: the giant cymbal monkey, its thrown cymbal, the golden key reward and the arena."""
import sys
import importlib
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.CrankEditorLibrary
BP = "/Game/Crank/Blueprints"
GIANT = "/Game/Crank/Boss/Giant/"
FX = "/Game/Crank/FX/"
MATS = "/Game/Crank/Materials/"


def obj(path):
    name = path.rsplit("/", 1)[-1]
    return '"%s.%s"' % (path, name)


def cls_path(bp_name):
    return '"%s/%s.%s_C"' % (BP, bp_name, bp_name)


def make_bp(name, parent):
    path = BP + "/" + name
    bp = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = tools.create_asset(name, BP, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp


def comp(bp, component, prop, value):
    if not lib.set_blueprint_component_property(bp, component, prop, value):
        print("  !! failed", bp.get_name(), component, prop, value)


def default(bp, prop, value):
    if not lib.set_blueprint_default_property(bp, prop, value):
        print("  !! failed default", bp.get_name(), prop, value)


def finish(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp, False)
    print("BP", bp.get_path_name())


# golden key reward (light, one doll can carry it)
bp = make_bp("BP_GoldenKey", unreal.CarryablePart)
comp(bp, "Mesh", "StaticMesh", obj(GIANT + "SM_GoldenKey"))
default(bp, "PartType", "GoldenKey")
default(bp, "Weight", "Light")
default(bp, "DisplayName", 'NSLOCTEXT("Crank", "BP_GoldenKey", "황금 태엽 열쇠 (작업대로!)")')
default(bp, "MassKg", "4.0")
default(bp, "HoldHeight", "15.0")
default(bp, "HoldForward", "30.0")
finish(bp)

# boomerang cymbal
bp = make_bp("BP_CymbalProjectile", unreal.CymbalProjectile)
comp(bp, "Disc", "StaticMesh", obj(GIANT + "SM_Giant_Cymbal"))
finish(bp)

# arena
bp = make_bp("BP_GiantArena", unreal.GiantArena)
finish(bp)

# the giant
bp = make_bp("BP_Boss_GiantMonkey", unreal.BossGiantMonkey)
comp(bp, "Mesh", "SkeletalMeshAsset", obj(GIANT + "SK_GiantMonkey"))
comp(bp, "KeyMesh", "StaticMesh", obj(GIANT + "SM_Giant_Key"))
clips = ["Idle", "Walk", "Dormant", "ClapWindup", "Clap", "HopWindup", "HopAir", "Land", "ThrowWindupL", "ThrowL",
         "ThrowWindupR", "ThrowR", "WindDownEnter", "WoundDown", "Rewind", "HitReact", "Stagger", "PhaseRoar", "Defeat"]
default(bp, "AnimSet", "(" + ",".join("%s=%s" % (c, obj(GIANT + "Anims/A_Giant_" + c)) for c in clips) + ")")
default(bp, "RingMesh", obj(FX + "SM_FX_Ring"))
default(bp, "RingMaterial", obj(MATS + "M_GiantRing"))
default(bp, "CymbalClass", cls_path("BP_CymbalProjectile"))
default(bp, "RewardClass", cls_path("BP_GoldenKey"))
finish(bp)

# verify
cdo = unreal.get_default_object(eal.load_asset(BP + "/BP_Boss_GiantMonkey").generated_class())
print("AnimSet:", lib.get_object_property_text(cdo, "AnimSet")[:160])
print("Mesh:", lib.get_object_property_text(lib.find_component_by_name(cdo, "Mesh"), "SkeletalMeshAsset"))
print("Reward:", lib.get_object_property_text(cdo, "RewardClass"), "Cymbal:", lib.get_object_property_text(cdo, "CymbalClass"))
