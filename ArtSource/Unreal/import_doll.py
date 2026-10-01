import sys, importlib, os
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal
import crank_import as ci
importlib.reload(ci)

ci.use_legacy_fbx()
DEST = "/Game/Crank/Characters/Doll"
SRC = os.path.join(ci.EXPORT, "Doll")
eal = unreal.EditorAssetLibrary

# Fresh import (re-import would reuse stale import settings such as the uniform scale).
if eal.does_directory_exist(DEST):
    eal.delete_directory(DEST)

paths = ci.import_skeletal(os.path.join(SRC, "SK_Doll.fbx"), DEST, "SK_Doll")
mesh = eal.load_asset(DEST + "/SK_Doll")
skeleton = mesh.get_editor_property("skeleton")
bounds = mesh.get_bounds()
print("skeletal:", paths, "skeleton:", skeleton.get_path_name())
print("bounds origin", bounds.origin, "extent", bounds.box_extent)
print("materials:", [str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("materials")])

pa = unreal.CrankEditorLibrary.build_doll_physics_asset(mesh, DEST + "/PHYS_Doll", 1.0)
print("physics:", pa.get_path_name() if pa else None)

# Static parts + key
for name in ["SM_DollPart_Head", "SM_DollPart_Torso", "SM_DollPart_Arm", "SM_DollPart_Leg", "SM_Doll_Key"]:
    print(ci.import_static(os.path.join(SRC, name + ".fbx"), DEST, name, collision=True))

# Animations
anim_dir = DEST + "/Anims"
for f in sorted(os.listdir(SRC)):
    if f.startswith("A_Doll_") and f.endswith(".fbx"):
        out = ci.import_anim(os.path.join(SRC, f), anim_dir, skeleton, os.path.splitext(f)[0])
        print(f, "->", out)
print(eal.list_assets(anim_dir, recursive=False))
