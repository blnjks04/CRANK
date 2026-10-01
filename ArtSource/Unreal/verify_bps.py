import unreal
eal = unreal.EditorAssetLibrary
lib = unreal.CrankEditorLibrary
B = "/Game/Crank/Blueprints/"
checks = [
    ("BP_WindupDoll", "CharacterMesh0", "SkeletalMeshAsset"),
    ("BP_WindupDoll", "KeyMesh", "StaticMesh"),
    ("BP_Boss_DustEater", "Body", "StaticMesh"),
    ("BP_Boss_DustEater", "HatchPivot", "RelativeLocation"),
    ("BP_WindStation", "Spinner", "RelativeLocation"),
    ("BP_PullDrawer", "Drawer", "StaticMesh"),
    ("BP_Part_MainSpring", "Mesh", "StaticMesh"),
]
for bp_name, comp_name, prop in checks:
    bp = eal.load_asset(B + bp_name)
    cdo = unreal.get_default_object(bp.generated_class())
    comp = lib.find_component_by_name(cdo, comp_name)
    print(bp_name, comp_name, prop, "=", lib.get_object_property_text(comp, prop) if comp else "NO COMPONENT")
for bp_name, prop in [("BP_WindupDoll", "BodyPartClass"), ("BP_Part_MainSpring", "Weight"), ("BP_RaidGameMode", "DefaultPawnClass"), ("BP_BodyPart", "PartMeshes")]:
    bp = eal.load_asset(B + bp_name)
    cdo = unreal.get_default_object(bp.generated_class())
    print(bp_name, prop, "=", lib.get_object_property_text(cdo, prop)[:200])
cdo = unreal.get_default_object(eal.load_asset(B + "BP_WindupDoll").generated_class())
print("AnimSet:", lib.get_object_property_text(cdo, "AnimSet")[:300])
