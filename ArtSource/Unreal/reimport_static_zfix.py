"""Re-import every procedural static mesh after the z-fighting fix (ArtSource/Blender/zfight.py).

Kitchen meshes go through import_static_all.py; the toy room shell / props keep the collision setup of
import_giant.py (complex-as-simple shell, rug without collision).
"""
import os
import sys
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal
import importlib
import crank_import as ci
importlib.reload(ci)

eal = unreal.EditorAssetLibrary
EXP = ci.EXPORT
TOY_SHELL = ["SM_Wall_North", "SM_Toy_Floor", "SM_Toy_Walls", "SM_Toy_Ceiling"]
TOY_PROPS = [("SM_Toy_Rug", "none"), ("SM_Toy_Chest", "complex"), ("SM_Toy_Ball", "complex"), ("SM_Toy_Books", "complex"),
             ("SM_Toy_BlockSmall", "complex"), ("SM_Toy_BlockBig", "complex")]
toy_names = set(TOY_SHELL) | {n for n, _ in TOY_PROPS}

# 1) kitchen / props / FX / boss through the regular script (everything except the toy room files)
names = []
for folder in ("FX", "Boss", "Arch", "Props"):
    src = os.path.join(EXP, folder)
    for f in sorted(os.listdir(src)):
        if f.endswith(".fbx") and os.path.splitext(f)[0] not in toy_names:
            names.append(os.path.splitext(f)[0])
saved_argv = sys.argv
sys.argv = ["import_static_all.py"] + names
try:
    exec(open(r"D:\Game\CRANK\ArtSource\Unreal\import_static_all.py", encoding="utf-8").read(), {"__name__": "__main__"})
finally:
    sys.argv = saved_argv

# 2) toy room, same settings as import_giant.py
ci.use_legacy_fbx()
for name in TOY_SHELL:
    ci.import_static(os.path.join(EXP, "Arch", name + ".fbx"), "/Game/Crank/Environment", name, collision=False)
    m = eal.load_asset("/Game/Crank/Environment/" + name)
    ci.assign_slot_materials(m)
    ci.exact_collision(m)
    eal.save_loaded_asset(m)
    print("toy ok", name)
for name, mode in TOY_PROPS:
    ci.import_static(os.path.join(EXP, "Props", name + ".fbx"), "/Game/Crank/Props", name, collision=False)
    m = eal.load_asset("/Game/Crank/Props/" + name)
    ci.assign_slot_materials(m)
    if mode == "complex":
        ci.exact_collision(m)
    else:
        m.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    eal.save_loaded_asset(m)
    print("toy ok", name)
print("reimport done:", len(names), "kitchen meshes +", len(toy_names), "toy room meshes")
