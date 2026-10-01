"""(Re)import the giant cymbal monkey rig: SK_GiantMonkey + its 19 clips (used by import_giant.py, or alone after a rig edit).

Old clips are deleted first so they get rebuilt against the new skeleton (the CymbalL / CymbalR bones).
Run make_giant_bps.py afterwards to re-wire BP_Boss_GiantMonkey.
"""
import os
import sys
import importlib
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal
import crank_import as ci

eal = unreal.EditorAssetLibrary
EXP = r"D:\Game\CRANK\ArtSource\Export\Giant"
GIANT = "/Game/Crank/Boss/Giant"


def run(mi=None):
    importlib.reload(ci)
    ci.use_legacy_fbx()
    mi = mi or eal.load_asset("/Game/Crank/Materials/MI_GiantMonkey")
    anim_dir = os.path.join(EXP, "Anims")
    clips = sorted(os.path.splitext(f)[0] for f in os.listdir(anim_dir) if f.endswith(".fbx"))
    for p in [GIANT + "/Anims/" + c for c in clips] + [GIANT + "/SK_GiantMonkey", GIANT + "/SK_GiantMonkey_Skeleton"]:
        if eal.does_asset_exist(p):
            eal.delete_asset(p)

    ci.import_skeletal(os.path.join(EXP, "SK_GiantMonkey.fbx"), GIANT, "SK_GiantMonkey")
    sk = eal.load_asset(GIANT + "/SK_GiantMonkey")
    skel = sk.get_editor_property("skeleton")
    # (setting SkeletalMaterial copies through Python does not stick: import the property text instead)
    lib = unreal.CrankEditorLibrary
    txt = lib.get_object_property_text(sk, "Materials")
    if "MaterialInterface=" not in txt:
        txt = txt.replace("((MaterialSlotName=", "((MaterialInterface=\"%s\",MaterialSlotName=" % mi.get_path_name(), 1)
        lib.set_object_property_text(sk, "Materials", txt)
    eal.save_loaded_asset(sk)
    print("skeletal", sk.get_path_name(), "bounds", sk.get_bounds().box_extent, "skeleton", skel.get_path_name())

    for name in clips:
        ci.import_anim(os.path.join(anim_dir, name + ".fbx"), GIANT + "/Anims", skel, name)
    print("anims", eal.list_assets(GIANT + "/Anims", recursive=False))
    return sk
