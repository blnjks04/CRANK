"""One-shot: full-resolution Nanite fallback for every complex-collision static mesh (see crank_import.exact_collision)."""
import sys, importlib
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal
import crank_import as ci
importlib.reload(ci)

eal = unreal.EditorAssetLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
fixed = 0
for data in reg.get_assets_by_path("/Game/Crank", recursive=True):
    if str(data.asset_class_path.asset_name) != "StaticMesh":
        continue
    mesh = data.get_asset()
    body = mesh.get_editor_property("body_setup")
    if not body or body.get_editor_property("collision_trace_flag") != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
        continue
    ns = mesh.get_editor_property("nanite_settings")
    if not ns.get_editor_property("enabled"):
        continue
    if ns.get_editor_property("fallback_target") == unreal.NaniteFallbackTarget.PERCENT_TRIANGLES \
            and ns.get_editor_property("fallback_percent_triangles") >= 1.0 and ns.get_editor_property("fallback_relative_error") <= 0.0:
        continue
    before = mesh.get_num_triangles(0)
    ci.exact_collision(mesh)
    eal.save_loaded_asset(mesh, False)
    fixed += 1
    print("NF %-28s collision tris %6d -> %6d" % (mesh.get_name(), before, mesh.get_num_triangles(0)))
print("NF fixed", fixed)
