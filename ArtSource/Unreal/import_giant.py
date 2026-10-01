"""Zone F import: giant cymbal monkey (Tripo model, Blender-authored clips), its parts, the toy room and new SFX."""
import os
import sys
import importlib
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Unreal")
import unreal
import crank_import as ci
importlib.reload(ci)

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
EXP = r"D:\Game\CRANK\ArtSource\Export"
GIANT = "/Game/Crank/Boss/Giant"
MATS = "/Game/Crank/Materials"

ci.use_legacy_fbx()


# ------------------------------------------------------------------ textures
def import_texture(path, dest, name, srgb=True, normal=False):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    tools.import_asset_tasks([t])
    tex = eal.load_asset(dest + "/" + name)
    tex.set_editor_property("srgb", srgb)
    if normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif not srgb:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    eal.save_loaded_asset(tex)
    return tex


TEX = os.path.join(EXP, "Giant", "Textures")
tex_base = import_texture(os.path.join(TEX, "T_Giant_BaseColor.jpg"), GIANT + "/Textures", "T_Giant_BaseColor")
tex_norm = import_texture(os.path.join(TEX, "T_Giant_Normal.png"), GIANT + "/Textures", "T_Giant_Normal", srgb=False, normal=True)
tex_rough = import_texture(os.path.join(TEX, "T_Giant_Roughness.jpg"), GIANT + "/Textures", "T_Giant_Roughness", srgb=False)
tex_metal = import_texture(os.path.join(TEX, "T_Giant_Metallic.jpg"), GIANT + "/Textures", "T_Giant_Metallic", srgb=False)


# ------------------------------------------------------------------ materials
def new_material(name):
    path = MATS + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


# Textured PBR master for Tripo assets, with a "Glow" emissive boost (weak point / hit flash).
mat = new_material("M_TripoPBR")
base = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, -200, parameter_name="BaseColorTex", texture=tex_base)
nrm = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 100, parameter_name="NormalTex", texture=tex_norm,
           sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
rgh = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 400, parameter_name="RoughnessTex", texture=tex_rough,
           sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
mtl = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 700, parameter_name="MetallicTex", texture=tex_metal,
           sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
tint = expr(mat, unreal.MaterialExpressionVectorParameter, -600, -350, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
mul_tint = expr(mat, unreal.MaterialExpressionMultiply, -400, -250)
mel.connect_material_expressions(base, "RGB", mul_tint, "A")
mel.connect_material_expressions(tint, "", mul_tint, "B")
mel.connect_material_property(mul_tint, "", MP.MP_BASE_COLOR)
mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
mel.connect_material_property(rgh, "R", MP.MP_ROUGHNESS)
mel.connect_material_property(mtl, "R", MP.MP_METALLIC)
glow = expr(mat, unreal.MaterialExpressionScalarParameter, -600, 950, parameter_name="Glow", default_value=0.0)
glow_col = expr(mat, unreal.MaterialExpressionVectorParameter, -600, 1050, parameter_name="GlowColor", default_value=unreal.LinearColor(1.0, 0.72, 0.22, 1))
g1 = expr(mat, unreal.MaterialExpressionMultiply, -350, 950)
mel.connect_material_expressions(glow_col, "", g1, "A")
mel.connect_material_expressions(glow, "", g1, "B")
g2 = expr(mat, unreal.MaterialExpressionMultiply, -150, 900)
mel.connect_material_expressions(base, "RGB", g2, "A")
mel.connect_material_expressions(g1, "", g2, "B")
mel.connect_material_property(g2, "", MP.MP_EMISSIVE_COLOR)
mat.set_editor_property("used_with_skeletal_mesh", True)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)

mi_path = MATS + "/MI_GiantMonkey"
if eal.does_asset_exist(mi_path):
    eal.delete_asset(mi_path)
mi = tools.create_asset("MI_GiantMonkey", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi.set_editor_property("parent", mat)
eal.save_loaded_asset(mi)

# Expanding shockwave ring: translucent unlit with a "Color" (rgb + alpha) parameter.
ring = new_material("M_GiantRing")
ring.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
ring.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
ring.set_editor_property("two_sided", True)
col = expr(ring, unreal.MaterialExpressionVectorParameter, -700, 0, parameter_name="Color", default_value=unreal.LinearColor(1.0, 0.85, 0.45, 0.85))
boost = expr(ring, unreal.MaterialExpressionMultiply, -450, -40)
two = expr(ring, unreal.MaterialExpressionConstant, -650, -150, r=3.0)
mel.connect_material_expressions(col, "RGB", boost, "A")
mel.connect_material_expressions(two, "", boost, "B")
mel.connect_material_property(boost, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(col, "A", MP.MP_OPACITY)
mel.recompile_material(ring)
eal.save_loaded_asset(ring)
print("materials ok")

# ------------------------------------------------------------------ skeletal mesh + clips
import import_giant_rig as rig
importlib.reload(rig)
sk_dir = os.path.join(EXP, "Giant")
rig.run(mi)

# ------------------------------------------------------------------ static parts (already in centimeters)
def import_static_scaled(fbx, dest, name, scale, collision):
    ui = ci.static_options(collision)
    ui.get_editor_property("static_mesh_import_data").set_editor_property("import_uniform_scale", scale)
    task = ci._task(fbx, dest, name, ui)
    tools.import_asset_tasks([task])
    return eal.load_asset(dest + "/" + name)


# the reward key glows a little so it reads on the rug
gk_path = MATS + "/MI_GoldenKey"
if eal.does_asset_exist(gk_path):
    eal.delete_asset(gk_path)
mi_gold = tools.create_asset("MI_GoldenKey", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi_gold.set_editor_property("parent", mat)
mel.set_material_instance_scalar_parameter_value(mi_gold, "Glow", 0.9)
eal.save_loaded_asset(mi_gold)

for name, coll in [("SM_Giant_Key", False), ("SM_Giant_Cymbal", False), ("SM_GoldenKey", True)]:
    m = import_static_scaled(os.path.join(sk_dir, name + ".fbx"), GIANT, name, 1.0, coll)
    for i in range(len(m.get_editor_property("static_materials"))):
        m.set_material(i, mi_gold if name == "SM_GoldenKey" else mi)
    eal.save_loaded_asset(m)
    print("static", name, m.get_bounds().box_extent)

# ------------------------------------------------------------------ toy room (meters -> x100) + new north wall
for name in ["SM_Wall_North", "SM_Toy_Floor", "SM_Toy_Walls", "SM_Toy_Ceiling"]:
    ci.import_static(os.path.join(EXP, "Arch", name + ".fbx"), "/Game/Crank/Environment", name, collision=False)
    m = eal.load_asset("/Game/Crank/Environment/" + name)
    ci.assign_slot_materials(m)
    ci.exact_collision(m)
    eal.save_loaded_asset(m)
for name, mode in [("SM_Toy_Rug", "none"), ("SM_Toy_Chest", "complex"), ("SM_Toy_Ball", "complex"), ("SM_Toy_Books", "complex"),
                   ("SM_Toy_BlockSmall", "complex"), ("SM_Toy_BlockBig", "complex")]:
    ci.import_static(os.path.join(EXP, "Props", name + ".fbx"), "/Game/Crank/Props", name, collision=False)
    m = eal.load_asset("/Game/Crank/Props/" + name)
    ci.assign_slot_materials(m)
    if mode == "complex":
        ci.exact_collision(m)
    else:
        m.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    eal.save_loaded_asset(m)
print("toy room ok")

# ------------------------------------------------------------------ sounds
SRC = r"D:\Game\CRANK\ArtSource\Audio\wav"
NEW = ["SFX_CymbalCrash", "SFX_GiantStomp", "SFX_GiantWindDown", "SFX_GiantRewind", "SFX_KeyHit", "SFX_Deflect",
       "SFX_MonkeyScreech", "SFX_CymbalWhoosh", "SFX_CymbalSpin", "SFX_GiantFall", "SFX_GiantTick"]
tasks = []
for name in NEW:
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(SRC, name + ".wav"))
    t.set_editor_property("destination_path", "/Game/Crank/Audio")
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tasks.append(t)
tools.import_asset_tasks(tasks)
for name in ("SFX_CymbalSpin", "SFX_GiantTick"):
    s = eal.load_asset("/Game/Crank/Audio/" + name)
    s.set_editor_property("looping", True)
    eal.save_loaded_asset(s)
print("sounds ok")
