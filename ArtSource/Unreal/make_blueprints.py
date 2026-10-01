"""Create BP_* subclasses of the C++ gameplay actors and assign assets in their defaults."""
import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.CrankEditorLibrary
BP = "/Game/Crank/Blueprints"

ENV = "/Game/Crank/Environment/"
PROPS = "/Game/Crank/Props/"
BOSS = "/Game/Crank/Boss/"
DOLL = "/Game/Crank/Characters/Doll/"
ANIM = DOLL + "Anims/"


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
    ok = lib.set_blueprint_component_property(bp, component, prop, value)
    if not ok:
        print("  !! failed", bp.get_name(), component, prop, value)


def default(bp, prop, value):
    ok = lib.set_blueprint_default_property(bp, prop, value)
    if not ok:
        print("  !! failed default", bp.get_name(), prop, value)


def finish(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp, False)
    print("BP", bp.get_path_name())


# ---------------------------------------------------------------- body part + carryables
bp = make_bp("BP_BodyPart", unreal.BodyPartPickup)
comp(bp, "Mesh", "StaticMesh", obj(DOLL + "SM_DollPart_Torso"))
default(bp, "PartMeshes", '((Head, %s), (Torso, %s), (Key, %s), (Arm, %s), (Leg, %s))' % (
    obj(DOLL + "SM_DollPart_Head"), obj(DOLL + "SM_DollPart_Torso"), obj(DOLL + "SM_Doll_Key"), obj(DOLL + "SM_DollPart_Arm"), obj(DOLL + "SM_DollPart_Leg")))
comp(bp, "AttachedHead", "StaticMesh", obj(DOLL + "SM_DollPart_Head"))
comp(bp, "AttachedHead", "RelativeLocation", "(X=0,Y=0,Z=40)")
comp(bp, "AttachedKey", "StaticMesh", obj(DOLL + "SM_Doll_Key"))
comp(bp, "AttachedKey", "RelativeLocation", "(X=-16,Y=0,Z=5)")
comp(bp, "AttachedKey", "RelativeRotation", "(Pitch=0,Yaw=180,Roll=0)")
finish(bp)

parts = [
    # name, mesh, type, weight, display, mass, hold height, hold forward
    ("BP_Part_Gear", PROPS + "SM_Part_Gear", "Gear", "Light", "톱니 (가벼움)", 3.0, 10.0, 10.0),
    ("BP_Part_SpringCoil", PROPS + "SM_Part_SpringCoil", "SpringCoil", "Heavy", "스프링 코일 (무거움)", 12.0, 10.0, 20.0),
    ("BP_Part_JewelBearing", PROPS + "SM_Part_JewelBearing", "JewelBearing", "Light", "보석 베어링 (가벼움)", 2.0, 10.0, 6.0),
    ("BP_Part_MainSpring", PROPS + "SM_Part_MainSpring", "MainSpring", "TwoPerson", "메인 스프링 (둘이 함께!)", 30.0, 25.0, 60.0),
    ("BP_Dishcloth", PROPS + "SM_Dishcloth", "Dishcloth", "Cloth", "행주 (둘이 들면 빠름)", 6.0, 20.0, 50.0),
]
for name, mesh, ptype, weight, label, mass, hh, hf in parts:
    bp = make_bp(name, unreal.CarryablePart)
    comp(bp, "Mesh", "StaticMesh", obj(mesh))
    default(bp, "PartType", ptype)
    default(bp, "Weight", weight)
    default(bp, "DisplayName", 'NSLOCTEXT("Crank", "%s", "%s")' % (name, label))
    default(bp, "MassKg", str(mass))
    default(bp, "HoldHeight", str(hh))
    default(bp, "HoldForward", str(hf))
    if ptype == "Dishcloth":
        default(bp, "bGlow", "False")
    finish(bp)

bp = make_bp("BP_SpentCell", unreal.BatteryCellPickup)
comp(bp, "Mesh", "StaticMesh", obj(BOSS + "SM_Boss_Cell"))
finish(bp)

# ---------------------------------------------------------------- doll
bp = make_bp("BP_WindupDoll", unreal.WindupCharacter)
comp(bp, "CharacterMesh0", "SkeletalMeshAsset", obj(DOLL + "SK_Doll"))
comp(bp, "KeyMesh", "StaticMesh", obj(DOLL + "SM_Doll_Key"))
anims = {k: obj(ANIM + "A_Doll_" + k) for k in ["Idle", "Walk", "Run", "Sprint", "Crawl", "Fall", "Fly", "Wind", "Pull", "Wound",
                                                  "Carry", "Carried", "ReachL", "ReachR", "Splat", "Dizzy", "Frozen"]}
emotes = ",".join(obj(ANIM + "A_Doll_Emote_" + e) for e in ["Wave", "Dance", "Cheer", "Bow"])
anim_text = "(" + ",".join("%s=%s" % (k, v) for k, v in anims.items()) + ",Emotes=(" + emotes + "))"
default(bp, "AnimSet", anim_text)
default(bp, "BodyPartClass", cls_path("BP_BodyPart"))
default(bp, "PaintMaterialIndex", "0")
finish(bp)

# ---------------------------------------------------------------- stations / zones / mechanisms
bp = make_bp("BP_WindStation", unreal.WindStation)
comp(bp, "Body", "StaticMesh", obj(PROPS + "SM_MusicBox"))
comp(bp, "Spinner", "StaticMesh", obj(PROPS + "SM_MusicBox_Dancer"))
comp(bp, "Spinner", "RelativeLocation", "(X=0,Y=-20,Z=92.4)")	# 4 mm above the lid (coplanar = flicker)
comp(bp, "Crank", "StaticMesh", obj(PROPS + "SM_MusicBox_Crank"))
comp(bp, "Crank", "RelativeLocation", "(X=0,Y=-125,Z=50)")
comp(bp, "Crank", "RelativeScale3D", "(X=0.6,Y=0.6,Z=0.6)")	# spins around its shaft: the handle must clear the floor
comp(bp, "NotesFX", "RelativeLocation", "(X=0,Y=-20,Z=200)")
finish(bp)

bp = make_bp("BP_DeliveryTube", unreal.DeliveryZone)
comp(bp, "Visual", "StaticMesh", obj(PROPS + "SM_DeliveryTube"))
comp(bp, "Box", "RelativeLocation", "(X=0,Y=0,Z=90)")
comp(bp, "Box", "BoxExtent", "(X=130,Y=130,Z=80)")
finish(bp)

bp = make_bp("BP_DeliveryWorkbench", unreal.DeliveryZone)
comp(bp, "Visual", "StaticMesh", obj(PROPS + "SM_ClockMovement"))
comp(bp, "Box", "RelativeLocation", "(X=0,Y=0,Z=100)")
comp(bp, "Box", "BoxExtent", "(X=200,Y=200,Z=100)")
default(bp, "AcceptedParts", "(MainSpring)")
default(bp, "Label", 'NSLOCTEXT("Crank", "WorkbenchDelivery", "메인 스프링 납품")')
finish(bp)

bp = make_bp("BP_PullDrawer", unreal.PullDrawer)
comp(bp, "Drawer", "StaticMesh", obj(ENV + "SM_Drawer"))
comp(bp, "HandleBox", "RelativeLocation", "(X=40,Y=0,Z=45)")
comp(bp, "HandleBox", "BoxExtent", "(X=24,Y=140,Z=22)")
finish(bp)

bp = make_bp("BP_PullLever", unreal.PullLever)
comp(bp, "Base", "StaticMesh", obj(PROPS + "SM_LeverBase"))
comp(bp, "Pivot", "RelativeLocation", "(X=0,Y=0,Z=48)")
comp(bp, "Handle", "StaticMesh", obj(PROPS + "SM_LeverHandle"))
comp(bp, "GrabBox", "RelativeLocation", "(X=0,Y=0,Z=125)")
comp(bp, "GrabBox", "BoxExtent", "(X=30,Y=30,Z=30)")
finish(bp)

bp = make_bp("BP_Bridge_Spatula", unreal.Drawbridge)
comp(bp, "Plank", "StaticMesh", obj(PROPS + "SM_Spatula"))
finish(bp)

bp = make_bp("BP_Bridge_Board", unreal.Drawbridge)
comp(bp, "Plank", "StaticMesh", obj(PROPS + "SM_PlankRamp"))
default(bp, "SwingTime", "1.6")
finish(bp)

bp = make_bp("BP_TableclothShortcut", unreal.TableclothShortcut)
comp(bp, "Curtain", "StaticMesh", obj(ENV + "SM_Tablecloth_Curtain"))
comp(bp, "FallenCloth", "StaticMesh", obj(ENV + "SM_Tablecloth_Fallen"))
comp(bp, "FallenCloth", "RelativeLocation", "(X=-520,Y=0,Z=-650)")
finish(bp)

bp = make_bp("BP_SurfaceZone", unreal.SurfaceZone)
finish(bp)

bp = make_bp("BP_Checkpoint", unreal.CrankCheckpoint)
finish(bp)

bp = make_bp("BP_RaidClock", unreal.RaidClock)
comp(bp, "Body", "StaticMesh", obj(PROPS + "SM_GrandfatherClock"))
comp(bp, "FacePivot", "RelativeLocation", "(X=196,Y=0,Z=1880)")
comp(bp, "HourHand", "StaticMesh", obj(PROPS + "SM_ClockHand_Hour"))
comp(bp, "MinuteHand", "StaticMesh", obj(PROPS + "SM_ClockHand_Minute"))
comp(bp, "PendulumPivot", "RelativeLocation", "(X=100,Y=0,Z=1450)")
comp(bp, "Pendulum", "StaticMesh", obj(PROPS + "SM_ClockPendulum"))
finish(bp)

# ---------------------------------------------------------------- boss
bp = make_bp("BP_WaterTrail", unreal.WaterTrail)
comp(bp, "Puddles", "StaticMesh", obj("/Game/Crank/FX/SM_FX_Puff"))
comp(bp, "Puddles", "OverrideMaterials", '(%s)' % obj("/Game/Crank/Materials/M_Water"))
finish(bp)

bp = make_bp("BP_ChargingDock", unreal.ChargingDock)
comp(bp, "Base", "StaticMesh", obj(PROPS + "SM_ChargingDock"))
finish(bp)

bp = make_bp("BP_Boss_DustEater", unreal.BossDustEater)
comp(bp, "Body", "StaticMesh", obj(BOSS + "SM_Boss_Body"))
comp(bp, "Bumper", "StaticMesh", obj(BOSS + "SM_Boss_Bumper"))
comp(bp, "HatchPivot", "RelativeLocation", "(X=-41,Y=0,Z=81)")	# hatch 1 cm above the cell tops (coplanar = flicker)
comp(bp, "Hatch", "StaticMesh", obj(BOSS + "SM_Boss_Hatch"))
comp(bp, "Ramp", "StaticMesh", obj(BOSS + "SM_Boss_Ramp"))
comp(bp, "BrushL", "StaticMesh", obj(BOSS + "SM_Boss_Brush"))
comp(bp, "BrushR", "StaticMesh", obj(BOSS + "SM_Boss_Brush"))
for i in range(3):
    comp(bp, "Cell%d" % i, "StaticMesh", obj(BOSS + "SM_Boss_Cell"))
comp(bp, "BinLever", "StaticMesh", obj(BOSS + "SM_Boss_Lever"))
comp(bp, "BinDoor", "StaticMesh", obj(BOSS + "SM_Boss_BinDoor"))
default(bp, "WaterTrailClass", cls_path("BP_WaterTrail"))
default(bp, "MainSpringClass", cls_path("BP_Part_MainSpring"))
default(bp, "SpentCellClass", cls_path("BP_SpentCell"))
finish(bp)

bp = make_bp("BP_BossArena", unreal.BossArenaManager)
finish(bp)

# ---------------------------------------------------------------- game mode
bp = make_bp("BP_RaidGameMode", unreal.RaidGameMode)
default(bp, "DefaultPawnClass", cls_path("BP_WindupDoll"))
default(bp, "DummyClass", cls_path("BP_WindupDoll"))
finish(bp)
print("all blueprints done")
