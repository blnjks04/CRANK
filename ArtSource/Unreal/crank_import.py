"""Unreal-side asset pipeline helpers (run inside the editor via remote execution)."""
import os
import unreal

EXPORT = r"D:\Game\CRANK\ArtSource\Export"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def use_legacy_fbx():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")


def _task(path, dest, name, options):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    return task


def static_options(collision=True):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", collision)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property("remove_degenerates", True)
    data.set_editor_property("import_uniform_scale", 100.0)
    return ui


def skeletal_options(skeleton=None, anim_only=False):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH if not anim_only else unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("import_mesh", not anim_only)
    ui.set_editor_property("import_animations", anim_only)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", False)
    if skeleton:
        ui.set_editor_property("skeleton", skeleton)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    sk.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    sk.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    sk.set_editor_property("import_morph_targets", False)
    an = ui.get_editor_property("anim_sequence_import_data")
    an.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    an.set_editor_property("remove_redundant_keys", False)
    return ui


def exact_collision(mesh):
    """Complex-as-simple collision that matches the drawn mesh.

    Complex collision is cooked from render LOD0, which for a Nanite mesh is its fallback mesh. The default (Auto)
    fallback is decimated: the tiled floor turned into invisible 5-16 cm bumps the dolls tripped over and sank into.
    A full-resolution fallback keeps the collision exact (and non-Nanite GPUs draw the real mesh too)."""
    mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    ns = mesh.get_editor_property("nanite_settings")
    if ns.get_editor_property("enabled"):
        ns.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
        ns.set_editor_property("fallback_percent_triangles", 1.0)
        ns.set_editor_property("fallback_relative_error", 0.0)
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, ns, True)


def import_static(fbx, dest, name=None, collision=True):
    name = name or os.path.splitext(os.path.basename(fbx))[0]
    task = _task(fbx, dest, name, static_options(collision))
    asset_tools.import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


def import_skeletal(fbx, dest, name=None):
    name = name or os.path.splitext(os.path.basename(fbx))[0]
    task = _task(fbx, dest, name, skeletal_options())
    asset_tools.import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


def import_anim(fbx, dest, skeleton, name=None):
    name = name or os.path.splitext(os.path.basename(fbx))[0]
    task = _task(fbx, dest, name, skeletal_options(skeleton=skeleton, anim_only=True))
    asset_tools.import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


MAT = "/Game/Crank/Materials/"
SLOT_MAP = {
    "Paint": MAT + "M_DollPaint",
    "Metal": MAT + "M_VC_Metal",
    "Gloss": MAT + "M_VC_Gloss",
    "Matte": MAT + "M_VC_Matte",
    "Brass": MAT + "M_VC_Brass",
    "Wood": MAT + "M_VC_Wood",
    "Cloth": MAT + "M_VC_Cloth",
    "Glass": MAT + "M_Glass",
    "Emissive": MAT + "M_VC_Emissive",
    "Eyes": MAT + "M_BossEyes",
}


def assign_slot_materials(mesh, mapping=None):
    mapping = mapping or SLOT_MAP
    return _assign(mesh, mapping)


def _assign(mesh, mapping):
    """mapping: slot name -> material asset path. Works for static and skeletal meshes."""
    if isinstance(mesh, unreal.StaticMesh):
        mats = mesh.get_editor_property("static_materials")
        for i, m in enumerate(mats):
            slot = str(m.get_editor_property("material_slot_name"))
            key = next((k for k in mapping if slot.startswith(k)), None)
            if key:
                mat = eal.load_asset(mapping[key])
                if mat:
                    mesh.set_material(i, mat)
    elif isinstance(mesh, unreal.SkeletalMesh):
        mats = mesh.get_editor_property("materials")
        new = []
        for m in mats:
            slot = str(m.get_editor_property("material_slot_name"))
            key = next((k for k in mapping if slot.startswith(k)), None)
            if key:
                mat = eal.load_asset(mapping[key])
                if mat:
                    m.set_editor_property("material_interface", mat)
            new.append(m)
        mesh.set_editor_property("materials", new)
    eal.save_loaded_asset(mesh)
