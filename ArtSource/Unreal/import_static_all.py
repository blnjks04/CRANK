"""Import every exported static mesh (FX, Boss, Arch, Props), assign materials by slot name, set collision."""
import sys, importlib, os
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal
import crank_import as ci
importlib.reload(ci)

ci.use_legacy_fbx()
eal = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) if hasattr(unreal, "StaticMeshEditorSubsystem") else None

GROUPS = {
    "FX": "/Game/Crank/FX",
    "Boss": "/Game/Crank/Boss",
    "Arch": "/Game/Crank/Environment",
    "Props": "/Game/Crank/Props",
}
# meshes that need exact (complex) collision for walking / climbing
COMPLEX = {
    "SM_Floor", "SM_Wall_North", "SM_Wall_South", "SM_Wall_East", "SM_Wall_West", "SM_Ceiling", "SM_Bookcase",
    "SM_CrateStack", "SM_Workbench", "SM_RulerRamp", "SM_EntryWedge", "SM_SinkCabinet", "SM_Counter", "SM_Drawer",
    "SM_Fridge", "SM_FridgeDoor_L", "SM_FridgeDoor_R", "SM_TableTop", "SM_TableLeg", "SM_Chair", "SM_Tablecloth_Curtain",
    "SM_Pot_A", "SM_Pot_B", "SM_Pot_C", "SM_Colander", "SM_BookStack", "SM_Spatula", "SM_TrayRamp", "SM_CanFence",
    "SM_Crate", "SM_MusicBox", "SM_ChargingDock", "SM_Toaster", "SM_CoolingRack", "SM_BreadBox", "SM_Jar_A", "SM_Jar_B",
    "SM_Kettle", "SM_PlankRamp", "SM_GrandfatherClock", "SM_DeliveryTube", "SM_ClockMovement", "SM_Boss_Body", "SM_Boss_Ramp",
    "SM_CabinetClutter", "SM_LeverBase",
}
# no collision at all (pure decoration / particles)
NONE = {"SM_FX_Puff", "SM_FX_Dot", "SM_FX_Star", "SM_FX_Bolt", "SM_FX_Spring", "SM_FX_Drop", "SM_FX_Note", "SM_FX_Ring",
        "SM_Tablecloth_Fallen", "SM_CeilingLamp", "SM_ClockHand_Hour", "SM_ClockHand_Minute", "SM_ClockPendulum",
        "SM_Rug", "SM_MusicBox_Dancer", "SM_MusicBox_Crank", "SM_Nightlight", "SM_Boss_Bumper", "SM_Boss_Hatch",
        "SM_Boss_Brush", "SM_Boss_BinDoor", "SM_Boss_Lever", "SM_LeverHandle"}

only = set(sys.argv[1:]) if len(sys.argv) > 1 else None
folders = set(os.environ.get("CRANK_IMPORT_FOLDERS", "FX,Boss,Arch,Props").split(","))
for folder, dest in GROUPS.items():
    if folder not in folders:
        continue
    src = os.path.join(ci.EXPORT, folder)
    if not os.path.isdir(src):
        continue
    for f in sorted(os.listdir(src)):
        if not f.endswith(".fbx"):
            continue
        name = os.path.splitext(f)[0]
        if only and name not in only:
            continue
        path = dest + "/" + name
        ci.import_static(os.path.join(src, f), dest, name, collision=(name not in NONE and name not in COMPLEX))
        mesh = eal.load_asset(path)
        if not mesh:
            print("FAILED", name)
            continue
        ci.assign_slot_materials(mesh)
        body = mesh.get_editor_property("body_setup")
        if body:
            if name in COMPLEX:
                ci.exact_collision(mesh)
            elif name in NONE:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        eal.save_loaded_asset(mesh)
        print("ok", path, mesh.get_bounds().box_extent)
