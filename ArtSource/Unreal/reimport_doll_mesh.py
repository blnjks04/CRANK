"""Swap in a rebuilt SK_Doll (same bones) without touching its skeleton, clips, physics asset or the blueprints."""
import os
import sys
import importlib
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal
import crank_import as ci
importlib.reload(ci)

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DEST = "/Game/Crank/Characters/Doll"
SRC = os.path.join(ci.EXPORT, "Doll")
ci.use_legacy_fbx()

sk = eal.load_asset(DEST + "/SK_Doll")
skel = sk.get_editor_property("skeleton")
phys = sk.get_editor_property("physics_asset")
print("before: skeleton", skel.get_path_name(), "physics", phys.get_path_name() if phys else None, "bounds", sk.get_bounds().box_extent)

task = ci._task(os.path.join(SRC, "SK_Doll.fbx"), DEST, "SK_Doll", ci.skeletal_options(skeleton=skel))
tools.import_asset_tasks([task])
sk = eal.load_asset(DEST + "/SK_Doll")
if sk.get_editor_property("skeleton") != skel:
    print("!! skeleton changed")
if phys and sk.get_editor_property("physics_asset") != phys:
    sk.set_editor_property("physics_asset", phys)
ci.assign_slot_materials(sk)
eal.save_loaded_asset(sk)
print("after: skeleton", sk.get_editor_property("skeleton").get_path_name(), "physics",
      sk.get_editor_property("physics_asset").get_path_name() if sk.get_editor_property("physics_asset") else None,
      "bounds", sk.get_bounds().box_extent)
print("materials", [(str(m.get_editor_property("material_slot_name")), m.get_editor_property("material_interface").get_name() if m.get_editor_property("material_interface") else None) for m in sk.get_editor_property("materials")])

for name in ["SM_DollPart_Head", "SM_DollPart_Torso", "SM_DollPart_Arm", "SM_DollPart_Leg", "SM_Doll_Key"]:
    ci.import_static(os.path.join(SRC, name + ".fbx"), DEST, name, collision=True)
    m = eal.load_asset(DEST + "/" + name)
    ci.assign_slot_materials(m)
    eal.save_loaded_asset(m)
    print("part", name, m.get_bounds().box_extent)
