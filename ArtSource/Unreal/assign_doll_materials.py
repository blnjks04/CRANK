import sys, importlib
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal
import crank_import as ci
importlib.reload(ci)
eal = unreal.EditorAssetLibrary
D = "/Game/Crank/Characters/Doll/"
for name in ["SK_Doll", "SM_DollPart_Head", "SM_DollPart_Torso", "SM_DollPart_Arm", "SM_DollPart_Leg", "SM_Doll_Key"]:
    mesh = eal.load_asset(D + name)
    ci.assign_slot_materials(mesh)
    if isinstance(mesh, unreal.SkeletalMesh):
        print(name, [(str(m.get_editor_property("material_slot_name")), m.get_editor_property("material_interface").get_name() if m.get_editor_property("material_interface") else None) for m in mesh.get_editor_property("materials")])
    else:
        print(name, [(str(m.get_editor_property("material_slot_name")), m.get_editor_property("material_interface").get_name() if m.get_editor_property("material_interface") else None) for m in mesh.get_editor_property("static_materials")])
